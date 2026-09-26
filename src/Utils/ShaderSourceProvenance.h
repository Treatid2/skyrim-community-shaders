#pragma once

#include "ContentHash.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Util::ShaderSourceProvenance
{
	enum class IncludeType
	{
		Local,
		System
	};

	struct IncludeDirective
	{
		IncludeType type;
		std::string name;
	};

	inline std::string NormalizedPathKey(const std::filesystem::path& a_path)
	{
		std::string key = a_path.lexically_normal().string();
#ifdef _WIN32
		std::transform(key.begin(), key.end(), key.begin(), [](unsigned char a_char) {
			return static_cast<char>(std::tolower(a_char));
		});
#endif
		return key;
	}

	/** Parses a textual HLSL include directive without evaluating conditions. */
	inline std::optional<IncludeDirective> ParseIncludeDirective(std::string_view a_line)
	{
		size_t position = a_line.find_first_not_of(" \t");
		if (position == std::string_view::npos || a_line[position] != '#')
			return std::nullopt;

		position = a_line.find_first_not_of(" \t", position + 1);
		if (position == std::string_view::npos || a_line.substr(position, 7) != "include")
			return std::nullopt;

		const size_t firstDelimiter = a_line.find_first_of("\"<", position + 7);
		if (firstDelimiter == std::string_view::npos)
			return std::nullopt;
		const bool local = a_line[firstDelimiter] == '"';
		const char closeDelimiter = local ? '"' : '>';
		const size_t secondDelimiter = a_line.find(closeDelimiter, firstDelimiter + 1);
		if (secondDelimiter == std::string_view::npos || secondDelimiter == firstDelimiter + 1)
			return std::nullopt;

		return IncludeDirective{
			local ? IncludeType::Local : IncludeType::System,
			std::string(a_line.substr(firstDelimiter + 1, secondDelimiter - firstDelimiter - 1))
		};
	}

	/** Resolves includes identically for provenance reads and the D3D compiler. */
	inline std::optional<std::filesystem::path> ResolveIncludePath(
		IncludeType a_type,
		std::string_view a_name,
		const std::filesystem::path& a_includingFile,
		const std::filesystem::path& a_shadersRoot)
	{
		const auto relative = std::filesystem::path(a_name);
		const auto local = a_includingFile.parent_path() / relative;
		const auto rooted = a_shadersRoot / relative;
		// CSX shader includes are root-relative first for both delimiter forms;
		// the including file's directory is the deterministic fallback.
		(void)a_type;
		const std::array candidates{ rooted, local };
		for (const auto& candidate : candidates) {
			std::error_code regularError;
			if (!std::filesystem::is_regular_file(candidate, regularError))
				continue;
			std::error_code canonicalError;
			const auto canonical = std::filesystem::weakly_canonical(candidate, canonicalError);
			return canonicalError ? candidate.lexically_normal() : canonical;
		}
		return std::nullopt;
	}

	namespace detail
	{
		inline std::optional<ContentHash::Hash128> ReadFreshClosureDigestInternal(
			const std::filesystem::path& a_path,
			const std::filesystem::path& a_shadersRoot,
			std::unordered_map<std::string, std::optional<ContentHash::Hash128>>& a_results,
			std::unordered_set<std::string>& a_visiting,
			bool& a_complete)
		{
			const auto key = NormalizedPathKey(a_path);
			if (const auto result = a_results.find(key); result != a_results.end())
				return result->second;
			if (!a_visiting.insert(key).second)
				return std::nullopt;

			std::ifstream file(a_path, std::ios::binary);
			if (!file.is_open()) {
				a_complete = false;
				a_visiting.erase(key);
				return std::nullopt;
			}
			const std::string raw{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
			if (file.bad()) {
				a_complete = false;
				a_visiting.erase(key);
				return std::nullopt;
			}
			std::string contents;
			contents.reserve(raw.size());
			for (size_t index = 0; index < raw.size(); ++index) {
				if (raw[index] == '\r' && index + 1 < raw.size() && raw[index + 1] == '\n')
					continue;
				contents.push_back(raw[index]);
			}
			const auto selfHash = ContentHash::HashString(contents);

			std::vector<std::pair<std::string, std::filesystem::path>> includes;
			std::istringstream stream(contents);
			std::string line;
			while (std::getline(stream, line)) {
				const auto directive = ParseIncludeDirective(line);
				if (!directive)
					continue;
				if (const auto includePath = ResolveIncludePath(
						directive->type, directive->name, a_path, a_shadersRoot)) {
					includes.emplace_back(NormalizedPathKey(*includePath), *includePath);
				}
			}
			std::sort(includes.begin(), includes.end(), [](const auto& a_left, const auto& a_right) {
				return a_left.first < a_right.first;
			});

			auto combined = selfHash;
			for (const auto& [includeKey, includePath] : includes) {
				if (a_visiting.contains(includeKey))
					continue;
				const auto child = ReadFreshClosureDigestInternal(
					includePath, a_shadersRoot, a_results, a_visiting, a_complete);
				if (!a_complete) {
					a_visiting.erase(key);
					return std::nullopt;
				}
				if (child)
					combined = ContentHash::CombineHashes(combined, *child);
			}

			a_visiting.erase(key);
			a_results[key] = combined;
			return combined;
		}
	}

	/** Reads current source bytes and returns their resolved transitive closure digest. */
	inline std::optional<ContentHash::Hash128> ReadFreshClosureDigest(
		const std::filesystem::path& a_path,
		const std::filesystem::path& a_shadersRoot)
	{
		std::unordered_map<std::string, std::optional<ContentHash::Hash128>> results;
		std::unordered_set<std::string> visiting;
		bool complete = true;
		const auto digest = detail::ReadFreshClosureDigestInternal(
			a_path, a_shadersRoot, results, visiting, complete);
		return complete ? digest : std::nullopt;
	}

	/** Runs compilation and retains its input digest only when a fresh closure read agrees. */
	template <class ReadDigest, class Compile, class ReadFailure>
	auto CompileWithStableDigest(ReadDigest&& a_readDigest, Compile&& a_compile, ReadFailure&& a_readFailure)
	{
		auto readDigest = [&](bool a_refresh) {
			try {
				return a_readDigest(a_refresh);
			} catch (...) {
				a_readFailure(std::current_exception());
				return decltype(a_readDigest(a_refresh)){};
			}
		};
		const auto before = readDigest(false);
		if (!a_compile())
			return decltype(before){};
		const auto after = readDigest(true);
		return before && before == after ? before : decltype(before){};
	}
}
