// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "core/hle/service/os/multi_wait_holder.h"

namespace Service {
class Process;
}

namespace Service::AM {

struct Applet;

class ProcessHolder : public MultiWaitHolder, public Common::IntrusiveListBaseNode<ProcessHolder> {
public:
    explicit ProcessHolder(std::shared_ptr<Applet> applet, Process& process);
    ~ProcessHolder();

    Applet& GetApplet() const {
        return *m_applet;
    }

    Process& GetProcess() const {
        return m_process;
    }

private:
    // Co-owns the Applet (rather than just referencing it) so that WindowSystem dropping its own
    // shared_ptr (e.g. WindowSystem::PruneTerminatedAppletsLocked erasing a terminated applet from
    // m_applets) can't free the Applet -- and the Process it owns -- while EventObserver's thread
    // still has this holder linked and may dereference it.
    std::shared_ptr<Applet> m_applet;
    Process& m_process;
};

} // namespace Service::AM
