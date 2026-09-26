#include "Api/ScreenshotServiceDispatch.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

using CSX::Api::DevBenchDispatchFailure;
using CSX::Api::DevBenchMainThreadResult;
using CSX::Api::DispatchScreenshotRequest;
using CSX::Api::MakeScreenshotTransportError;
using CSX::ScreenshotAPI::Request001;
using CSX::ScreenshotAPI::Response001;
using CSX::ScreenshotAPI::Status;
using json = nlohmann::json;

namespace
{
	void Check(bool a_condition, std::string_view a_message)
	{
		if (!a_condition)
			throw std::runtime_error(std::string(a_message));
	}

	Request001 MakeRequest(const std::string& a_json)
	{
		Request001 request;
		request.jsonUtf8 = a_json.data();
		request.jsonBytes = static_cast<std::uint32_t>(a_json.size());
		return request;
	}

	json ReadResponse(const Response001& a_response)
	{
		return json::parse(a_response.jsonUtf8, a_response.jsonUtf8 + a_response.jsonBytes);
	}
}

int RunTest()
{
	const std::string requestText = R"({"action":"status"})";
	auto request = MakeRequest(requestText);
	Response001 response;

	auto status = DispatchScreenshotRequest(
		&request,
		&response,
		[](json) -> DevBenchMainThreadResult {
			return {
				json{ { "ok", false } },
				DevBenchDispatchFailure{ "dispatcher_timeout", "not admitted", "admission", true, false },
			};
		});
	Check(status == Status::kServiceUnavailable && response.status == status,
		"pre-admission failure did not remain a retryable transport failure");
	Check(!response.jsonUtf8 && response.jsonBytes == 0,
		"pre-admission failure published a command response");

	response = {};
	status = DispatchScreenshotRequest(
		&request,
		&response,
		[](json) -> DevBenchMainThreadResult {
			return CSX::Api::MakeDevBenchDispatchFailure(
				CSX::Api::DevBenchDispatchErrorFormat::screenshot,
				{ "dispatcher_failed", "callback failed", "execution", false, true });
		});
	Check(status == Status::kSuccess && response.status == status,
		"admitted execution failure was collapsed into a transport failure");
	const auto admittedFailure = ReadResponse(response);
	Check(
		!admittedFailure.at("ok").get<bool>() &&
			admittedFailure.at("error").at("message") == "callback failed" &&
			admittedFailure.at("error").at("phase") == "execution" &&
			!admittedFailure.at("error").at("retryable").get<bool>() &&
			admittedFailure.at("error").at("admitted").get<bool>(),
		"admitted execution failure lost its non-retryable provenance");

	response = {};
	status = DispatchScreenshotRequest(
		&request,
		&response,
		[](json command) -> DevBenchMainThreadResult {
			return { json{ { "ok", true }, { "action", command.at("action") } }, std::nullopt };
		});
	Check(status == Status::kSuccess && ReadResponse(response).at("action") == "status",
		"successful command response did not pass through unchanged");

	const std::string invalidText = "[";
	auto invalidRequest = MakeRequest(invalidText);
	response = {};
	status = DispatchScreenshotRequest(
		&invalidRequest,
		&response,
		[](json) -> DevBenchMainThreadResult { return { json::object(), std::nullopt }; });
	Check(status == Status::kInvalidJson, "invalid JSON reached the command executor");

	const std::string arrayText = "[]";
	auto arrayRequest = MakeRequest(arrayText);
	response = {};
	status = DispatchScreenshotRequest(
		&arrayRequest,
		&response,
		[](json) -> DevBenchMainThreadResult { return { json::object(), std::nullopt }; });
	Check(status == Status::kInvalidJson, "non-object JSON reached the command executor");

	response = {};
	status = DispatchScreenshotRequest(
		&request,
		&response,
		[](json) -> DevBenchMainThreadResult { throw std::runtime_error("executor escaped"); });
	Check(status == Status::kInternalError, "escaping executor failure was not contained");
	response = {};
	status = DispatchScreenshotRequest(
		&request,
		&response,
		[](json) -> DevBenchMainThreadResult { return { json::parse("["), std::nullopt }; });
	Check(status == Status::kInternalError,
		"executor parse failure was misreported as invalid request JSON");

	const auto admission = MakeScreenshotTransportError(Status::kServiceUnavailable);
	Check(admission.at("error").at("phase") == "admission" &&
			  admission.at("error").at("retryable").get<bool>(),
		"service-unavailable adapter error is not retryable admission failure");
	const auto transport = MakeScreenshotTransportError(Status::kInternalError);
	Check(transport.at("error").at("phase") == "transport" &&
			  !transport.at("error").at("retryable").get<bool>(),
		"internal transport failure was marked retryable");

	return 0;
}

int main()
{
	try {
		return RunTest();
	} catch (const std::exception& e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
