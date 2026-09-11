#pragma once

#include <cstdint>
#include <limits>

namespace PuddleMaskCachePolicy
{
	struct Stamp
	{
		uint32_t frame = 0;
		std::uint64_t resourceGeneration = 0;

		constexpr bool operator==(const Stamp&) const = default;
	};

	constexpr bool CanReuse(bool a_cacheAllowed, bool a_hasCachedData, Stamp a_cached, Stamp a_current)
	{
		return a_cacheAllowed && a_hasCachedData && a_cached == a_current;
	}

	constexpr std::uint64_t NextResourceGeneration(std::uint64_t a_current)
	{
		return a_current == std::numeric_limits<std::uint64_t>::max() ? 1u : a_current + 1u;
	}
}
