#pragma once

namespace VRVendorRelatchPolicy
{
	constexpr bool RequiresVendorRuntime(bool a_targetActive, bool a_vendorMethod)
	{
		return a_targetActive && a_vendorMethod;
	}

	constexpr bool RequiresFSRCompatibility(bool a_targetActive, bool a_fsrMethod)
	{
		return a_targetActive && a_fsrMethod;
	}

	constexpr bool NeedsDeferredFSRReset(
		bool a_targetActive,
		bool a_fsrMethod,
		bool a_preservedResources,
		bool a_recreatedResources)
	{
		return a_targetActive &&
		       a_fsrMethod &&
		       !a_preservedResources &&
		       !a_recreatedResources;
	}
}
