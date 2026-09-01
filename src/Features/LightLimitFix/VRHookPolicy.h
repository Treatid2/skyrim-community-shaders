#pragma once

#include <cstddef>
#include <cstdint>

namespace LightLimitFixVRHookPolicy
{
	[[nodiscard]] constexpr bool HasExternalBranchPrefix(
		const std::uint8_t* a_instructions,
		std::size_t a_size) noexcept
	{
		if (a_instructions == nullptr || a_size == 0)
			return false;

		return a_instructions[0] == 0xE9 ||
		       (a_size >= 2 && a_instructions[0] == 0xFF && a_instructions[1] == 0x25);
	}
}
