#include "Utils/ShaderSourceProvenance.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
	struct SourceFixture
	{
		std::filesystem::path directory = std::filesystem::temp_directory_path() /
		                                  ("csx-source-provenance-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::path root = directory / "Water.hlsl";
		std::filesystem::path include = directory / "Common.hlsli";

		SourceFixture()
		{
			std::filesystem::create_directory(directory);
			Write(root, "#include \"Common.hlsli\"\nfloat4 main() { return VALUE; }\n");
			Write(include, "#define VALUE 1\n");
		}

		~SourceFixture()
		{
			std::error_code error;
			std::filesystem::remove_all(directory, error);
		}

		static void Write(const std::filesystem::path& a_path, const std::string& a_text)
		{
			std::filesystem::create_directories(a_path.parent_path());
			std::ofstream stream(a_path, std::ios::binary | std::ios::trunc);
			stream << a_text;
			stream.close();
			assert(stream);
		}

		std::optional<std::string> ReadClosure() const
		{
			std::string closure;
			for (const auto& path : { root, include }) {
				std::ifstream stream(path, std::ios::binary);
				if (!stream)
					return std::nullopt;
				const std::string contents{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
				closure += std::to_string(contents.size()) + ':' + contents;
			}
			return closure;
		}

		std::optional<Util::ContentHash::Hash128> ReadFreshDigest() const
		{
			return Util::ShaderSourceProvenance::ReadFreshClosureDigest(root, directory);
		}
	};

	Util::ContentHash::Hash128 RequiredFileHash(const std::filesystem::path& a_path)
	{
		const auto hash = Util::ContentHash::HashFile(a_path);
		assert(hash);
		return *hash;
	}

	std::filesystem::path RequiredCanonical(const std::filesystem::path& a_path)
	{
		std::error_code error;
		const auto canonical = std::filesystem::weakly_canonical(a_path, error);
		assert(!error);
		return canonical;
	}
}

int main()
{
	int readFailures = 0;
	auto CompileWithStableDigest = [&](auto&& a_readDigest, auto&& a_compile) {
		return Util::ShaderSourceProvenance::CompileWithStableDigest(
			a_readDigest, a_compile, [&](std::exception_ptr a_error) {
				assert(a_error);
				++readFailures;
			});
	};
	SourceFixture fixture;
	const auto cachedClosure = fixture.ReadClosure();
	assert(cachedClosure);
	std::vector<bool> refreshed;
	auto readDigest = [&](bool a_refresh) {
		refreshed.push_back(a_refresh);
		return a_refresh ? fixture.ReadClosure() : cachedClosure;
	};

	// Persistence can run after another file version has replaced the compiler input.
	const auto deferredDigest = CompileWithStableDigest(readDigest, [] { return true; });
	assert(deferredDigest == cachedClosure);
	SourceFixture::Write(fixture.include, "#define VALUE 2\n");
	assert(fixture.ReadClosure() != cachedClosure);
	assert(deferredDigest == cachedClosure);
	assert((refreshed == std::vector<bool>{ false, true }));

	// Edits during compilation must be detected even when the timestamp cache is stale.
	for (const bool editRoot : { false, true }) {
		SourceFixture edited;
		const auto before = edited.ReadClosure();
		const auto& path = editRoot ? edited.root : edited.include;
		const auto timestamp = std::filesystem::last_write_time(path);
		const auto digest = CompileWithStableDigest(
			[&](bool a_refresh) { return a_refresh ? edited.ReadClosure() : before; },
			[&] {
				SourceFixture::Write(path, editRoot ? "float4 main() { return 2; }\n" : "#define VALUE 2\n");
				std::filesystem::last_write_time(path, timestamp);
				return true;
			});
		assert(!digest);
		assert(std::filesystem::last_write_time(path) == timestamp);
	}

	SourceFixture unavailable;
	const auto unavailableDigest = CompileWithStableDigest(
		[&](bool) { return unavailable.ReadClosure(); },
		[&] {
			std::filesystem::remove(unavailable.include);
			return true;
		});
	assert(!unavailableDigest);
	bool compiledWithoutDigest = false;
	assert(!CompileWithStableDigest(
		[](bool) { return std::optional<std::string>{}; },
		[&] { compiledWithoutDigest = true; return true; }));
	assert(compiledWithoutDigest);
	assert(!CompileWithStableDigest(readDigest, [] { return false; }));
	assert(readFailures == 0);
	for (const bool failAfterCompile : { false, true }) {
		bool compiled = false;
		assert(!CompileWithStableDigest(
			[&](bool a_refresh) -> std::optional<std::string> {
				if (a_refresh == failAfterCompile)
					throw std::runtime_error("source inspection failed");
				return cachedClosure;
			},
			[&] { compiled = true; return true; }));
		assert(compiled);
	}
	assert(readFailures == 2);

	// Persistent admission must inspect current bytes even when mtimes and any
	// prior in-memory digest hint remain unchanged.
	for (const bool editRoot : { false, true }) {
		SourceFixture sameTimestamp;
		const auto before = sameTimestamp.ReadFreshDigest();
		assert(before);
		const auto& editedPath = editRoot ? sameTimestamp.root : sameTimestamp.include;
		const auto timestamp = std::filesystem::last_write_time(editedPath);
		SourceFixture::Write(
			editedPath,
			editRoot ? "#include \"Common.hlsli\"\nfloat4 main() { return VALUE + 1; }\n" :
					   "#define VALUE 2\n");
		std::filesystem::last_write_time(editedPath, timestamp);
		const auto after = sameTimestamp.ReadFreshDigest();
		assert(after && after != before);
		assert(sameTimestamp.ReadFreshDigest() == after);
		assert(std::filesystem::last_write_time(editedPath) == timestamp);
	}

	// Both include forms are shader-root-first, with the actual including file's
	// directory as fallback for nested local files.
	SourceFixture resolution;
	const auto shadersRoot = resolution.directory / "Shaders";
	const auto source = shadersRoot / "Menu" / "Example.hlsl";
	const auto rootCommon = shadersRoot / "Common.hlsli";
	const auto localCommon = shadersRoot / "Menu" / "Common.hlsli";
	SourceFixture::Write(source, "#include \"Common.hlsli\"\nfloat4 main() { return VALUE; }\n");
	SourceFixture::Write(rootCommon, "#define VALUE 10\n");
	SourceFixture::Write(localCommon, "#define VALUE 20\n");

	using Util::ShaderSourceProvenance::IncludeType;
	const auto resolvedLocal = Util::ShaderSourceProvenance::ResolveIncludePath(
		IncludeType::Local, "Common.hlsli", source, shadersRoot);
	const auto resolvedSystem = Util::ShaderSourceProvenance::ResolveIncludePath(
		IncludeType::System, "Common.hlsli", source, shadersRoot);
	assert(resolvedLocal == RequiredCanonical(rootCommon));
	assert(resolvedSystem == RequiredCanonical(rootCommon));

	const auto expectedCollisionDigest = Util::ContentHash::CombineHashes(
		RequiredFileHash(source), RequiredFileHash(rootCommon));
	assert(Util::ShaderSourceProvenance::ReadFreshClosureDigest(source, shadersRoot) == expectedCollisionDigest);

	const auto systemSource = shadersRoot / "Menu" / "SystemExample.hlsl";
	SourceFixture::Write(systemSource, "#include <Common.hlsli>\nfloat4 main() { return VALUE; }\n");
	const auto expectedSystemDigest = Util::ContentHash::CombineHashes(
		RequiredFileHash(systemSource), RequiredFileHash(rootCommon));
	assert(Util::ShaderSourceProvenance::ReadFreshClosureDigest(systemSource, shadersRoot) == expectedSystemDigest);

	const auto nestedSource = shadersRoot / "Menu" / "NestedExample.hlsl";
	const auto nestedParent = shadersRoot / "Menu" / "Parent.hlsli";
	const auto nestedLeaf = shadersRoot / "Menu" / "LocalOnly" / "Leaf.hlsli";
	SourceFixture::Write(nestedSource, "#include \"Menu/Parent.hlsli\"\nfloat4 main() { return VALUE; }\n");
	SourceFixture::Write(nestedParent, "#include \"LocalOnly/Leaf.hlsli\"\n");
	SourceFixture::Write(nestedLeaf, "#define VALUE 30\n");
	const auto nestedClosure = Util::ContentHash::CombineHashes(
		RequiredFileHash(nestedParent), RequiredFileHash(nestedLeaf));
	const auto expectedNestedDigest = Util::ContentHash::CombineHashes(
		RequiredFileHash(nestedSource), nestedClosure);
	assert(Util::ShaderSourceProvenance::ReadFreshClosureDigest(nestedSource, shadersRoot) == expectedNestedDigest);
	return 0;
}
