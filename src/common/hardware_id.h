// SPDX-FileCopyrightText: Copyright 2026 NeXoEmu Project
// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from RaptorCitrus for NeXoEmu (Eden-based emulator)

#pragma once

#include <string>

namespace Common {

/// Returns a unique hardware identifier for this machine.
/// Used by Raptor Network for account binding and authentication.
const std::string& GetRaptorHardwareID();

} // namespace Common
