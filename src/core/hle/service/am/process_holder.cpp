// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/k_process.h"
#include "core/hle/service/am/process_holder.h"
#include "core/hle/service/os/process.h"

namespace Service::AM {

ProcessHolder::ProcessHolder(std::shared_ptr<Applet> applet, Process& process)
    : MultiWaitHolder(process.GetHandle()), m_applet(std::move(applet)), m_process(process) {}

ProcessHolder::~ProcessHolder() = default;

} // namespace Service::AM
