#include "Features/Wetterness/PuddleMaskCachePolicy.h"

#include <cstdint>
#include <limits>

namespace
{
	using PuddleMaskCachePolicy::CanReuse;
	using PuddleMaskCachePolicy::Decision;
	using PuddleMaskCachePolicy::Evaluate;
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
	static_assert(Evaluate(true, true, cachedTextured, cachedTextured) == Decision::Reuse);
	static_assert(Evaluate(true, true, cachedTextured, Stamp{ 41u, 8u }) == Decision::RefreshResourcePublication);
	static_assert(Evaluate(true, true, cachedTextured, Stamp{ 42u, 7u }) == Decision::Recalculate);
	static_assert(Evaluate(false, true, cachedTextured, cachedTextured) == Decision::Recalculate);
	static_assert(Evaluate(true, false, cachedTextured, cachedTextured) == Decision::Recalculate);

	struct PublicationModel
	{
		Stamp cached{};
		bool hasCachedData = false;
		double rainTimerSeconds = 0.0;
		uint32_t effectiveMode = 0;
	};

	constexpr void Publish(
		PublicationModel& a_model,
		Stamp a_current,
		bool a_resourceAvailable,
		double a_frameDeltaSeconds,
		bool a_paused = false)
	{
		const auto decision = Evaluate(true, a_model.hasCachedData, a_model.cached, a_current);
		if (decision == Decision::Recalculate && !a_paused && a_frameDeltaSeconds > 0.0) {
			a_model.rainTimerSeconds += a_frameDeltaSeconds;
		}
		if (decision != Decision::Reuse) {
			a_model.effectiveMode = a_resourceAvailable ? 1u : 0u;
			a_model.cached = a_current;
			a_model.hasCachedData = true;
		}
	}

	constexpr bool SameFrameResourceTransitionsPreserveTime()
	{
		PublicationModel model{};
		Publish(model, Stamp{ 41u, 7u }, true, 0.25);
		if (model.rainTimerSeconds != 0.25 || model.effectiveMode != 1u) {
			return false;
		}

		Publish(model, Stamp{ 41u, 8u }, false, 0.25);
		if (model.rainTimerSeconds != 0.25 || model.effectiveMode != 0u) {
			return false;
		}

		Publish(model, Stamp{ 41u, 9u }, true, 0.25);
		if (model.rainTimerSeconds != 0.25 || model.effectiveMode != 1u) {
			return false;
		}

		Publish(model, Stamp{ 42u, 9u }, true, 0.25);
		return model.rainTimerSeconds == 0.5 && model.effectiveMode == 1u;
	}

	constexpr bool PauseAndZeroDeltaPreserveTime()
	{
		PublicationModel paused{};
		Publish(paused, Stamp{ 41u, 7u }, true, 0.25, true);
		PublicationModel zeroDelta{};
		Publish(zeroDelta, Stamp{ 41u, 7u }, true, 0.0);
		return paused.rainTimerSeconds == 0.0 && zeroDelta.rainTimerSeconds == 0.0;
	}

	static_assert(SameFrameResourceTransitionsPreserveTime());
	static_assert(PauseAndZeroDeltaPreserveTime());

	static_assert(NextResourceGeneration(7u) == 8u);
	static_assert(NextResourceGeneration(std::numeric_limits<std::uint64_t>::max()) == 1u);
}

int main() {}
