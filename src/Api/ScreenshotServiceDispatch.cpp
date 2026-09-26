#include "Api/ScreenshotServiceDispatch.h"

#include <nlohmann/json.hpp>

#include <limits>
#include <string>

namespace CSX::Api
{
	ScreenshotAPI::Status DispatchScreenshotRequest(
		const ScreenshotAPI::Request001* a_request,
		ScreenshotAPI::Response001* a_response,
		const ScreenshotRequestExecutor& a_execute) noexcept
	{
		using ScreenshotAPI::Request001;
		using ScreenshotAPI::Response001;
		using ScreenshotAPI::Status;

		if (!a_response)
			return Status::kInvalidArgument;
		if (a_response->structSize < sizeof(Response001))
			return Status::kStructureTooSmall;
		a_response->status = Status::kInternalError;
		a_response->jsonUtf8 = nullptr;
		a_response->jsonBytes = 0;

		if (!a_request) {
			a_response->status = Status::kInvalidArgument;
			return a_response->status;
		}
		if (a_request->structSize < sizeof(Request001)) {
			a_response->status = Status::kStructureTooSmall;
			return a_response->status;
		}
		if (!a_request->jsonUtf8 || a_request->jsonBytes == 0 || !a_execute) {
			a_response->status = Status::kInvalidArgument;
			return a_response->status;
		}
		if (a_request->jsonBytes > ScreenshotAPI::MaximumRequestBytes) {
			a_response->status = Status::kRequestTooLarge;
			return a_response->status;
		}

		nlohmann::json request;
		try {
			request = nlohmann::json::parse(
				a_request->jsonUtf8,
				a_request->jsonUtf8 + a_request->jsonBytes);
		} catch (const nlohmann::json::parse_error&) {
			a_response->status = Status::kInvalidJson;
			return a_response->status;
		} catch (...) {
			a_response->status = Status::kInternalError;
			return a_response->status;
		}
		if (!request.is_object()) {
			a_response->status = Status::kInvalidJson;
			return a_response->status;
		}

		try {
			auto dispatch = a_execute(std::move(request));
			if (dispatch.failure && !dispatch.failure->admitted) {
				a_response->status = Status::kServiceUnavailable;
				return a_response->status;
			}

			thread_local std::string responseStorage;
			responseStorage = dispatch.response.dump();
			if (responseStorage.size() > std::numeric_limits<std::uint32_t>::max()) {
				a_response->status = Status::kInternalError;
				return a_response->status;
			}
			a_response->jsonUtf8 = responseStorage.c_str();
			a_response->jsonBytes = static_cast<std::uint32_t>(responseStorage.size());
			a_response->status = Status::kSuccess;
			return a_response->status;
		} catch (...) {
			a_response->status = Status::kInternalError;
			return a_response->status;
		}
	}

	nlohmann::json MakeScreenshotTransportError(ScreenshotAPI::Status a_status)
	{
		return {
			{ "ok", false },
			{ "error", {
						   { "code", "transport_error" },
						   { "transportStatus", static_cast<std::uint32_t>(a_status) },
						   { "phase", a_status == ScreenshotAPI::Status::kServiceUnavailable ? "admission" : "transport" },
						   { "retryable", a_status == ScreenshotAPI::Status::kServiceUnavailable },
					   } },
		};
	}
}
