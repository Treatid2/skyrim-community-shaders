#include "Features/Upscaling/ColourPipelineProbePolicy.h"

#include <initializer_list>
#include <limits>
#include <stdexcept>

namespace
{
	void Require(bool a_condition)
	{
		if (!a_condition)
			throw std::runtime_error("colour-pipeline staging policy regression");
	}
}

int main()
{
	namespace Policy = CSX::Diagnostics::ColourPipelineProbe::Policy;
	constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
	Require(Policy::ValidRectangle(4096, 2048, 2048, 0, 2048, 2048));
	Require(!Policy::ValidRectangle(4096, 2048, 2049, 0, 2048, 2048));
	Require(!Policy::ValidRectangle(maximum, maximum, maximum - 3, 0, 8, 1));
	Require(!Policy::ValidRectangle(32, 32, maximum, 0, 1, 1));
	Require(!Policy::ValidRectangle(32, 32, 0, 0, 0, 1));
	Require(!Policy::ValidRectangle(maximum, maximum, 0, 0, maximum, maximum));
	Require(Policy::CanAllocate(8192, 8192, 4, 0));
	Require(!Policy::CanAllocate(8192, 8192, 8, 0));
	Require(!Policy::CanAllocate(8193, 1, 1, 0));
	Require(!Policy::CanAllocate(1, 1, 17, 0));
	Require(!Policy::CanAllocate(1, 1, 0, 0));
	Require(!Policy::CanAllocate(0, 1, 4, 0));
	Require(!Policy::CanAllocate(1, 1, 4, std::numeric_limits<std::uint64_t>::max()));
	for (std::uint32_t bytesPerPixel : { 4u, 8u, 16u }) {
		for (std::uint32_t width : { 1u, 17u, 1920u, 4096u, 8192u }) {
			const auto bytes = std::uint64_t{ width } * 17 * bytesPerPixel;
			Require(Policy::CanAllocate(width, 17, bytesPerPixel, Policy::kMaximumCaptureBytes - bytes));
			Require(!Policy::CanAllocate(width, 17, bytesPerPixel, Policy::kMaximumCaptureBytes - bytes + 1));
		}
	}
}
