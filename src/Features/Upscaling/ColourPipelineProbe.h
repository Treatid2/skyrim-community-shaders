#pragma once

#ifdef DEVBENCH_BRIDGE_ENABLED

#	include <d3d11.h>
#	include <nlohmann/json_fwd.hpp>

#	include <cstdint>
#	include <string>

namespace CSX::Diagnostics::ColourPipelineProbe
{
	enum class Stage : std::uint8_t
	{
		FsrInput,
		FsrOutput,
		CombinedMain,
		ImageSpaceInput,
		ImageSpaceOutput
	};

	struct DispatchMetadata
	{
		std::uint64_t colourContractRevision = 0;
		std::uint32_t frame = 0;
		std::uint64_t dispatchSerial = 0;
		std::uint64_t contextGeneration = 0;
		std::uint32_t renderWidth = 0;
		std::uint32_t renderHeight = 0;
		std::uint32_t displayWidth = 0;
		std::uint32_t displayHeight = 0;
		bool requestedHighDynamicRangeInput = false;
		bool requestedAutoExposure = false;
		bool effectiveHighDynamicRangeInput = false;
		bool effectiveAutoExposure = false;
		bool exposureResourceBound = false;
		float preExposure = 1.0f;
		std::string path;
	};

	bool Arm(
		const std::string& a_captureId,
		std::uint64_t a_expectedColourContractRevision,
		const nlohmann::json& a_metadata,
		std::string& a_error);
	bool Reset(std::string& a_error);
	bool WantsVendorCapture() noexcept;
	void ServiceReadbacks(ID3D11DeviceContext* a_context, std::uint32_t a_cpuFrame) noexcept;
	void CaptureVendorStage(
		Stage a_stage,
		std::uint32_t a_eye,
		ID3D11Texture2D* a_texture,
		ID3D11ShaderResourceView* a_srv,
		ID3D11RenderTargetView* a_rtv,
		ID3D11UnorderedAccessView* a_uav,
		std::uint32_t a_activeWidth,
		std::uint32_t a_activeHeight,
		const DispatchMetadata& a_dispatch,
		const char* a_symbol,
		const char* a_callsite) noexcept;
	void CaptureImageSpaceStage(
		Stage a_stage,
		ID3D11Texture2D* a_texture,
		ID3D11ShaderResourceView* a_srv,
		ID3D11RenderTargetView* a_rtv,
		ID3D11UnorderedAccessView* a_uav,
		std::uint32_t a_engineTarget,
		bool a_matchesMainTarget,
		const char* a_symbol,
		const char* a_callsite) noexcept;
	void RecordFsrOutputCopyBack(
		ID3D11Texture2D* a_combinedDestination,
		std::uint32_t a_eyeWidth,
		std::uint32_t a_eyeHeight,
		const char* a_symbol,
		const char* a_callsite) noexcept;
	nlohmann::json BuildStatus();
	nlohmann::json BuildCapture();
}

#endif
