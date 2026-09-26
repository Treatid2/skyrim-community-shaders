#pragma once

#include <cstdint>

namespace FSRMainPassPolicy
{
	enum class Result : std::uint8_t
	{
		Ready,
		Deferred,
		Failed
	};

	[[nodiscard]] constexpr Result ClassifyResetBlock(
		bool a_resetApplied,
		bool a_lifecycleFailed,
		bool a_sameTerminalRequest) noexcept
	{
		if (a_resetApplied)
			return Result::Ready;
		return a_lifecycleFailed || a_sameTerminalRequest ?
		           Result::Failed :
		           Result::Deferred;
	}

	[[nodiscard]] constexpr bool RequiresCurrentInputFallback(
		bool a_fsrSelected,
		Result a_result) noexcept
	{
		// FSR has no publishable full-size color for either non-ready result.
		return a_fsrSelected && a_result != Result::Ready;
	}
}
