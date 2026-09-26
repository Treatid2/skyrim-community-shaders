#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace EncodeTexturesHostPolicy
{
	inline constexpr float MaximumSamplingDimension = 16384.0f;

	inline std::optional<std::uint32_t> NormalizeSamplingDimension(
		float a_value) noexcept
	{
		if (!std::isfinite(a_value) || a_value <= 0.0f)
			return std::nullopt;

		const float rounded = std::round(a_value);
		const float tolerance =
			std::max(1.0f, std::abs(rounded)) *
			std::numeric_limits<float>::epsilon() * 4.0f;
		if (std::abs(a_value - rounded) > tolerance ||
			rounded < 1.0f ||
			rounded > MaximumSamplingDimension) {
			return std::nullopt;
		}

		return static_cast<std::uint32_t>(rounded);
	}
}
