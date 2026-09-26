#include "Api/MainThreadDispatchPolicy.h"
#include "Api/MainThreadDispatchState.h"

#include <atomic>
#include <chrono>
#include <latch>
#include <stdexcept>
#include <string_view>
#include <thread>

using State = CSX::Api::MainThreadDispatchState<int>;
using namespace std::chrono_literals;

int main()
{
	{
		auto state = std::make_shared<State>();
		std::atomic_bool mutated = false;
		if (state->WaitUntil(std::chrono::steady_clock::now()) != State::Phase::queued ||
			!state->CancelIfQueued()) {
			throw std::runtime_error("queued deadline must cancel before execution");
		}
		std::thread worker([&] {
			if (!state->TryBegin())
				return;
			mutated.store(true, std::memory_order_release);
			state->Complete(1);
		});
		worker.join();
		if (mutated.load(std::memory_order_acquire) || state->TryBegin())
			throw std::runtime_error("cancel must make queued work permanently ineligible");
	}
	{
		auto state = std::make_shared<State>();
		std::atomic_bool mutated = false;
		std::latch admitted{ 1 };
		std::latch release{ 1 };
		std::thread worker([&] {
			if (!state->TryBegin())
				return;
			admitted.count_down();
			release.wait();
			mutated.store(true, std::memory_order_release);
			state->Complete(42);
		});
		admitted.wait();
		if (state->WaitUntil(std::chrono::steady_clock::now() + 1s) == State::Phase::queued)
			throw std::runtime_error("admission was not published");
		if (state->CancelIfQueued())
			throw std::runtime_error("cancellation must lose after admission");
		release.count_down();
		if (state->WaitForCompletion() != 42 || !mutated.load(std::memory_order_acquire))
			throw std::runtime_error("admitted work did not publish its response");
		worker.join();
	}
	for (int iteration = 0; iteration < 128; ++iteration) {
		auto state = std::make_shared<State>();
		std::latch ready{ 2 };
		std::latch start{ 1 };
		std::atomic_bool admitted = false;
		std::atomic_bool cancelled = false;
		std::thread runner([&] {
			ready.count_down();
			start.wait();
			admitted.store(state->TryBegin(), std::memory_order_release);
		});
		std::thread canceller([&] {
			ready.count_down();
			start.wait();
			cancelled.store(state->CancelIfQueued(), std::memory_order_release);
		});
		ready.wait();
		start.count_down();
		runner.join();
		canceller.join();
		const bool admissionWon = admitted.load(std::memory_order_acquire);
		const bool cancellationWon = cancelled.load(std::memory_order_acquire);
		if (admissionWon == cancellationWon)
			throw std::runtime_error("admission and cancellation did not have exactly one winner");
		if (admissionWon) {
			state->Complete(iteration);
			if (state->WaitForCompletion() != iteration || state->CancelIfQueued())
				throw std::runtime_error("admitted race winner did not remain authoritative");
		} else if (state->TryBegin()) {
			throw std::runtime_error("cancelled race loser was admitted later");
		}
	}
	{
		State state;
		if (state.WaitUntil(std::chrono::steady_clock::now()) != State::Phase::queued ||
			!state.TryBegin() || state.CancelIfQueued() || state.TryBegin()) {
			throw std::runtime_error("admission must win cancellation after the deadline observation");
		}
		state.Complete(7);
		if (state.WaitForCompletion() != 7 || state.TryBegin())
			throw std::runtime_error("completed work must not be admitted again");
	}
	{
		auto state = std::make_shared<State>();
		if (!state->TryBegin())
			throw std::runtime_error("failure case was not admitted");
		state->Fail(std::make_exception_ptr(std::runtime_error("expected failure")));
		try {
			(void)state->WaitForCompletion();
			throw std::runtime_error("failed work returned a result");
		} catch (const std::runtime_error& error) {
			if (std::string_view(error.what()) != "expected failure")
				throw;
		}
	}
	return 0;
}
