#pragma once

#include <cstdint>

namespace CSX::Diagnostics::ColourPipelineProbe::Policy
{
	inline constexpr std::uint32_t kMaximumDimension = 8192;
	inline constexpr std::uint64_t kMaximumSlotBytes = 256ull * 1024 * 1024;
	inline constexpr std::uint64_t kMaximumCaptureBytes = 1024ull * 1024 * 1024;
	inline constexpr std::uint32_t kMaximumCaptureIdBytes = 128;
	inline constexpr std::uint32_t kMaximumMetadataBytes = 16 * 1024;
	inline constexpr std::uint32_t kTimeoutSeconds = 15;

	/** Check source bounds before addition or staging allocation. */
	inline constexpr bool ValidRectangle(
		std::uint32_t a_sourceWidth, std::uint32_t a_sourceHeight,
		std::uint32_t a_x, std::uint32_t a_y,
		std::uint32_t a_width, std::uint32_t a_height) noexcept
	{
		return a_width && a_height && a_width <= kMaximumDimension &&
		       a_height <= kMaximumDimension && a_x <= a_sourceWidth &&
		       a_y <= a_sourceHeight && a_width <= a_sourceWidth - a_x &&
		       a_height <= a_sourceHeight - a_y;
	}

	/** Bound payload bytes; driver allocation overhead is not measured. */
	inline constexpr bool CanAllocate(
		std::uint32_t a_width, std::uint32_t a_height,
		std::uint32_t a_bytesPerPixel, std::uint64_t a_allocated) noexcept
	{
		if (!a_width || !a_height || a_width > kMaximumDimension ||
			a_height > kMaximumDimension || !a_bytesPerPixel ||
			a_bytesPerPixel > 16 || a_allocated > kMaximumCaptureBytes)
			return false;
		const auto bytes = std::uint64_t{ a_width } * a_height * a_bytesPerPixel;
		return bytes <= kMaximumSlotBytes && bytes <= kMaximumCaptureBytes - a_allocated;
	}
}
