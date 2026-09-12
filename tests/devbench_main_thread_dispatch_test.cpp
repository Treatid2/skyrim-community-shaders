#include "Api/DevBenchMainThreadDispatch.h"

#include <atomic>
#include <future>
#include <latch>
#include <stdexcept>
#include <thread>

namespace
{
	using json = nlohmann::json;
	using Format = CSX::Api::DevBenchDispatchErrorFormat;
	using namespace std::chrono_literals;
	std::atomic_int entries = 0;
	bool failEntry = false;

	struct Queue
	{
		enum class Mode
		{
			inlineTask,
			deferred,
			rejected,
			retainedThenThrow,
			inlineThenThrow
		};
		Mode mode = Mode::inlineTask;
		std::function<void()> retained;

		void AddTask(std::function<void()> a_task)
		{
			if (mode == Mode::rejected)
				throw std::runtime_error("submission failed");
			if (mode == Mode::deferred || mode == Mode::retainedThenThrow)
				retained = std::move(a_task);
			else
				a_task();
			if (mode == Mode::retainedThenThrow || mode == Mode::inlineThenThrow)
				throw std::runtime_error("submission failed after accepting task");
		}
	};

	void Require(bool a_condition, const char* a_message)
	{
		if (!a_condition)
			throw std::runtime_error(a_message);
	}

	struct ExpectedResponses
	{
		Format format;
		const char* unavailable;
		const char* timeout;
		const char* rejected;
		const char* failed;
		const char* unknown;
		const char* binding;
	};

	// Literal snapshots of each existing bridge response contract.
	constexpr ExpectedResponses kResponses[] = {
		{ Format::standard,
			R"({"error":"SKSE task interface unavailable"})",
			R"({"error":"main thread did not run within 0ms"})",
			R"({"error":"SKSE task queue rejected the main-thread task"})",
			R"({"error":"main-thread task failed","detail":"task failed"})",
			R"({"error":"main-thread task failed"})",
			R"({"error":"main-thread task failed","detail":"binding failed"})" },
		{ Format::profiler,
			R"({"_dispatchError":"SKSE task interface unavailable"})",
			R"({"_dispatchError":"main thread did not run within 0ms"})",
			R"({"_dispatchError":"SKSE task queue rejected the main-thread task"})",
			R"({"_dispatchError":"task failed"})",
			R"({"_dispatchError":"unknown main-thread failure"})",
			R"({"_dispatchError":"binding failed"})" },
		{ Format::screenshot,
			R"({"ok":false,"error":{"admitted":false,"code":"dispatcher_unavailable","message":"SKSE task interface unavailable","phase":"admission","retryable":true}})",
			R"({"ok":false,"error":{"admitted":false,"code":"dispatcher_timeout","message":"main thread did not run within 0ms","phase":"admission","retryable":true}})",
			R"({"ok":false,"error":{"admitted":false,"code":"dispatcher_failed","message":"SKSE task queue rejected the main-thread task","phase":"admission","retryable":true}})",
			R"({"ok":false,"error":{"admitted":true,"code":"dispatcher_failed","message":"task failed","phase":"execution","retryable":false}})",
			R"({"ok":false,"error":{"admitted":true,"code":"dispatcher_failed","message":"unknown main-thread failure","phase":"execution","retryable":false}})",
			R"({"ok":false,"error":{"admitted":true,"code":"dispatcher_failed","message":"binding failed","phase":"execution","retryable":false}})" },
	};
}

// Runtime binding is part of the admitted task and must share its exception guard.
void CSX::Api::EnterRuntimeMainThreadTask()
{
	++entries;
	if (failEntry)
		throw std::runtime_error("binding failed");
}

int main()
{
	for (const auto& expected : kResponses) {
		const auto format = expected.format;
		using CSX::Api::RunDevBenchMainThreadTask;
		Queue queue;
		int mutations = 0;
		auto run = [&]() -> json {
			++mutations;
			return { { "mutation", mutations } };
		};
		const auto unavailable = RunDevBenchMainThreadTask<Queue>(nullptr, run, format);
		Require(unavailable.response == json::parse(expected.unavailable) &&
					unavailable.failure && !unavailable.failure->admitted && unavailable.failure->retryable,
			"unavailable dispatcher changed its wire response");
		for (auto mode : { Queue::Mode::deferred, Queue::Mode::rejected, Queue::Mode::retainedThenThrow }) {
			queue.mode = mode;
			const auto entriesBefore = entries.load();
			const auto result = RunDevBenchMainThreadTask(&queue, run, format, 0ms);
			const bool timeout = mode == Queue::Mode::deferred;
			Require(result.response == json::parse(timeout ? expected.timeout : expected.rejected) &&
						result.failure && !result.failure->admitted && result.failure->retryable &&
						result.failure->phase == "admission",
				"unadmitted task changed its wire response");
			if (queue.retained) {
				queue.retained();
				queue.retained = {};
			}
			Require(mutations == 0 && entries == entriesBefore, "terminal dispatch failure allowed a late mutation or binding");
		}
		queue.mode = Queue::Mode::inlineThenThrow;
		const auto completed = RunDevBenchMainThreadTask(&queue, run, format);
		Require(completed.response == json{ { "mutation", 1 } } && !completed.failure,
			"submission failure hid a completed task result");
		queue.mode = Queue::Mode::inlineTask;
		const auto failed = RunDevBenchMainThreadTask(&queue, []() -> json { throw std::runtime_error("task failed"); }, format);
		Require(failed.response == json::parse(expected.failed) && failed.failure && failed.failure->admitted &&
					!failed.failure->retryable && failed.failure->phase == "execution",
			"task exception changed its execution outcome");
		const auto unknown = RunDevBenchMainThreadTask(&queue, []() -> json { throw 1; }, format);
		Require(unknown.response == json::parse(expected.unknown) && unknown.failure && unknown.failure->admitted &&
					!unknown.failure->retryable && unknown.failure->phase == "execution",
			"unknown task exception changed its execution outcome");
		failEntry = true;
		const auto entryFailure = RunDevBenchMainThreadTask(&queue, run, format);
		failEntry = false;
		Require(entryFailure.response == json::parse(expected.binding) && entryFailure.failure &&
					entryFailure.failure->admitted && !entryFailure.failure->retryable && mutations == 1,
			"runtime binding failure escaped the task exception guard");
	}
	{
		// Submission throws while an admitted task is blocked. Its result, rather
		// than rejection, must be returned even when the admission deadline expires.
		std::latch admitted{ 1 };
		std::latch release{ 1 };
		std::jthread worker;
		auto caller = std::async(std::launch::async, [&] {
			return CSX::Api::DispatchMainThreadTask(
				[&](auto a_task) {
					worker = std::jthread(std::move(a_task));
					admitted.wait();
					throw std::runtime_error("submission failed after admission");
				},
				[&] {
					admitted.count_down();
					release.wait();
					return 42;
				},
				0ms);
		});
		admitted.wait();
		Require(caller.wait_for(20ms) == std::future_status::timeout,
			"caller returned before admitted work published its result");
		release.count_down();
		Require(caller.get() == 42, "admitted task lost its exact result after submission failure");
	}
}
