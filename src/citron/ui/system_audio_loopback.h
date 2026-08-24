// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace SystemAudioLoopback {

// Captures system audio (WASAPI loopback on Windows, PulseAudio on Linux) into AudioVisualizerTap.
void Start();
void Stop();

} // namespace SystemAudioLoopback
