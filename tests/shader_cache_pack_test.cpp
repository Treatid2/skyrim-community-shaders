#include "Utils/ShaderCachePack.h"

#include "Utils/CryptoHash.h"

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <nlohmann/json.hpp>
#include <thread>

#ifdef _WIN32
#	include <Windows.h>
#endif

using namespace Util::ShaderCachePack;

namespace
{
	PackSetId TestPackSetId()
	{
		PackSetId result{};
		result.front() = std::byte{ 0x53 };
		return result;
	}

	PackSetId ParsePackSetId(std::string_view a_value)
	{
		assert(a_value.size() == 32);
		PackSetId result{};
		for (std::size_t index = 0; index < result.size(); ++index) {
			auto nibble = [](char a_character) -> std::uint8_t {
				if (a_character >= '0' && a_character <= '9')
					return static_cast<std::uint8_t>(a_character - '0');
				assert(a_character >= 'a' && a_character <= 'f');
				return static_cast<std::uint8_t>(a_character - 'a' + 10);
			};
			result[index] = static_cast<std::byte>((nibble(a_value[index * 2]) << 4) | nibble(a_value[index * 2 + 1]));
		}
		return result;
	}

	nlohmann::json MakePackManifest()
	{
		return {
			{ "schema", "csx.shader-cache.pack-manifest" },
			{ "schemaVersion", 2 },
			{ "formatVersion", 1 },
			{ "hashAlgorithm", "sha256" },
			{ "packSetId", "53000000000000000000000000000000" },
			{ "runtime", "VR" },
			{ "shaderCacheABI", "test-abi" },
			{ "optimizedRecordCount", 3 },
			{ "developerRecordCount", 0 },
			{ "compatibilityVariants", nlohmann::json::array({ "default" }) },
			{ "files",
				{
					{ "Optimized.A.csxpack", { { "lane", 1 }, { "generation", 1 }, { "recordCount", 3 } } },
					{ "Optimized.B.csxpack", { { "lane", 1 }, { "generation", 0 }, { "recordCount", 0 } } },
					{ "Developer.A.csxpack", { { "lane", 2 }, { "generation", 1 }, { "recordCount", 0 } } },
					{ "Developer.B.csxpack", { { "lane", 2 }, { "generation", 0 }, { "recordCount", 0 } } },
				} },
		};
	}

#ifdef _WIN32
	int RunLeaseProbe(
		const std::filesystem::path& a_executable,
		const std::filesystem::path& a_first,
		const std::filesystem::path& a_second,
		bool a_expectOpen)
	{
		auto quote = [](const std::filesystem::path& a_value) {
			return L"\"" + a_value.wstring() + L"\"";
		};
		std::wstring command =
			quote(a_executable) + L" --lease-probe " + quote(a_first) + L" " + quote(a_second) +
			(a_expectOpen ? L" open" : L" locked");
		STARTUPINFOW startup{};
		startup.cb = sizeof(startup);
		PROCESS_INFORMATION process{};
		assert(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process));
		assert(WaitForSingleObject(process.hProcess, 30000) == WAIT_OBJECT_0);
		DWORD exitCode = 1;
		assert(GetExitCodeProcess(process.hProcess, &exitCode));
		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);
		return static_cast<int>(exitCode);
	}

	void TerminateLeaseHolder(
		const std::filesystem::path& a_executable,
		const std::filesystem::path& a_first,
		const std::filesystem::path& a_second,
		const std::filesystem::path& a_ready)
	{
		auto quote = [](const std::filesystem::path& a_value) {
			return L"\"" + a_value.wstring() + L"\"";
		};
		std::wstring command =
			quote(a_executable) + L" --lease-hold " + quote(a_first) + L" " + quote(a_second) + L" " + quote(a_ready);
		STARTUPINFOW startup{};
		startup.cb = sizeof(startup);
		PROCESS_INFORMATION process{};
		assert(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process));
		for (unsigned attempt = 0; attempt < 500 && !std::filesystem::exists(a_ready); ++attempt)
			Sleep(10);
		assert(std::filesystem::exists(a_ready));
		assert(TerminateProcess(process.hProcess, 91));
		assert(WaitForSingleObject(process.hProcess, 30000) == WAIT_OBJECT_0);
		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);
	}
#endif

	Entry MakeEntry(std::string a_logical, std::string a_exact, std::uint8_t a_value, std::size_t a_size = 64)
	{
		Entry entry{ std::move(a_logical), std::move(a_exact), "{\"schema\":1}", {} };
		entry.bytecode.assign(a_size, static_cast<std::byte>(a_value));
		return entry;
	}

	std::filesystem::path ActivePack(const std::filesystem::path& a_first, const std::filesystem::path& a_second)
	{
		return std::filesystem::file_size(a_first) >= std::filesystem::file_size(a_second) ? a_first : a_second;
	}

	template <class T>
	void Overwrite(const std::filesystem::path& a_path, std::uint64_t a_offset, const T& a_value)
	{
		std::fstream stream(a_path, std::ios::binary | std::ios::in | std::ios::out);
		assert(stream);
		stream.seekp(static_cast<std::streamoff>(a_offset));
		stream.write(reinterpret_cast<const char*>(std::addressof(a_value)), sizeof(a_value));
		stream.flush();
		assert(stream);
	}

	void SetPackGeneration(const std::filesystem::path& a_path, std::uint64_t a_generation)
	{
		std::array<std::byte, 80> header{};
		std::fstream stream(a_path, std::ios::binary | std::ios::in | std::ios::out);
		assert(stream.read(reinterpret_cast<char*>(header.data()), header.size()));
		std::memcpy(header.data() + 16, &a_generation, sizeof(a_generation));
		const auto hash = Util::CryptoHash::Sha256Bytes(std::span(header.data(), 48));
		std::memcpy(header.data() + 48, hash.data(), hash.size());
		stream.seekp(0);
		stream.write(reinterpret_cast<const char*>(header.data()), header.size());
		stream.flush();
		assert(stream);
	}
}

int main(int argc, char** argv)
{
	static_assert(!ShouldReadLooseBlob(false, false));
	static_assert(!ShouldReadLooseBlob(false, true));
	static_assert(ShouldReadLooseBlob(true, false));
	static_assert(!ShouldReadLooseBlob(true, true));
	static_assert(ClassifyLayoutMembers({ false, false, false, false, false }) == LayoutState::Absent);
	static_assert(ClassifyLayoutMembers({ true, true, true, true, true }) == LayoutState::Complete);

	for (std::uint32_t mask = 0; mask < 32; ++mask) {
		std::array<bool, 5> present{};
		for (std::size_t index = 0; index < present.size(); ++index)
			present[index] = (mask & (1u << index)) != 0;
		const auto expected =
			mask == 0  ? LayoutState::Absent :
			mask == 31 ? LayoutState::Complete :
						 LayoutState::PartialOrInvalid;
		assert(ClassifyLayoutMembers(present) == expected);
	}

	if (argc == 5 && std::string_view(argv[1]) == "--lease-probe") {
		std::string error;
		Store external(argv[2], argv[3], Lane::Optimized, TestPackSetId());
		const bool opened = external.Open(&error);
		const bool expected = std::string_view(argv[4]) == "open";
		return opened == expected ? 0 : 1;
	}
#ifdef _WIN32
	if (argc == 5 && std::string_view(argv[1]) == "--lease-hold") {
		std::string error;
		Store external(argv[2], argv[3], Lane::Optimized, TestPackSetId());
		if (!external.Open(&error))
			return 1;
		std::ofstream(argv[4]).put('1');
		Sleep(30000);
		return 2;
	}
#endif
	if (argc == 5) {
		std::string error;
		Store external(argv[1], argv[2], Lane::Optimized, ParsePackSetId(argv[4]));
		assert(external.Open(&error));
		const auto record = external.Find(argv[3], &error);
		assert(record && !record->bytecode.empty());
		return 0;
	}
	assert(argc == 1);
	const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	const auto root = std::filesystem::temp_directory_path() / ("csx-pack-test-" + unique);
	std::filesystem::create_directories(root);

	// Runtime admission enforces the same strict manifest shape as packaging.
	{
		std::string manifestError;
		const auto valid = MakePackManifest();
		assert(ParseManifestContract(valid, "VR", "test-abi", &manifestError));
		auto expectRejected = [&](nlohmann::json manifest) {
			manifestError.clear();
			assert(!ParseManifestContract(manifest, "VR", "test-abi", &manifestError));
			assert(!manifestError.empty());
		};

		auto zeroIdentity = valid;
		zeroIdentity["packSetId"] = std::string(32, '0');
		expectRejected(std::move(zeroIdentity));
		auto missingHash = valid;
		missingHash.erase("hashAlgorithm");
		expectRejected(std::move(missingHash));
		auto wrongCountType = valid;
		wrongCountType["optimizedRecordCount"] = "3";
		expectRejected(std::move(wrongCountType));
		auto missingFile = valid;
		missingFile["files"].erase("Developer.B.csxpack");
		expectRejected(std::move(missingFile));
		auto wrongLane = valid;
		wrongLane["files"]["Developer.A.csxpack"]["lane"] = 1;
		expectRejected(std::move(wrongLane));
		auto ambiguousGeneration = valid;
		ambiguousGeneration["files"]["Optimized.B.csxpack"]["generation"] = 1;
		expectRejected(std::move(ambiguousGeneration));
		auto wrongAggregate = valid;
		wrongAggregate["optimizedRecordCount"] = 4;
		expectRejected(std::move(wrongAggregate));
		auto duplicateVariant = valid;
		duplicateVariant["compatibilityVariants"].push_back("default");
		expectRejected(std::move(duplicateVariant));
	}

	// The reserved zero identity is rejected before either fixed file is mutated.
	{
		const auto invalidRoot = root / "zero-identity";
		std::filesystem::create_directories(invalidRoot);
		const auto first = invalidRoot / "Optimized.A.csxpack";
		const auto second = invalidRoot / "Optimized.B.csxpack";
		std::ofstream(first, std::ios::binary).close();
		std::ofstream(second, std::ios::binary).close();
		std::string invalidError;
		Store invalid(first, second, Lane::Optimized, PackSetId{});
		assert(!invalid.Open(&invalidError));
		assert(!invalidError.empty());
		assert(std::filesystem::file_size(first) == 0);
		assert(std::filesystem::file_size(second) == 0);
	}

	// A live index must not trust payload bytes that changed after Open().
	{
		const auto mutationRoot = root / "mutation";
		std::filesystem::create_directories(mutationRoot);
		const auto first = mutationRoot / "Optimized.A.csxpack";
		const auto second = mutationRoot / "Optimized.B.csxpack";
		std::ofstream(first, std::ios::binary).close();
		std::ofstream(second, std::ios::binary).close();
		std::string mutationError;
		Store mutation(first, second, Lane::Optimized, TestPackSetId());
		assert(mutation.Open(&mutationError));
		assert(mutation.Append(MakeEntry("logical", "exact", 0x71), &mutationError));
		assert(mutation.Checkpoint(&mutationError));
		const char changed = 'X';
		Overwrite(ActivePack(first, second), 160, changed);
		mutationError.clear();
		assert(!mutation.Find("exact", &mutationError));
		assert(!mutationError.empty());
	}

	// Oversized record dimensions are contained as a corrupt tail instead of
	// wrapping record arithmetic or addressing beyond the mapped payload.
	{
		const auto overflowRoot = root / "overflow";
		std::filesystem::create_directories(overflowRoot);
		const auto first = overflowRoot / "Optimized.A.csxpack";
		const auto second = overflowRoot / "Optimized.B.csxpack";
		std::ofstream(first, std::ios::binary).close();
		std::ofstream(second, std::ios::binary).close();
		std::string overflowError;
		{
			Store writer(first, second, Lane::Optimized, TestPackSetId());
			assert(writer.Open(&overflowError));
			assert(writer.Append(MakeEntry("logical", "exact", 0x72), &overflowError));
			assert(writer.Checkpoint(&overflowError));
		}
		const auto maximum = (std::numeric_limits<std::uint32_t>::max)();
		Overwrite(ActivePack(first, second), 104, maximum);
		Store reader(first, second, Lane::Optimized, TestPackSetId());
		assert(reader.Open(&overflowError));
		assert(reader.GetStats().corruptTailBytes > 0);
		assert(!reader.Find("exact", &overflowError));
	}

	// A nonzero pack-set identity is also the cross-process writer lease key.
	{
		const auto leaseRoot = root / "lease";
		std::filesystem::create_directories(leaseRoot);
		const auto first = leaseRoot / "Optimized.A.csxpack";
		const auto second = leaseRoot / "Optimized.B.csxpack";
		std::ofstream(first, std::ios::binary).close();
		std::ofstream(second, std::ios::binary).close();
		const auto setID = TestPackSetId();
		std::string leaseError;
		Store owner(first, second, Lane::Optimized, setID);
		assert(owner.Open(&leaseError));
		Store contender(first, second, Lane::Optimized, setID);
		assert(!contender.Open(&leaseError));
		assert(!leaseError.empty());
		bool threadOpened = true;
		std::jthread contenderThread([&] {
			std::string threadError;
			Store threadContender(
				first.parent_path() / "." / first.filename(),
				second.parent_path() / "." / second.filename(),
				Lane::Optimized,
				setID);
			threadOpened = threadContender.Open(&threadError);
		});
		contenderThread.join();
		assert(!threadOpened);
#ifdef _WIN32
		assert(RunLeaseProbe(argv[0], first, second, false) == 0);
#endif
	}

	// Equal readable generations are ambiguous rather than an arbitrary A/B tie.
	{
		const auto ambiguousRoot = root / "ambiguous-generation";
		std::filesystem::create_directories(ambiguousRoot);
		const auto first = ambiguousRoot / "Optimized.A.csxpack";
		const auto second = ambiguousRoot / "Optimized.B.csxpack";
		std::ofstream(first, std::ios::binary).close();
		std::ofstream(second, std::ios::binary).close();
		std::string ambiguousError;
		{
			Store writer(first, second, Lane::Optimized, TestPackSetId());
			assert(writer.Open(&ambiguousError));
			assert(writer.Append(MakeEntry("logical", "exact", 0x73), &ambiguousError));
			assert(writer.Checkpoint(&ambiguousError));
			assert(writer.Compact(&ambiguousError));
		}
		SetPackGeneration(first, 2);
		Store reader(first, second, Lane::Optimized, TestPackSetId());
		assert(!reader.Open(&ambiguousError));
		assert(ambiguousError.find("ambiguous") != std::string::npos);
	}
#ifdef _WIN32
	{
		const auto leaseRoot = root / "lease";
		assert(RunLeaseProbe(
				   argv[0],
				   leaseRoot / "Optimized.A.csxpack",
				   leaseRoot / "Optimized.B.csxpack",
				   true) == 0);
	}
	{
		const auto abandonedRoot = root / "abandoned-lease";
		std::filesystem::create_directories(abandonedRoot);
		const auto first = abandonedRoot / "Optimized.A.csxpack";
		const auto second = abandonedRoot / "Optimized.B.csxpack";
		const auto ready = abandonedRoot / "ready";
		std::ofstream(first, std::ios::binary).close();
		std::ofstream(second, std::ios::binary).close();
		TerminateLeaseHolder(argv[0], first, second, ready);
		std::string leaseError;
		Store recoveredLease(first, second, Lane::Optimized, TestPackSetId());
		assert(recoveredLease.Open(&leaseError));
	}
#endif

	const auto a = root / "Optimized.A.csxpack";
	const auto b = root / "Optimized.B.csxpack";
	std::ofstream(a, std::ios::binary).close();
	std::ofstream(b, std::ios::binary).close();

	std::string error;
	{
		Store store(a, b, Lane::Optimized, TestPackSetId());
		assert(store.Open(&error));
		assert(store.Append(MakeEntry("water|provider=1", "water|source=old|provider=1", 0x11), &error));
		assert(store.Append(MakeEntry("water|provider=1", "water|source=new|provider=1", 0x22), &error));
		assert(store.Append(MakeEntry("water|provider=2", "water|source=new|provider=2", 0x33), &error));
		assert(store.Checkpoint(&error));

		auto old = store.Find("water|source=old|provider=1", &error);
		auto current = store.Find("water|source=new|provider=1", &error);
		assert(old && current);
		assert(old->bytecode.front() == std::byte{ 0x11 });
		assert(current->bytecode.front() == std::byte{ 0x22 });
		const auto before = store.GetStats();
		assert(before.recordCount == 3 && before.liveRecordCount == 2 && before.supersededBytes > 0);
	}

	// A committed prefix remains readable when a process dies during the next append.
	{
		std::ofstream tail(a, std::ios::binary | std::ios::app);
		const char incomplete[] = "CSXREC1";
		tail.write(incomplete, sizeof(incomplete));
	}
	Store recovered(a, b, Lane::Optimized, TestPackSetId());
	error.clear();
	assert(recovered.Open(&error));
	assert(!error.empty());
	assert(recovered.Find("water|source=new|provider=1", &error));
	assert(recovered.GetStats().corruptTailBytes > 0);

	// Compaction retains the latest exact record per logical/compatibility key in
	// the new generation while the previous generation remains searchable.
	assert(recovered.Compact(&error));
	const auto compacted = recovered.GetStats();
	assert(compacted.activeGeneration == 2);
	assert(compacted.recordCount == 2);
	assert(compacted.liveRecordCount == 2);
	assert(compacted.supersededBytes == 0);
	assert(compacted.Fragmentation() == 0.0);
	assert(recovered.Find("water|source=new|provider=1", &error));
	assert(recovered.Find("water|source=new|provider=2", &error));
	assert(recovered.Find("water|source=old|provider=1", &error));

	// Exercise the opposite A/B role: B is active after the first compaction,
	// and the next compaction must safely initialize and promote A.
	assert(recovered.Append(MakeEntry("water|provider=1", "water|source=newest|provider=1", 0x44), &error));
	assert(recovered.Checkpoint(&error));
	assert(recovered.Compact(&error));
	assert(recovered.GetStats().activeGeneration == 3);
	assert(recovered.Find("water|source=newest|provider=1", &error));

	// Reset first commits a new generation barrier, so a failed cleanup cannot
	// make any record from the prior generation visible again.
	assert(recovered.Reset(&error) == ResetDisposition::Complete);
	assert(recovered.GetStats().activeGeneration == 5);
	assert(!recovered.Find("water|source=newest|provider=1", &error));
	assert(recovered.Append(MakeEntry("water|provider=1", "water|source=reset|provider=1", 0x55), &error));
	assert(recovered.Checkpoint(&error));
	assert(recovered.Find("water|source=reset|provider=1", &error));

#ifdef _WIN32
	// Cleanup failure after the empty generation barrier is a committed but
	// explicitly degraded reset, not an unqualified success.
	{
		const auto degradedRoot = root / "degraded-reset";
		std::filesystem::create_directories(degradedRoot);
		const auto first = degradedRoot / "Optimized.A.csxpack";
		const auto second = degradedRoot / "Optimized.B.csxpack";
		std::ofstream(first, std::ios::binary).close();
		std::ofstream(second, std::ios::binary).close();
		std::string resetError;
		{
			Store degraded(first, second, Lane::Optimized, TestPackSetId());
			assert(degraded.Open(&resetError));
			assert(degraded.Append(MakeEntry("reset", "reset-exact", 0x60), &resetError));
			assert(degraded.Checkpoint(&resetError));

			const HANDLE cleanupBlocker = CreateFileW(
				first.c_str(),
				GENERIC_READ,
				FILE_SHARE_READ,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				nullptr);
			assert(cleanupBlocker != INVALID_HANDLE_VALUE);
			resetError.clear();
			assert(degraded.Reset(&resetError) == ResetDisposition::CommittedDegraded);
			assert(!resetError.empty());
			assert(degraded.GetStats().available);
			assert(!degraded.Find("reset-exact", &resetError));
			CloseHandle(cleanupBlocker);
		}

		resetError.clear();
		Store restarted(first, second, Lane::Optimized, TestPackSetId());
		assert(restarted.Open(&resetError));
		assert(resetError.find("generation gap") != std::string::npos);
		assert(!restarted.Find("reset-exact", &resetError));
	}
#endif

	// A vanished backing file is an ordinary cache-lane failure, not an
	// exception escaping through shader compilation.
	std::filesystem::remove(ActivePack(a, b));
	error.clear();
	assert(!recovered.Find("water|source=reset|provider=1", &error));
	assert(!error.empty());
	error.clear();
	assert(!recovered.Compact(&error));
	assert(!error.empty());

	// One failed lane must not disable an independent healthy lane.
	const auto laneRoot = root / "lane-isolation";
	std::filesystem::create_directories(laneRoot);
	const auto optimizedA = laneRoot / "Optimized.A.csxpack";
	const auto optimizedB = laneRoot / "Optimized.B.csxpack";
	const auto developerA = laneRoot / "Developer.A.csxpack";
	const auto developerB = laneRoot / "Developer.B.csxpack";
	std::ofstream(optimizedA, std::ios::binary).close();
	std::ofstream(optimizedB, std::ios::binary).close();
	std::ofstream(developerA, std::ios::binary).close();
	Store healthyLane(optimizedA, optimizedB, Lane::Optimized, TestPackSetId());
	Store incompleteLane(developerA, developerB, Lane::Developer, TestPackSetId());
	error.clear();
	assert(healthyLane.Open(&error));
	error.clear();
	assert(!incompleteLane.Open(&error));
	assert(!error.empty());
	assert(healthyLane.Append(MakeEntry("healthy", "healthy-exact", 0x61), &error));
	assert(healthyLane.Checkpoint(&error));
	assert(healthyLane.Find("healthy-exact", &error));

	std::filesystem::remove_all(root);
	return 0;
}
