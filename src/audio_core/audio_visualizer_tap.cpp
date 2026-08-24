// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cmath>

#include "audio_core/audio_visualizer_tap.h"

namespace AudioCore {

std::atomic<float> AudioVisualizerTap::bass_env{0.0f};
std::atomic<float> AudioVisualizerTap::mid_env{0.0f};
std::atomic<float> AudioVisualizerTap::treble_env{0.0f};

namespace {

// RBJ constant-skirt-gain bandpass, Direct Form II Transposed.
struct Biquad {
    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void SetBandpass(float center_hz, float q, float sample_rate) {
        const float w0 = 2.0f * 3.14159265358979323846f * center_hz / sample_rate;
        const float alpha = std::sin(w0) / (2.0f * q);
        const float a0 = 1.0f + alpha;
        b0 = alpha / a0;
        b1 = 0.0f;
        b2 = -alpha / a0;
        a1 = (-2.0f * std::cos(w0)) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    float Process(float x) {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// Asymmetric one-pole follower: fast attack, slow release, so a hit pops instead of creeping in.
struct Envelope {
    float value = 0.0f;
    float attack = 0.0f;
    float release = 0.0f;

    void SetTimes(float attack_seconds, float release_seconds, float sample_rate) {
        attack = std::exp(-1.0f / (attack_seconds * sample_rate));
        release = std::exp(-1.0f / (release_seconds * sample_rate));
    }

    float Process(float rectified) {
        const float coef = rectified > value ? attack : release;
        value = coef * value + (1.0f - coef) * rectified;
        return value;
    }
};

struct Band {
    Biquad filter;
    Envelope envelope;
    float configured_rate = 0.0f;

    void EnsureConfigured(float center_hz, float q, float sample_rate) {
        if (configured_rate == sample_rate) {
            return;
        }
        filter.SetBandpass(center_hz, q, sample_rate);
        envelope.SetTimes(0.010f, 0.300f, sample_rate);
        configured_rate = sample_rate;
    }
};

// Single-writer assumption: only one Render-type SinkStream feeds this at a time in practice.
Band& BassBand() {
    static Band band;
    return band;
}
Band& MidBand() {
    static Band band;
    return band;
}
Band& TrebleBand() {
    static Band band;
    return band;
}

constexpr float kGain = 6.0f;

} // namespace

void AudioVisualizerTap::Feed(std::span<const s16> interleaved, u32 channels, u32 sample_rate) {
    if (interleaved.empty() || channels == 0 || sample_rate == 0) {
        return;
    }

    auto& bass = BassBand();
    auto& mid = MidBand();
    auto& treble = TrebleBand();
    const float rate = static_cast<float>(sample_rate);
    bass.EnsureConfigured(80.0f, 0.9f, rate);
    mid.EnsureConfigured(600.0f, 0.5f, rate);
    treble.EnsureConfigured(4000.0f, 0.6f, rate);

    const std::size_t frames = interleaved.size() / channels;
    float bass_out = bass_env.load(std::memory_order_relaxed);
    float mid_out = mid_env.load(std::memory_order_relaxed);
    float treble_out = treble_env.load(std::memory_order_relaxed);

    for (std::size_t frame = 0; frame < frames; ++frame) {
        float mono = 0.0f;
        const std::size_t base = frame * channels;
        for (u32 c = 0; c < channels; ++c) {
            mono += interleaved[base + c];
        }
        mono = (mono / static_cast<float>(channels)) / 32768.0f;

        bass_out = bass.envelope.Process(std::fabs(bass.filter.Process(mono)));
        mid_out = mid.envelope.Process(std::fabs(mid.filter.Process(mono)));
        treble_out = treble.envelope.Process(std::fabs(treble.filter.Process(mono)));
    }

    bass_env.store(std::clamp(bass_out * kGain, 0.0f, 1.0f), std::memory_order_relaxed);
    mid_env.store(std::clamp(mid_out * kGain, 0.0f, 1.0f), std::memory_order_relaxed);
    treble_env.store(std::clamp(treble_out * kGain, 0.0f, 1.0f), std::memory_order_relaxed);
}

AudioVisualizerTap::Bands AudioVisualizerTap::GetBands() {
    return Bands{
        .bass = bass_env.load(std::memory_order_relaxed),
        .mid = mid_env.load(std::memory_order_relaxed),
        .treble = treble_env.load(std::memory_order_relaxed),
    };
}

} // namespace AudioCore
