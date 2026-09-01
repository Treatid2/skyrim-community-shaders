#include "Features/LightLimitFix/VRHookPolicy.h"

#include <array>
#include <cstdint>

namespace
{
	using namespace LightLimitFixVRHookPolicy;

	constexpr bool CoversExternalBranchRecognition()
	{
		constexpr std::array<std::uint8_t, 2> relativeJump{ 0xE9, 0x00 };
		constexpr std::array<std::uint8_t, 2> indirectJump{ 0xFF, 0x25 };
		constexpr std::array<std::uint8_t, 2> relativeCall{ 0xE8, 0x00 };
		constexpr std::array<std::uint8_t, 1> truncatedIndirectJump{ 0xFF };

		return HasExternalBranchPrefix(relativeJump.data(), relativeJump.size()) &&
		       HasExternalBranchPrefix(indirectJump.data(), indirectJump.size()) &&
		       !HasExternalBranchPrefix(relativeCall.data(), relativeCall.size()) &&
		       !HasExternalBranchPrefix(truncatedIndirectJump.data(), truncatedIndirectJump.size()) &&
		       !HasExternalBranchPrefix(nullptr, 0);
	}

	static_assert(CoversExternalBranchRecognition());
}

int main()
{
	return CoversExternalBranchRecognition() ? 0 : 1;
}
