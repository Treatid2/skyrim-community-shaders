#include "Features/Upscaling/FSRColorContractPolicy.h"
#include "Features/Upscaling/VRSubmitColorContract.h"

namespace
{
	using namespace VRSubmitColorContract;

	constexpr bool CoversPresentationAndVendorAdmission()
	{
		const auto automatic = Resolve(true, SourceColorSpace::Automatic);
		const auto gamma = Resolve(true, SourceColorSpace::Gamma);
		const auto linear = Resolve(true, SourceColorSpace::Linear);
		if (automatic != gamma || !IsPresentationSupported(gamma) || !IsVendorSupported(gamma) || DLSSUsesHDR(gamma))
			return false;
		if (linear == gamma || !IsPresentationSupported(linear) || IsVendorSupported(linear) || DLSSUsesHDR(linear))
			return false;

		constexpr SourceColorSpace sources[]{ SourceColorSpace::Automatic, SourceColorSpace::Gamma, SourceColorSpace::Linear };
		for (const auto source : sources) {
			if (IsPresentationSupported(Resolve(false, source)) || IsVendorSupported(Resolve(false, source)))
				return false;
		}
		return !IsPresentationSupported(Resolve(true, SourceColorSpace::Unsupported)) &&
		       !IsVendorSupported(Resolve(true, static_cast<SourceColorSpace>(255)));
	}

	constexpr bool CoversIndependentRangeAndProcessing()
	{
		const Contract linearLDR{ Transfer::Linear, DynamicRange::LDR };
		const Contract linearHDR{ Transfer::Linear, DynamicRange::HDR };
		const Contract gammaHDR{ Transfer::Gamma, DynamicRange::HDR };
		return !DLSSUsesHDR(linearLDR) && DLSSUsesHDR(linearHDR) && DLSSUsesHDR(gammaHDR) &&
		       !IsPresentationSupported(linearHDR) && !IsVendorSupported(gammaHDR) &&
		       !IsPresentationSupported({}) && !IsVendorSupported({}) &&
		       kLegacyFsrHighDynamicRange;
	}

	constexpr bool CoversFsrProcessingControl()
	{
		using namespace FSRColorContractPolicy;
		const auto initial = Decode(kDefaultState);
		if (initial != Requested{} ||
			!ContextMatches(ContextState(kDefaultState), kDefaultState)) {
			return false;
		}

		const auto stale = PlanUpdate(kDefaultState, 0, false, false);
		if (stale.revisionMatched || stale.changed || stale.resultingRevision != 1)
			return false;
		const auto unchanged = PlanUpdate(kDefaultState, 1, true, true);
		if (!unchanged.revisionMatched || unchanged.changed ||
			unchanged.desiredState != kDefaultState || unchanged.resultingRevision != 1) {
			return false;
		}

		const auto changed = PlanUpdate(kDefaultState, 1, false, true);
		const auto decoded = Decode(changed.desiredState);
		if (!changed.revisionMatched || !changed.changed ||
			decoded.revision != 2 || decoded.highDynamicRangeInput || !decoded.autoExposure) {
			return false;
		}
		return !CanReuseContext(ContextState(kDefaultState), changed.desiredState, false) &&
		       CanReuseContext(ContextState(kDefaultState), changed.desiredState, true) &&
		       CanReuseContext(ContextState(changed.desiredState), changed.desiredState, false);
	}

	static_assert(CoversPresentationAndVendorAdmission());
	static_assert(CoversIndependentRangeAndProcessing());
	static_assert(CoversFsrProcessingControl());
}

int main()
{
	return CoversPresentationAndVendorAdmission() && CoversIndependentRangeAndProcessing() &&
	               CoversFsrProcessingControl() ?
	           0 :
	           1;
}
