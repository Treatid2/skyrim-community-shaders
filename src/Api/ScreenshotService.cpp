#include "Api/ScreenshotService.h"

#include "Api/DevBenchMainThreadDispatch.h"
#include "Api/MainThreadDispatchState.h"
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
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace
{
	using CSX::ScreenshotAPI::Interface001;
	using CSX::ScreenshotAPI::Request001;
	using CSX::ScreenshotAPI::Response001;
	using CSX::ScreenshotAPI::Status;
	constexpr auto kMainThreadTimeout = std::chrono::seconds(5);

	CSX::Api::DevBenchMainThreadResult HandleOnRuntimeMainThread(nlohmann::json a_request)
	{
		const auto dispatchRequest = a_request;
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

		auto failure = [](std::string a_code, std::string a_message, std::string a_phase,
						   bool a_retryable, bool a_admitted) {
			return CSX::Api::MakeDevBenchDispatchFailure(
				CSX::Api::DevBenchDispatchErrorFormat::screenshot,
				{ std::move(a_code), std::move(a_message), std::move(a_phase), a_retryable, a_admitted });
		};
		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks)
			return failure("dispatcher_unavailable", "SKSE task interface unavailable", "admission", true, false);

		using DispatchState = CSX::Api::MainThreadDispatchState<nlohmann::json>;
		auto state = std::make_shared<DispatchState>();
		try {
			tasks->AddTask([state, handle = std::move(handle)]() mutable {
				CSX::Api::EnterRuntimeMainThreadTask();
				if (!state->TryBegin())
					return;
				try {
					state->Complete(handle());
				} catch (...) {
					state->Fail(std::current_exception());
				}
			});
		} catch (...) {
			if (state->CancelIfQueued())
				return failure("dispatcher_failed", "SKSE task queue rejected the main-thread task", "admission", true, false);
		}

		const auto deadline = std::chrono::steady_clock::now() + kMainThreadTimeout;
		auto phase = state->WaitUntil(deadline);
		if (phase == DispatchState::Phase::queued) {
			if (state->CancelIfQueued())
				return failure("dispatcher_timeout", "main thread did not run within 5000ms", "admission", true, false);
			phase = state->WaitForTerminalUntil(deadline);
		}
		if (phase == DispatchState::Phase::running &&
			state->WaitForTerminalUntil(deadline) == DispatchState::Phase::running) {
			return {
				globals::features::screenshotFeature.MakeApiDispatchError(
					dispatchRequest,
					"dispatcher_admitted",
					"main-thread execution began but did not complete within 5000ms",
					false,
					{ { "executionMayComplete", true } }),
				CSX::Api::DevBenchDispatchFailure{
					"dispatcher_admitted",
					"main-thread execution began but did not complete within 5000ms",
					"execution",
					false,
					true },
			};
		}
		try {
			return { state->WaitForCompletion(), std::nullopt };
		} catch (const std::exception& e) {
			return failure("dispatcher_failed", e.what(), "execution", false, true);
		} catch (...) {
			return failure("dispatcher_failed", "unknown main-thread failure", "execution", false, true);
		}
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
