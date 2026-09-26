#pragma once

#include "Api/MainThreadDispatchState.h"
#include "Api/RuntimeThreadAffinity.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace CSX::Api
{
	enum class DevBenchDispatchErrorFormat
	{
		standard,
		profiler,
		screenshot,
	};

	struct DevBenchDispatchFailure
	{
		std::string code;
		std::string message;
		std::string phase;
		bool retryable = false;
		bool admitted = false;
	};

	struct DevBenchMainThreadResult
	{
		nlohmann::json response;
		std::optional<DevBenchDispatchFailure> failure;
	};

	inline DevBenchMainThreadResult MakeDevBenchDispatchFailure(
		DevBenchDispatchErrorFormat a_format,
		DevBenchDispatchFailure a_failure)
	{
		using json = nlohmann::json;
		json response;
		if (a_format == DevBenchDispatchErrorFormat::profiler) {
			response = { { "_dispatchError", a_failure.message } };
		} else if (a_format == DevBenchDispatchErrorFormat::screenshot) {
			response = {
				{ "ok", false },
				{ "error", {
							   { "code", a_failure.code },
							   { "message", a_failure.message },
							   { "phase", a_failure.phase },
							   { "retryable", a_failure.retryable },
							   { "admitted", a_failure.admitted },
						   } },
			};
		} else {
			response = { { "error", a_failure.admitted ? "main-thread task failed" : a_failure.message } };
			if (a_failure.admitted && a_failure.message != "unknown main-thread failure")
				response["detail"] = a_failure.message;
		}
		return { std::move(response), std::move(a_failure) };
	}

	// Keep the existing wire error shapes while sharing the admission protocol.
	template <class TaskInterface>
	DevBenchMainThreadResult RunDevBenchMainThreadTask(
		TaskInterface* a_tasks,
		std::function<nlohmann::json()> a_run,
		DevBenchDispatchErrorFormat a_format = DevBenchDispatchErrorFormat::standard,
		std::chrono::milliseconds a_timeout = std::chrono::milliseconds(5000))
	{
		auto failure = [a_format](
						   std::string a_code,
						   std::string a_message,
						   std::string a_phase,
						   bool a_retryable,
						   bool a_admitted) {
			return MakeDevBenchDispatchFailure(
				a_format,
				{
					std::move(a_code),
					std::move(a_message),
					std::move(a_phase),
					a_retryable,
					a_admitted,
				});
		};
		if (!a_tasks)
			return failure("dispatcher_unavailable", "SKSE task interface unavailable", "admission", true, false);
		try {
			return {
				DispatchMainThreadTask(
					[a_tasks](auto a_task) { a_tasks->AddTask(std::move(a_task)); },
					[run = std::move(a_run)] {
						EnterRuntimeMainThreadTask();
						return run();
					},
					a_timeout),
				std::nullopt,
			};
		} catch (const MainThreadDispatchRejected&) {
			return failure("dispatcher_failed", "SKSE task queue rejected the main-thread task", "admission", true, false);
		} catch (const MainThreadDispatchTimeout&) {
			const auto message = "main thread did not run within " + std::to_string(a_timeout.count()) + "ms";
			return failure("dispatcher_timeout", message, "admission", true, false);
		} catch (const std::exception& e) {
			return failure("dispatcher_failed", e.what(), "execution", false, true);
		} catch (...) {
			return failure("dispatcher_failed", "unknown main-thread failure", "execution", false, true);
		}
	}
}
