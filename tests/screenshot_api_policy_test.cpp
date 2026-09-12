#include "Features/ScreenshotApiPolicy.h"

#include <stdexcept>

using namespace std::chrono_literals;

int main()
{
	using namespace CSX::ScreenshotPolicy;
	for (const auto* unsafe : { "", ".", "..", "CON", "con.txt", "NUL.png", "COM1", "LPT9.log",
			 "trailing.", "trailing ", "stream:name", "star*", "slash/", "back\\slash", "caf\xC3\xA9" }) {
		if (IsSafeWindowsFilenameSegment(unsafe))
			throw std::runtime_error("unsafe filename segment was accepted");
	}
	if (!IsSafeWindowsFilenameSegment("frame_000001") ||
		FilenameCollisionKey("LEFT") != FilenameCollisionKey("left"))
		throw std::runtime_error("safe filename policy is invalid");
	if (!CanAdmitPendingOperations(MaximumPendingOperations - 1) ||
		CanAdmitPendingOperations(MaximumPendingOperations))
		throw std::runtime_error("pending-operation admission boundary is invalid");
	if (!CanAugmentOutputs(2, 2) || !CanAugmentOutputs(3, 1) ||
		CanAugmentOutputs(3, 2) || CanAugmentOutputs(4, 1) ||
		CanAugmentOutputs(0, 1))
		throw std::runtime_error("post-augmentation output boundary is invalid");
	if (ExpectedSequenceArtifacts(true) != 1 ||
		ExpectedSequenceArtifacts(false) != 0)
		throw std::runtime_error("sequence packaging artifact accounting is invalid");
	if (!CanStartWorker(true, false) || CanStartWorker(false, false) ||
		CanStartWorker(true, true) || CanStartWorker(false, true))
		throw std::runtime_error("worker admission is not terminal after close");
	if (RequiredEyeMask("left_eye") != 0x1 ||
		RequiredEyeMask("framed_left") != 0x1 ||
		RequiredEyeMask("right_eye") != 0x2 ||
		RequiredEyeMask("framed_right") != 0x2 ||
		RequiredEyeMask("side_by_side") != 0x3 ||
		(RequiredEyeMask("left_eye") | RequiredEyeMask("framed_left")) != 0x1 ||
		(RequiredEyeMask("left_eye") | RequiredEyeMask("right_eye")) != 0x3)
		throw std::runtime_error("requested output views produce an invalid eye mask");
	if (SelectDispatchClass(true, true, true) != DispatchClass::Manual ||
		SelectDispatchClass(true, true, false) != DispatchClass::Sequence ||
		SelectDispatchClass(true, false, false) != DispatchClass::Manual ||
		SelectDispatchClass(false, true, true) != DispatchClass::Sequence ||
		SelectDispatchClass(false, false, true) != DispatchClass::None)
		throw std::runtime_error("fair dispatcher selection is invalid");
	DispatchArbitration arbitration;
	// A busy source can free up on the same Present parity on every frame.
	// Failed manual attempts must not hand each newly available slot away.
	for (int busyPresent = 0; busyPresent != 6; ++busyPresent) {
		const auto selected = arbitration.Select(true, true);
		if (selected != DispatchClass::Manual)
			throw std::runtime_error("a capacity retry lost the pending manual capture's turn");
		arbitration.FinishAttempt(selected, true);
	}
	arbitration.FinishAttempt(DispatchClass::Manual, false);
	if (arbitration.Select(true, true) != DispatchClass::Sequence)
		throw std::runtime_error("a completed manual turn starved the sequence queue");
	arbitration.FinishAttempt(DispatchClass::Sequence, false);
	if (arbitration.Select(true, true) != DispatchClass::Manual)
		throw std::runtime_error("a completed sequence turn starved the manual queue");
	if (ResolveBusyDispatch(true, false, false) != BusyDispatchDisposition::Drop ||
		ResolveBusyDispatch(true, false, true) != BusyDispatchDisposition::Drop)
		throw std::runtime_error("sequence backpressure delayed a missed slot instead of dropping it");
	if (ResolveBusyDispatch(false, false, false) != BusyDispatchDisposition::Retry ||
		ResolveBusyDispatch(false, false, true) != BusyDispatchDisposition::Fail)
		throw std::runtime_error("manual capture retries ignored their admission deadline");
	const auto now = std::chrono::steady_clock::time_point(10s);
	if (!HasDispatchDeadlineElapsed(now, now) ||
		!HasDispatchDeadlineElapsed(now, now - 1ms) ||
		HasDispatchDeadlineElapsed(now, now + 1ms))
		throw std::runtime_error("queued dispatch deadline boundary is invalid");
	if (!CanAcceptSequenceCommand(false) || CanAcceptSequenceCommand(true))
		throw std::runtime_error("sequence command admission crossed the finalization boundary");
	if (ResolveSequenceTerminalOutcome(SequenceTerminationIntent::Stop, 0, false, false, false, false) != "stopped" ||
		ResolveSequenceTerminalOutcome(SequenceTerminationIntent::Cancel, 1, false, false, false, false) != "cancelled_partial" ||
		ResolveSequenceTerminalOutcome(SequenceTerminationIntent::PolicyAbort, 0, false, true, false, false) != "failed" ||
		ResolveSequenceTerminalOutcome(SequenceTerminationIntent::PolicyAbort, 1, false, true, false, false) != "failed_partial" ||
		ResolveSequenceTerminalOutcome(SequenceTerminationIntent::Natural, 1, false, true, false, false) != "completed_with_warnings")
		throw std::runtime_error("sequence terminal outcome policy is invalid");
	for (const bool sequenceFrame : { false, true }) {
		for (const bool deadlineReached : { false, true }) {
			if (ResolveBusyDispatch(sequenceFrame, true, deadlineReached) != BusyDispatchDisposition::Cancel)
				throw std::runtime_error("cancellation during dispatch abandoned a nonterminal request");
		}
	}
	if (!IsSamePublication(7, 0x1000, 7, 0x1000) ||
		IsSamePublication(7, 0x1000, 8, 0x1000) ||
		IsSamePublication(7, 0x1000, 7, 0x2000) ||
		IsSamePublication(0, 0x1000, 0, 0x1000))
		throw std::runtime_error("stereo publication coherence is invalid");
	if (ResolveActualOutputView("source_native", true, false, false) != "source_native" ||
		!ResolveActualOutputView("left_eye", true, false, false).empty() ||
		ResolveActualOutputView("framed_combined", false, true, false) != "framed_left" ||
		ResolveActualOutputView("framed_combined", false, true, true) != "framed_right" ||
		ResolveActualOutputView("side_by_side", false, false, false) != "side_by_side")
		throw std::runtime_error("actual output provenance policy is invalid");
	const std::filesystem::path captureRoot = "C:/Users/test/Pictures/Community Shaders";
	if (!IsContainedPath(captureRoot, captureRoot) ||
		!IsContainedPath(captureRoot, captureRoot / "Screenshots") ||
		IsContainedPath(captureRoot, captureRoot.parent_path() / "Other") ||
		IsContainedPath(captureRoot, captureRoot / ".." / "Other"))
		throw std::runtime_error("settings-default containment policy is invalid");
	if (!IsWallClockScheduleWithinLimit(0, 1000, 3601) ||
		IsWallClockScheduleWithinLimit(1, 1000, 3601))
		throw std::runtime_error("wall-clock sequence limit is invalid");
	if (!IsGameFrameScheduleWithinLimit(0, 60, 3601) ||
		IsGameFrameScheduleWithinLimit(1, 60, 3601))
		throw std::runtime_error("game-frame sequence limit is invalid");
	return 0;
}
