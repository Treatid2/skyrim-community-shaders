#include "Features/ScreenshotApiPolicy.h"
#include "Features/ScreenshotManifestSnapshot.h"
#include "Features/ScreenshotWorkerThread.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace logger
{
	template <class... T>
	void warn(T&&...)
	{}
	template <class... T>
	void error(T&&...)
	{}
}
void LogScreenshotApiErrorNoexcept(const char*, const char* = nullptr) noexcept {}
using json = nlohmann::json;
namespace CSX::Api
{
	struct ServiceFoundation
	{
		static std::string TimestampUtc() { return "fixture-utc"; }
		std::string SessionId() const { return "fixture-session"; }
	};
}
namespace BuildProvenance
{
	json GetProducer() { return json::object(); }
}
std::string PathUtf8(const std::filesystem::path& path) { return path.string(); }

namespace CSX::ScreenshotStorage
{
	struct DirectoryLease
	{
		std::filesystem::path destination;
		std::filesystem::path directory;
		static std::shared_ptr<DirectoryLease> CreateExclusive(
			const std::filesystem::path& path, std::string_view, const std::filesystem::path& root)
		{
			if (root != "fixture-approved-root")
				throw std::runtime_error("preparation did not forward its approved root");
			return std::make_shared<DirectoryLease>(DirectoryLease{ path / "actual-parent", path / "actual-parent" / "fixture-sequence" });
		}
		const std::filesystem::path& Destination() const { return destination; }
		const std::filesystem::path& Path() const { return directory; }
		void VerifyDirectChild(const std::filesystem::path&) const {}
	};
}

struct BlockingIo
{
	std::mutex mutex;
	std::condition_variable condition;
	bool entered = false, released = false;
	void Wait()
	{
		std::unique_lock lock(mutex);
		entered = true;
		condition.notify_all();
		condition.wait(lock, [&] { return released; });
	}
	void AwaitEntry()
	{
		std::unique_lock lock(mutex);
		if (!condition.wait_for(lock, std::chrono::seconds(1), [&] { return entered; }))
			throw std::runtime_error("production worker did not enter controlled I/O");
	}
	void Release()
	{
		{
			std::lock_guard lock(mutex);
			released = true;
		}
		condition.notify_all();
	}
	void Reset()
	{
		std::lock_guard lock(mutex);
		entered = released = false;
	}
};
BlockingIo preparationIo, manifestIo;
std::atomic<unsigned> preparationCalls = 0;
void WriteJsonAtomically(const std::filesystem::path& path, const json&)
{
	if (path == "blocked-manifest")
		manifestIo.Wait();
	if (path == "denied-manifest")
		throw std::runtime_error("controlled manifest failure");
}
json DescribeCommittedArtifact(const std::filesystem::path&) { return { { "committed", true } }; }

// Types, worker entry points and publication retries are extracted from the
// production coordinator; only filesystem and journal boundaries are controlled.
struct ScreenshotApi
{
	using json = nlohmann::json;
#include "screenshot_worker_types_under_test.h"
	std::atomic<bool> failTransition = false, failEvent = false;
	explicit ScreenshotApi(std::shared_ptr<CSX::Api::ServiceFoundation>);
	~ScreenshotApi();
	bool CanAdmitPreparationLocked() const;
	static void PreparationWorkerLoop(std::shared_ptr<PreparationWorkerState>);
	static void ManifestWorkerLoop(std::shared_ptr<ManifestWorkerState>);
	void ManifestResultLoop(std::stop_token);
	void DrainPreparationResultsLocked();
	bool DrainManifestResultsLocked();
	void QueueSequenceManifestLocked(SequenceRecord&, bool);
	void TryFinalizeSequenceLocked(SequenceRecord&);
	void FinalizeSequenceLocked(SequenceRecord&, const ManifestResult*);
	void CancelQueuedPreparationLocked(std::string_view);
	void MarkSequenceCancellationLocked(SequenceRecord&);
	void OnFeatureDisabled(std::string_view);
	void BeginShutdown(std::string_view);
	bool DrainForShutdown(std::chrono::milliseconds);
	static bool IsTerminal(std::string_view state)
	{
		return state == "completed" || state == "cancelled" || state == "failed" || state == "failed_partial";
	}
	std::string SequenceTerminalOutcomeLocked(const SequenceRecord& sequence)
	{
		return sequence.cancelRequested ? "cancelled" : sequence.abortRequested ? "failed" :
		                                                                          "completed";
	}
	void TransitionLocked(RequestRecord& record, std::string state, std::string_view, json = json::object())
	{
		if (failTransition.exchange(false))
			throw std::runtime_error("controlled transition publication failure");
		record.state = std::move(state);
	}
	void AppendEventLocked(RequestRecord&, std::string_view, json = json::object())
	{
		if (failEvent.exchange(false))
			throw std::runtime_error("controlled event publication failure");
	}
	void TrimLocked() {}
	void CancelQueuedDispatchesLocked(std::string_view, std::string_view) {}
	void DispatchDeadlineLoop(std::stop_token token)
	{
		std::unique_lock lock(mutex);
		dispatchDeadlineCondition.wait(lock, token, [] { return false; });
	}
	static std::filesystem::path ResolveDestinationDirectory(const std::filesystem::path& path, const json&, bool, std::filesystem::path* root)
	{
		*root = "fixture-approved-root";
		++preparationCalls;
		if (path == "blocked-preparation")
			preparationIo.Wait();
		if (path == "denied-preparation")
			throw std::runtime_error("controlled preparation failure");
		return path;
	}
	void StopResultService()
	{
		manifestResultDrainer.request_stop();
		manifestWorkerState->condition.notify_all();
		manifestResultDrainer.join();
	}
	void AdmitPreparation(std::string id, const std::filesystem::path& path, bool manifest = false)
	{
		std::lock_guard lock(mutex);
		auto& parent = requests[id];
		parent.requestId = id;
		parent.state = "preparing";
		auto& sequence = sequences[id];
		sequence.requestId = id;
		sequence.frameCount = 2;
		sequence.frameManifest = manifest;
		sequence.preparationPending = true;
		sequence.packaging = { { "previewVideo", { { "state", "not_requested" } } } };
		{
			std::lock_guard workerLock(preparationWorkerState->mutex);
			preparationWorkerState->jobs.push_back({ .requestId = id, .configuredDirectory = path });
			++preparationWorkerState->outstanding;
		}
		preparationWorkerState->condition.notify_all();
	}
	void AdmitManifest(std::string id, const std::filesystem::path& path)
	{
		std::lock_guard lock(mutex);
		requests[id].requestId = id;
		auto& sequence = sequences[id];
		sequence.requestId = id;
		sequence.frameManifest = true;
		sequence.finalizing = true;
		sequence.finalTerminalOutcome = "completed";
		sequence.finalManifestGeneration = sequence.manifestGeneration = 1;
		{
			std::lock_guard workerLock(manifestWorkerState->mutex);
			manifestWorkerState->jobs.push_back({
				.job = { .requestId = id, .generation = 1, .final = true, .destination = path, .directoryLease = std::make_shared<CSX::ScreenshotStorage::DirectoryLease>() },
				.result = { .requestId = id, .generation = 1, .final = true, .destination = path },
			});
			++manifestWorkerState->outstanding;
		}
		manifestWorkerState->condition.notify_all();
	}
};
#include "screenshot_worker_lanes_under_test.h"

void Require(bool condition, const char* message)
{
	if (!condition)
		throw std::runtime_error(message);
}
template <class Predicate>
void Await(Predicate predicate)
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (!predicate()) {
		if (std::chrono::steady_clock::now() >= deadline)
			throw std::runtime_error("production result service did not make bounded progress");
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}
std::unique_ptr<ScreenshotApi> CreateApi()
{
	return std::make_unique<ScreenshotApi>(std::make_shared<CSX::Api::ServiceFoundation>());
}

int main()
{
	{
		auto api = CreateApi();
		api->AdmitManifest("blocked", "blocked-manifest");
		manifestIo.AwaitEntry();
		api->AdmitPreparation("independent", "ready");
		Await([&] { std::lock_guard lock(api->mutex); return !api->sequences.at("independent").preparationPending; });
		{
			std::lock_guard lock(api->mutex);
			const auto& prepared = api->sequences.at("independent");
			Require(prepared.directoryLease && prepared.directory == std::filesystem::path("ready/actual-parent/fixture-sequence") &&
						prepared.capture["destination"]["resolvedDirectory"] == PathUtf8(prepared.directoryLease->Destination()),
				"preparation receipt did not bind the retained parent and sequence lease");
		}
		Require(!api->DrainForShutdown(std::chrono::milliseconds(20)), "blocked manifest reported drained");
		manifestIo.Release();
		Require(api->DrainForShutdown(std::chrono::seconds(2)), "independent lanes did not drain");
	}
	manifestIo.Reset();
	{
		auto api = CreateApi();
		api->AdmitPreparation("blocked", "blocked-preparation");
		preparationIo.AwaitEntry();
		api->AdmitManifest("independent", "ready-manifest");
		Await([&] { std::lock_guard lock(api->mutex); return api->requests.at("independent").state == "completed"; });
		api->AdmitPreparation("cancelled", "must-not-resolve");
		{
			std::lock_guard lock(api->mutex);
			api->MarkSequenceCancellationLocked(api->sequences.at("cancelled"));
		}
		const auto calls = preparationCalls.load();
		preparationIo.Release();
		Require(api->DrainForShutdown(std::chrono::seconds(2)), "preparation cancellation did not drain");
		Require(preparationCalls == calls, "queued cancellation entered filesystem resolution");
		std::lock_guard lock(api->mutex);
		Require(api->requests.at("cancelled").state == "cancelled", "queued cancellation lost terminal outcome");
	}
	preparationIo.Reset();
	{
		auto api = CreateApi();
		api->AdmitPreparation("denied", "denied-preparation", true);
		Require(api->DrainForShutdown(std::chrono::seconds(2)), "failed preparation retained admission");
		std::lock_guard lock(api->mutex);
		Require(api->requests.at("denied").state == "failed" &&
					api->sequences.at("denied").packaging["frameManifest"]["path"].is_null(),
			"failed preparation left unresolved packaging");
	}
	{
		auto api = CreateApi();
		api->failTransition = true;
		const auto calls = preparationCalls.load();
		api->AdmitPreparation("retry", "ready");
		Require(api->DrainForShutdown(std::chrono::seconds(2)), "preparation publication retry did not recover");
		Require(preparationCalls == calls + 1, "publication retry repeated preparation I/O");
		api->failEvent = true;
		api->AdmitManifest("manifest-retry", "ready-manifest");
		Require(api->DrainForShutdown(std::chrono::seconds(2)), "manifest publication retry did not recover");
		std::lock_guard lock(api->mutex);
		Require(api->requests.at("manifest-retry").state == "completed", "manifest retry lost outcome");
		Require(api->requests.at("manifest-retry").warnings.size() == 1, "manifest retry provenance lost");
	}
	{
		auto api = CreateApi();
		api->StopResultService();
		api->AdmitPreparation("ready-cancel", "ready", true);
		Await([&] { std::lock_guard lock(api->preparationWorkerState->mutex); return !api->preparationWorkerState->results.empty(); });
		api->BeginShutdown("controlled shutdown");
		Await([&] { std::lock_guard lock(api->manifestWorkerState->mutex); return !api->manifestWorkerState->results.empty(); });
		{
			std::lock_guard lock(api->mutex);
			Require(api->sequences.at("ready-cancel").manifestGeneration == 1, "shutdown queued redundant partial manifest");
			api->DrainManifestResultsLocked();
			Require(api->requests.at("ready-cancel").state == "cancelled", "ready preparation beat shutdown cancellation");
		}
		Require(api->DrainForShutdown(std::chrono::milliseconds(20)), "manual result application did not release capacity");
	}
	{
		auto api = CreateApi();
		api->StopResultService();
		api->AdmitManifest("undrained", "ready-manifest");
		Await([&] { std::lock_guard lock(api->manifestWorkerState->mutex); return !api->manifestWorkerState->results.empty(); });
		{
			std::lock_guard lock(api->manifestWorkerState->mutex);
			Require(api->manifestWorkerState->outstanding == 1, "undrained result escaped capacity accounting");
		}
		{
			std::lock_guard lock(api->preparationWorkerState->mutex);
			api->preparationWorkerState->outstanding = CSX::ScreenshotPolicy::MaximumPreparationJobs;
		}
		Require(!api->CanAdmitPreparationLocked(), "preparation admission ignored capacity");
		{
			std::lock_guard lock(api->preparationWorkerState->mutex);
			api->preparationWorkerState->outstanding = 0;
		}
		Require(api->CanAdmitPreparationLocked(), "preparation capacity did not reopen");
		{
			std::lock_guard lock(api->mutex);
			api->DrainManifestResultsLocked();
			api->manifestWorkerState->outstanding = CSX::ScreenshotPolicy::MaximumPartialManifestJobs;
			auto& sequence = api->sequences.at("undrained");
			api->QueueSequenceManifestLocked(sequence, false);
			Require(sequence.manifestGeneration == 1, "partial checkpoint consumed final reserve");
			api->manifestWorkerState->outstanding = 0;
		}
	}
	{
		auto api = CreateApi();
		api->AdmitPreparation("blocked", "blocked-preparation");
		api->AdmitManifest("blocked", "blocked-manifest");
		preparationIo.AwaitEntry();
		manifestIo.AwaitEntry();
		std::weak_ptr preparation = api->preparationWorkerState;
		std::weak_ptr manifest = api->manifestWorkerState;
		const auto start = std::chrono::steady_clock::now();
		api.reset();
		const auto elapsed = std::chrono::steady_clock::now() - start;
		Require(elapsed >= std::chrono::milliseconds(1900) && elapsed < std::chrono::milliseconds(2800), "destructor did not share one I/O deadline");
		Require(!preparation.expired() && !manifest.expired(), "blocked worker lost isolated state");
		preparationIo.Release();
		manifestIo.Release();
		Await([&] { return preparation.expired() && manifest.expired(); });
	}
	return 0;
}
