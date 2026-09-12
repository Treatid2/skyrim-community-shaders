#pragma once

#include "Api/DevBenchMainThreadDispatch.h"
#include "VRAPI/CSscreenshotapi.h"

#include <functional>
#include <nlohmann/json_fwd.hpp>

namespace CSX::Api
{
	using ScreenshotRequestExecutor = std::function<DevBenchMainThreadResult(nlohmann::json)>;

	ScreenshotAPI::Status DispatchScreenshotRequest(
		const ScreenshotAPI::Request001* a_request,
		ScreenshotAPI::Response001* a_response,
		const ScreenshotRequestExecutor& a_execute) noexcept;

	nlohmann::json MakeScreenshotTransportError(ScreenshotAPI::Status a_status);
}
