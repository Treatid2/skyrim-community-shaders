#include "Features/Wetterness/PuddleMaskCachePolicy.h"

#include <cstdint>
#include <limits>

namespace
{
	using PuddleMaskCachePolicy::CanReuse;
	using PuddleMaskCachePolicy::NextResourceGeneration;
	using PuddleMaskCachePolicy::Stamp;

	constexpr Stamp cachedTextured{ 41u, 7u };

	static_assert(CanReuse(true, true, cachedTextured, cachedTextured));
	static_assert(!CanReuse(false, true, cachedTextured, cachedTextured));
	static_assert(!CanReuse(true, false, cachedTextured, cachedTextured));

	// A failed setup in the same frame advances the resource generation, so the
	// old textured publication cannot be reused before Simple is republished.
	static_assert(!CanReuse(true, true, cachedTextured, Stamp{ 41u, 8u }));
	static_assert(CanReuse(true, true, Stamp{ 41u, 8u }, Stamp{ 41u, 8u }));

	// Successful recovery is also a resource transition and invalidates the
	// same-frame fallback publication before Textured can be republished.
	static_assert(!CanReuse(true, true, Stamp{ 41u, 8u }, Stamp{ 41u, 9u }));
	static_assert(!CanReuse(true, true, Stamp{ 41u, 9u }, Stamp{ 42u, 9u }));

	static_assert(NextResourceGeneration(7u) == 8u);
	static_assert(NextResourceGeneration(std::numeric_limits<std::uint64_t>::max()) == 1u);
}

int main() {}
