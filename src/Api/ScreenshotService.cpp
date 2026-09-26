#include "Api/ScreenshotService.h"

#include "Api/DevBenchMainThreadDispatch.h"
#include "Api/RuntimeThreadAffinity.h"
#include "Api/ScreenshotServiceDispatch.h"
#include "Api/ServiceRegistry.h"
#include "Features/ScreenshotFeature.h"
#include "Globals.h"
#include "VRAPI/CSscreenshotapi.h"
#include "VRAPI/CSserviceapi.h"

#include <SKSE/SKSE.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <mutex>
#include <optional>
#include <string>

namespace
{
	using CSX::ScreenshotAPI::Interface001;
	using CSX::ScreenshotAPI::Request001;
	using CSX::ScreenshotAPI::Response001;
	using CSX::ScreenshotAPI::Status;
	constexpr auto kMainThreadTimeout = std::chrono::seconds(5);

	CSX::Api::DevBenchMainThreadResult HandleOnRuntimeMainThread(nlohmann::json a_request)
	{
		auto handle = [request = std::move(a_request)]() mutable {
			return globals::features::screenshotFeature.HandleApiRequest(request);
		};
		if (CSX::Api::IsRuntimeMainThread()) {
			try {
				return { handle(), std::nullopt };
			} catch (const std::exception& e) {
				return CSX::Api::MakeDevBenchDispatchFailure(
					CSX::Api::DevBenchDispatchErrorFormat::screenshot,
					{ "dispatcher_failed", e.what(), "execution", false, true });
			} catch (...) {
				return CSX::Api::MakeDevBenchDispatchFailure(
					CSX::Api::DevBenchDispatchErrorFormat::screenshot,
					{ "dispatcher_failed", "unknown main-thread failure", "execution", false, true });
			}
		}
		return CSX::Api::RunDevBenchMainThreadTask(
			SKSE::GetTaskInterface(),
			std::move(handle),
			CSX::Api::DevBenchDispatchErrorFormat::screenshot,
			kMainThreadTimeout);
	}

	Status Dispatch(const void*, const Request001* a_request, Response001* a_response) noexcept
	{
		return CSX::Api::DispatchScreenshotRequest(a_request, a_response, HandleOnRuntimeMainThread);
	}

	Interface001& ServiceInterface()
	{
		static Interface001 service{
			.structSize = sizeof(Interface001),
			.major = CSX::ScreenshotAPI::ServiceMajor,
			.minor = CSX::ScreenshotAPI::ServiceMinor,
			.schemaRevision = CSX::ScreenshotAPI::SchemaRevision,
			.context = nullptr,
			.Dispatch = Dispatch,
		};
		return service;
	}
}

namespace CSX::Api
{
	void InitializeScreenshotService()
	{
		static std::once_flag initialized;
		std::call_once(initialized, [] {
			const std::uint64_t capabilities =
				ServiceAPI::kCapabilityInspection |
				ServiceAPI::kCapabilityRuntimeMutation |
				ServiceAPI::kCapabilityPersistentMutation |
				ServiceAPI::kCapabilityAsynchronousOperations |
				ServiceAPI::kCapabilityEventStream;
			const auto status = GetProcessServiceRegistry().Register({
				ScreenshotAPI::ServiceName,
				ScreenshotAPI::ServiceMajor,
				ScreenshotAPI::ServiceMinor,
				ScreenshotAPI::SchemaRevision,
				capabilities,
				&ServiceInterface(),
			});
			if (status != ServiceAPI::Status::kSuccess)
				logger::error("Failed to register CSX screenshot service ({})", static_cast<std::uint32_t>(status));
			else
				logger::info("Registered CSX screenshot service ABI {}.{}", ScreenshotAPI::ServiceMajor, ScreenshotAPI::ServiceMinor);
		});
	}

	const ScreenshotAPI::Interface001* GetScreenshotService001()
	{
		InitializeScreenshotService();
		return &ServiceInterface();
	}

	nlohmann::json DispatchScreenshotServiceRequest(const nlohmann::json& a_request)
	{
		const auto requestText = a_request.dump();
		ScreenshotAPI::Request001 request;
		request.jsonUtf8 = requestText.data();
		request.jsonBytes = static_cast<std::uint32_t>(requestText.size());
		ScreenshotAPI::Response001 response;
		const auto* service = GetScreenshotService001();
		const auto status = service->Dispatch(service->context, &request, &response);
		if (status != ScreenshotAPI::Status::kSuccess || !response.jsonUtf8)
			return MakeScreenshotTransportError(status);
		return nlohmann::json::parse(response.jsonUtf8, response.jsonUtf8 + response.jsonBytes);
	}
}
