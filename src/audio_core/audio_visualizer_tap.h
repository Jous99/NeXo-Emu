// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <span>

#include "common/common_types.h"

namespace AudioCore {

// Lock-free bass/mid/treble readout; Feed() is real-time-safe (no locks, no allocation).
class AudioVisualizerTap {
public:
    struct Bands {
        float bass = 0.0f;
        float mid = 0.0f;
        float treble = 0.0f;
    };

    static void Feed(std::span<const s16> interleaved, u32 channels, u32 sample_rate);
    static Bands GetBands();

private:
    static std::atomic<float> bass_env;
    static std::atomic<float> mid_env;
    static std::atomic<float> treble_env;
};

} // namespace AudioCore
