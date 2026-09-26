#define DEVBENCH_BRIDGE_ENABLED 1
#include "Features/Upscaling/VRRenderScaleRetryTelemetry.h"

namespace
{
	using namespace VRRenderScaleRetryTelemetry;

	constexpr Context Owner(uint64_t a_request, uint64_t a_epoch)
	{
		return { 7, a_request, a_epoch, 2, 3, 4 };
	}

	constexpr bool CoversViewportOwnership()
	{
		const ViewportOwner owner{ Owner(11, 13), 17, 19 };
		return OwnsViewportObservation(owner, Owner(11, 13), 17, 19) &&
		       !OwnsViewportObservation(owner, Owner(12, 13), 17, 19) &&
		       !OwnsViewportObservation(owner, Owner(11, 14), 17, 19) &&
		       !OwnsViewportObservation(owner, Owner(11, 13), 18, 19) &&
		       !OwnsViewportObservation(owner, Owner(11, 13), 17, 20);
	}

	constexpr bool CoversQualificationPropagation()
	{
		const auto candidate = ResolveQualificationContext(
			EventType::PromotionCandidate, 5, true, {});
		const auto promoted = ResolveQualificationContext(
			EventType::Promoted, 0, false, candidate);
		const auto cleared = ResolveQualificationContext(
			EventType::GuardCleared, 0, false, candidate);
		const auto unrelated = ResolveQualificationContext(
			EventType::Retry, 0, false, candidate);
		return candidate.known && candidate.requiredStableCycles == 5 &&
		       candidate.doorHandoff && promoted.known &&
		       promoted.requiredStableCycles == 5 && promoted.doorHandoff &&
		       cleared.known && cleared.requiredStableCycles == 5 &&
		       cleared.doorHandoff && !unrelated.known;
	}

	static_assert(CoversViewportOwnership());
	static_assert(CoversQualificationPropagation());
}

int main()
{
	return CoversViewportOwnership() && CoversQualificationPropagation() ? 0 : 1;
}
