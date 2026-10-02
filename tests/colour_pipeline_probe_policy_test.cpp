#include "Features/Upscaling/ColourPipelineProbePolicy.h"

#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>

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
	struct Dispatch
	{
		std::uint32_t frame = 0;
		std::uint64_t colourContractRevision = 0;
		std::uint32_t contextIndex = 0;
		std::uint64_t dispatchSerial = 0;
		std::uint64_t contextGeneration = 0;
		std::string path;
		float configuredSharpness = 0;
		float effectiveSharpness = 0;
		bool sharpeningEnabled = false;
	};
	for (const auto* path : { "host", "runtime-fsr3", "runtime-fsr4", "runtime-to-host-fallback" }) {
		for (std::uint32_t eye : { 0u, 1u }) {
			Dispatch input{ 8, 3, eye, 0, 99, "previous-runtime-frame", 0.75f, 0, false };
			const Dispatch actual{ 8, 3, eye, 123 + eye, 42, path, 0.75f, 0.5f, true };
			Require(Policy::BindInputDispatch(input, actual, 8, 3, eye));
			Require(input.path == actual.path && input.contextGeneration == actual.contextGeneration);
			Require(input.dispatchSerial == actual.dispatchSerial && input.contextIndex == eye);
			Require(input.configuredSharpness == 0.75f && input.effectiveSharpness == 0.5f && input.sharpeningEnabled);
			Require(!Policy::BindInputDispatch(input, actual, 9, 3, eye));
			Require(!Policy::BindInputDispatch(input, actual, 8, 4, eye));
			Require(!Policy::BindInputDispatch(input, actual, 8, 3, 1u - eye));
			auto unavailable = actual;
			unavailable.dispatchSerial = 0;
			Require(!Policy::BindInputDispatch(input, unavailable, 8, 3, eye));
			unavailable = actual;
			unavailable.path.clear();
			Require(!Policy::BindInputDispatch(input, unavailable, 8, 3, eye));
		}
	}
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
