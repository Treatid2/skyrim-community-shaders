#include "Features/Upscaling/VRVendorRelatchPolicy.h"

#include <cassert>

int main()
{
	using namespace VRVendorRelatchPolicy;

	// Native/RS-off retains the selected FSR method as a setting, but its
	// physical resource contract has no submit-stage vendor backend.
	assert(!RequiresVendorRuntime(false, true));
	assert(!RequiresFSRCompatibility(false, true));
	assert(!NeedsDeferredFSRReset(false, true, false, false));

	// An active FSR contract still requires creation, compatibility proof, and
	// a deferred rebuild when neither preserved nor recreated resources exist.
	assert(RequiresVendorRuntime(true, true));
	assert(RequiresFSRCompatibility(true, true));
	assert(NeedsDeferredFSRReset(true, true, false, false));
	assert(!NeedsDeferredFSRReset(true, true, true, false));
	assert(!NeedsDeferredFSRReset(true, true, false, true));

	// Active non-vendor methods do not acquire a vendor runtime.
	assert(!RequiresVendorRuntime(true, false));

	// The relatch transaction exclusively owns resource teardown and creation.
	// Ordinary frame/resource checks must not re-enter the vendor backend while
	// either queued mutation or synchronous target recreation is active.
	assert(ShouldDeferOrdinaryResourceRefresh(true, true, false));
	assert(ShouldDeferOrdinaryResourceRefresh(true, false, true));
	assert(ShouldDeferOrdinaryResourceRefresh(true, true, true));
	assert(!ShouldDeferOrdinaryResourceRefresh(true, false, false));
	assert(!ShouldDeferOrdinaryResourceRefresh(false, true, true));
}
