// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <atomic>
#include <unordered_set>

#include <chrono>

#include "common/scope_exit.h"
#include "common/scratch_buffer.h"
#include "core/core.h"
#include "core/hle/kernel/k_hardware_timer.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/kernel/svc/nexo_deadline_watch.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel::Svc {

std::atomic<u64> g_nexo_deadline_watch_until_ms{0};

void ArmNeXoDeadlineWatch(u64 duration_ms) {
    const auto now_ms = static_cast<u64>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
    g_nexo_deadline_watch_until_ms.store(now_ms + duration_ms, std::memory_order_relaxed);
}

bool IsNeXoDeadlineWatchActive() {
    const u64 until_ms = g_nexo_deadline_watch_until_ms.load(std::memory_order_relaxed);
    if (until_ms == 0) {
        return false;
    }
    const auto now_ms = static_cast<u64>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
    if (now_ms > until_ms) {
        g_nexo_deadline_watch_until_ms.store(0, std::memory_order_relaxed);
        return false;
    }
    return true;
}

namespace {
// [NeXo] Logs a WaitSynchronization timeout if the deadline watch is currently armed and
// the timeout looks like a plausible application-level deadline (not -1/infinite, not a
// sub-millisecond polling interval). See nexo_deadline_watch.h.
void MaybeLogNeXoDeadlineWatch(int64_t timeout_ns) {
    const u64 until_ms = g_nexo_deadline_watch_until_ms.load(std::memory_order_relaxed);
    if (until_ms == 0) {
        return;
    }
    const auto now_ms = static_cast<u64>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
    if (now_ms > until_ms) {
        g_nexo_deadline_watch_until_ms.store(0, std::memory_order_relaxed);
        return;
    }
    if (timeout_ns <= 0) {
        return; // -1 (infinite) or 0 (poll) -- not a deadline
    }
    constexpr int64_t MinInterestingNs = 50'000'000; // 50ms
    if (timeout_ns < MinInterestingNs) {
        return; // short polling-interval waits, not a deadline
    }
    LOG_INFO(Kernel_SVC, "[NeXo][DEADLINE-WATCH] WaitSynchronization timeout_ns={} ({:.3f} ms)",
             timeout_ns, static_cast<double>(timeout_ns) / 1'000'000.0);
}
} // namespace

/// Close a handle
Result CloseHandle(Core::System& system, Handle handle) {
    LOG_TRACE(Kernel_SVC, "Closing handle 0x{:08X}", handle);

    // Remove the handle.
    R_UNLESS(GetCurrentProcess(system.Kernel()).GetHandleTable().Remove(handle),
             ResultInvalidHandle);

    R_SUCCEED();
}

/// Clears the signaled state of an event or process.
Result ResetSignal(Core::System& system, Handle handle) {
    // Reduce log spam by only logging when handle is not found
    static std::unordered_set<Handle> logged_handles;
    bool should_log = logged_handles.find(handle) == logged_handles.end();

    if (should_log) {
        LOG_DEBUG(Kernel_SVC, "called handle 0x{:08X}", handle);
    }

    // Get the current handle table.
    const auto& handle_table = GetCurrentProcess(system.Kernel()).GetHandleTable();

    // Try to reset as readable event.
    {
        KScopedAutoObject readable_event = handle_table.GetObject<KReadableEvent>(handle);
        if (readable_event.IsNotNull()) {
            if (should_log) {
                logged_handles.erase(handle); // Remove from logged set if we find it
            }
            R_RETURN(readable_event->Reset());
        }
    }

    // Try to reset as process.
    {
        KScopedAutoObject process = handle_table.GetObject<KProcess>(handle);
        if (process.IsNotNull()) {
            if (should_log) {
                logged_handles.erase(handle); // Remove from logged set if we find it
            }
            R_RETURN(process->Reset());
        }
    }

    // Handle not found
    if (should_log) {
        LOG_WARNING(Kernel_SVC, "ResetSignal called with invalid handle 0x{:08X}", handle);
        logged_handles.insert(handle);
    }

    R_RETURN(ResultInvalidHandle);
}

/// Wait for the given handles to synchronize, timeout after the specified nanoseconds
Result WaitSynchronization(Core::System& system, int32_t* out_index, u64 user_handles,
                           int32_t num_handles, int64_t timeout_ns) {
    LOG_TRACE(Kernel_SVC, "called user_handles={:#x}, num_handles={}, timeout_ns={}", user_handles,
             num_handles, timeout_ns);
    MaybeLogNeXoDeadlineWatch(timeout_ns);

    // [NeXo][DIAG] The deadline watch only logs *finite* timeouts (see
    // MaybeLogNeXoDeadlineWatch) -- an infinite wait (timeout_ns <= 0) on a KEvent that never
    // gets signaled would be invisible to it. If gRPC-core's poller wakeup mechanism (Twenty-First
    // Update: no BSD wakeup-fd socket exists, so it's likely a raw KEvent) is stuck blocked
    // forever, this is where it would show up. Only active during the same armed window; logs
    // entry (so we know a real infinite wait started) and, separately, how long it actually took
    // wall-clock to resolve (so a wait that "completes" after 20+ real seconds is visible even
    // though it doesn't crash/hang the log).
    const bool diag_infinite_wait = timeout_ns <= 0 && IsNeXoDeadlineWatchActive();
    std::chrono::steady_clock::time_point diag_wait_start{};
    if (diag_infinite_wait) {
        diag_wait_start = std::chrono::steady_clock::now();
        LOG_INFO(Kernel_SVC, "[NeXo][DIAG] WaitSynchronization INFINITE wait starting, "
                             "num_handles={}",
                 num_handles);
    }

    // Ensure number of handles is valid.
    R_UNLESS(0 <= num_handles && num_handles <= Svc::ArgumentHandleCountMax, ResultOutOfRange);

    // Get the synchronization context.
    auto& kernel = system.Kernel();

    auto& handle_table = GetCurrentProcess(kernel).GetHandleTable();
    auto objs = GetCurrentThread(kernel).GetSynchronizationObjectBuffer();
    auto handles = GetCurrentThread(kernel).GetHandleBuffer();

    // Copy user handles.
    if (num_handles > 0) {
        // Get the handles.
        R_UNLESS(GetCurrentMemory(kernel).ReadBlock(user_handles, handles.data(),
                                                    sizeof(Handle) * num_handles),
                 ResultInvalidPointer);

        // Convert the handles to objects.
        R_UNLESS(handle_table.GetMultipleObjects<KSynchronizationObject>(
                     objs.data(), handles.data(), num_handles),
                 ResultInvalidHandle);

        for (auto i = 0; i < num_handles; ++i) {
            LOG_TRACE(Kernel_SVC, "  handle[{}]=0x{:X} type={}", i, handles[i],
                     objs[i]->GetTypeObj().GetName());
        }

        // [NeXo][DIAG] timeout_ns==0 EXACTLY is a real one-shot, non-blocking probe --
        // distinct from timeout_ns<0 (true infinite wait), which diag_infinite_wait above already
        // lumps together under "INFINITE" for logging purposes. This is exactly the shape of check
        // the shared nnSdk worker-thread bootstrap trampoline does on a handle read from its own
        // task-arg struct (offset 0x1b0, per this session's live disassembly) -- "if not already
        // signaled the instant this runs, bail immediately." Log the resolved object TYPE for any
        // single-handle, exactly-zero-timeout probe seen while the deadline watch is armed, so the
        // *kind* of kernel object gating that dispatch can be identified without more disassembly.
        if (timeout_ns == 0 && num_handles == 1 && IsNeXoDeadlineWatchActive()) {
            // [NeXo][DIAG] readable_event_obj lets this be matched directly against
            // CreateEvent/SignalEvent's own diagnostics (svc_event.cpp) -- see those comments for
            // why object address, not handle number, is the correlation key.
            LOG_INFO(Kernel_SVC,
                     "[NeXo][DIAG] WaitSynchronization ONE-SHOT probe (timeout=0) handle=0x{:X} "
                     "type={} readable_event_obj={}",
                     handles[0], objs[0]->GetTypeObj().GetName(), static_cast<const void*>(objs[0]));
        }
    }

    // Ensure handles are closed when we're done.
    SCOPE_EXIT {
        for (auto i = 0; i < num_handles; ++i) {
            objs[i]->Close();
        }
    };

    // Convert the timeout from nanoseconds to ticks.
    s64 timeout;
    if (timeout_ns > 0) {
        u64 ticks = kernel.HardwareTimer().GetTick();
        ticks += timeout_ns;
        ticks += 2;

        timeout = ticks;
    } else {
        timeout = timeout_ns;
    }

    // Wait on the objects.
    Result res = KSynchronizationObject::Wait(kernel, out_index, objs.data(), num_handles, timeout);
    LOG_TRACE(Kernel_SVC, "resolved out_index={}, result=0x{:X}", *out_index, res.raw);

    // [NeXo][DIAG] Companion to the ONE-SHOT probe log above -- same gating, prints whether
    // this specific probe found its handle already signaled (res==Success) or not (ResultTimedOut,
    // matching the Sixth Update's decompiled `w0 == 0xea01` bail branch).
    if (timeout_ns == 0 && num_handles == 1 && IsNeXoDeadlineWatchActive()) {
        LOG_INFO(Kernel_SVC, "[NeXo][DIAG] WaitSynchronization ONE-SHOT probe result=0x{:X}",
                 res.raw);
    }

    if (diag_infinite_wait) {
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - diag_wait_start)
                                     .count();
        LOG_INFO(Kernel_SVC,
                 "[NeXo][DIAG] WaitSynchronization INFINITE wait resolved after {}ms "
                 "wall-clock, out_index={}, result=0x{:X}",
                 elapsed_ms, *out_index, res.raw);
    }

    R_SUCCEED_IF(res == ResultSessionClosed);
    R_RETURN(res);
}

/// Resumes a thread waiting on WaitSynchronization
Result CancelSynchronization(Core::System& system, Handle handle) {
    LOG_TRACE(Kernel_SVC, "called handle=0x{:X}", handle);

    // Get the thread from its handle.
    KScopedAutoObject thread =
        GetCurrentProcess(system.Kernel()).GetHandleTable().GetObject<KThread>(handle);
    R_UNLESS(thread.IsNotNull(), ResultInvalidHandle);

    // Cancel the thread's wait.
    thread->WaitCancel();
    R_SUCCEED();
}

void SynchronizePreemptionState(Core::System& system) {
    auto& kernel = system.Kernel();

    // Lock the scheduler.
    KScopedSchedulerLock sl{kernel};

    // If the current thread is pinned, unpin it.
    KProcess* cur_process = GetCurrentProcessPointer(kernel);
    const auto core_id = GetCurrentCoreId(kernel);

    if (cur_process->GetPinnedThread(core_id) == GetCurrentThreadPointer(kernel)) {
        // Clear the current thread's interrupt flag.
        GetCurrentThread(kernel).ClearInterruptFlag();

        // Unpin the current thread.
        cur_process->UnpinCurrentThread();
    }
}

Result CloseHandle64(Core::System& system, Handle handle) {
    R_RETURN(CloseHandle(system, handle));
}

Result ResetSignal64(Core::System& system, Handle handle) {
    R_RETURN(ResetSignal(system, handle));
}

Result WaitSynchronization64(Core::System& system, int32_t* out_index, uint64_t handles,
                             int32_t num_handles, int64_t timeout_ns) {
    R_RETURN(WaitSynchronization(system, out_index, handles, num_handles, timeout_ns));
}

Result CancelSynchronization64(Core::System& system, Handle handle) {
    R_RETURN(CancelSynchronization(system, handle));
}

void SynchronizePreemptionState64(Core::System& system) {
    SynchronizePreemptionState(system);
}

Result CloseHandle64From32(Core::System& system, Handle handle) {
    R_RETURN(CloseHandle(system, handle));
}

Result ResetSignal64From32(Core::System& system, Handle handle) {
    R_RETURN(ResetSignal(system, handle));
}

Result WaitSynchronization64From32(Core::System& system, int32_t* out_index, uint32_t handles,
                                   int32_t num_handles, int64_t timeout_ns) {
    R_RETURN(WaitSynchronization(system, out_index, handles, num_handles, timeout_ns));
}

Result CancelSynchronization64From32(Core::System& system, Handle handle) {
    R_RETURN(CancelSynchronization(system, handle));
}

void SynchronizePreemptionState64From32(Core::System& system) {
    SynchronizePreemptionState(system);
}

} // namespace Kernel::Svc
