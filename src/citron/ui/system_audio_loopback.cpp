// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "citron/ui/system_audio_loopback.h"

#if defined(_WIN32)

#include <algorithm>
#include <atomic>
#include <cstring>
#include <span>
#include <thread>
#include <vector>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <windows.h>

#include "audio_core/audio_visualizer_tap.h"
#include "common/common_types.h"
#include "common/logging.h"

namespace SystemAudioLoopback {

namespace {

std::atomic<bool> g_running{false};
std::thread g_thread;

void ConvertAndFeed(const BYTE* data, UINT32 num_frames, const WAVEFORMATEX* fmt) {
    static thread_local std::vector<s16> pcm;
    const std::size_t sample_count = static_cast<std::size_t>(num_frames) * fmt->nChannels;
    pcm.resize(sample_count);

    if (fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
        (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE && fmt->wBitsPerSample == 32)) {
        const float* src = reinterpret_cast<const float*>(data);
        for (std::size_t i = 0; i < sample_count; ++i) {
            pcm[i] = static_cast<s16>(std::clamp(src[i], -1.0f, 1.0f) * 32767.0f);
        }
    } else if (fmt->wBitsPerSample == 16) {
        std::memcpy(pcm.data(), data, sample_count * sizeof(s16));
    } else {
        return;
    }

    AudioCore::AudioVisualizerTap::Feed(std::span<const s16>(pcm.data(), pcm.size()),
                                        fmt->nChannels, fmt->nSamplesPerSec);
}

void RunThread() {
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
        LOG_WARNING(Frontend, "SystemAudioLoopback: CoInitializeEx failed");
        return;
    }

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* audio_client = nullptr;
    IAudioCaptureClient* capture_client = nullptr;
    WAVEFORMATEX* format = nullptr;

    const auto cleanup = [&] {
        if (capture_client) {
            capture_client->Release();
        }
        if (audio_client) {
            audio_client->Release();
        }
        if (device) {
            device->Release();
        }
        if (enumerator) {
            enumerator->Release();
        }
        if (format) {
            CoTaskMemFree(format);
        }
        CoUninitialize();
    };

    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator),
                                reinterpret_cast<void**>(&enumerator))) ||
        FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device)) ||
        FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                reinterpret_cast<void**>(&audio_client))) ||
        FAILED(audio_client->GetMixFormat(&format))) {
        LOG_WARNING(Frontend, "SystemAudioLoopback: could not reach the default render device");
        cleanup();
        return;
    }

    constexpr REFERENCE_TIME kBufferDuration = 10'000'000; // 1 second, 100ns units
    if (FAILED(audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
                                        kBufferDuration, 0, format, nullptr)) ||
        FAILED(audio_client->GetService(__uuidof(IAudioCaptureClient),
                                        reinterpret_cast<void**>(&capture_client))) ||
        FAILED(audio_client->Start())) {
        LOG_WARNING(Frontend, "SystemAudioLoopback: WASAPI loopback stream setup failed");
        cleanup();
        return;
    }

    LOG_INFO(Frontend, "SystemAudioLoopback: capturing default render device via WASAPI loopback");

    while (g_running.load(std::memory_order_relaxed)) {
        UINT32 packet_size = 0;
        if (FAILED(capture_client->GetNextPacketSize(&packet_size))) {
            break;
        }
        while (packet_size != 0) {
            BYTE* data = nullptr;
            UINT32 num_frames = 0;
            DWORD flags = 0;
            if (FAILED(capture_client->GetBuffer(&data, &num_frames, &flags, nullptr, nullptr))) {
                packet_size = 0;
                break;
            }
            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && num_frames > 0) {
                ConvertAndFeed(data, num_frames, format);
            }
            capture_client->ReleaseBuffer(num_frames);
            if (FAILED(capture_client->GetNextPacketSize(&packet_size))) {
                packet_size = 0;
            }
        }
        Sleep(10);
    }

    audio_client->Stop();
    cleanup();
}

} // namespace

void Start() {
    if (g_running.exchange(true)) {
        return;
    }
    g_thread = std::thread(RunThread);
}

void Stop() {
    if (!g_running.exchange(false)) {
        return;
    }
    if (g_thread.joinable()) {
        g_thread.join();
    }
}

} // namespace SystemAudioLoopback

#elif defined(CITRON_ENABLE_PULSE_LOOPBACK)

#include <atomic>
#include <span>
#include <string>
#include <thread>
#include <pulse/pulseaudio.h>

#include "audio_core/audio_visualizer_tap.h"
#include "common/common_types.h"
#include "common/logging.h"

namespace SystemAudioLoopback {

namespace {

std::atomic<bool> g_running{false};
std::thread g_thread;
pa_mainloop* g_mainloop = nullptr;

// Runs on the PulseAudio thread whenever captured audio is ready.
void StreamReadCallback(pa_stream* stream, size_t length, void* /*userdata*/) {
    const void* data = nullptr;
    if (pa_stream_peek(stream, &data, &length) < 0) {
        return;
    }
    if (data && length > 0) {
        AudioCore::AudioVisualizerTap::Feed(
            std::span<const s16>(reinterpret_cast<const s16*>(data), length / sizeof(s16)), 2,
            48000);
    }
    if (length > 0) {
        pa_stream_drop(stream);
    }
}

void OpenMonitorStream(pa_context* ctx, const std::string& monitor_name) {
    pa_sample_spec spec{};
    spec.format = PA_SAMPLE_S16LE;
    spec.channels = 2;
    spec.rate = 48000;

    pa_stream* stream = pa_stream_new(ctx, "citron reactive backdrop", &spec, nullptr);
    if (!stream) {
        LOG_WARNING(Frontend, "SystemAudioLoopback: pa_stream_new failed");
        return;
    }
    pa_stream_set_read_callback(stream, StreamReadCallback, nullptr);

    pa_buffer_attr attr{};
    attr.maxlength = static_cast<uint32_t>(-1);
    attr.fragsize = 2048; // ~10ms at 48kHz stereo s16

    if (pa_stream_connect_record(stream, monitor_name.c_str(), &attr,
                                 PA_STREAM_ADJUST_LATENCY) < 0) {
        LOG_WARNING(Frontend, "SystemAudioLoopback: pa_stream_connect_record('{}') failed",
                   monitor_name);
        pa_stream_unref(stream);
        return;
    }
    LOG_INFO(Frontend, "SystemAudioLoopback: capturing '{}'", monitor_name);
}

// The monitor source isn't exposed directly; it's the default sink's name plus ".monitor".
void ServerInfoCallback(pa_context* ctx, const pa_server_info* info, void* /*userdata*/) {
    if (!info || !info->default_sink_name) {
        LOG_WARNING(Frontend, "SystemAudioLoopback: no default sink reported");
        return;
    }
    OpenMonitorStream(ctx, std::string(info->default_sink_name) + ".monitor");
}

void ContextStateCallback(pa_context* ctx, void* /*userdata*/) {
    switch (pa_context_get_state(ctx)) {
    case PA_CONTEXT_READY: {
        pa_operation* op = pa_context_get_server_info(ctx, ServerInfoCallback, nullptr);
        if (op) {
            pa_operation_unref(op);
        }
        break;
    }
    case PA_CONTEXT_FAILED:
        LOG_WARNING(Frontend, "SystemAudioLoopback: PulseAudio context failed");
        break;
    default:
        break;
    }
}

void RunThread() {
    g_mainloop = pa_mainloop_new();
    pa_mainloop_api* api = pa_mainloop_get_api(g_mainloop);
    pa_context* ctx = pa_context_new(api, "citron");
    pa_context_set_state_callback(ctx, ContextStateCallback, nullptr);

    if (pa_context_connect(ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
        LOG_WARNING(Frontend, "SystemAudioLoopback: pa_context_connect failed");
    } else {
        int ret = 0;
        while (g_running.load(std::memory_order_relaxed)) {
            pa_mainloop_iterate(g_mainloop, 1, &ret);
        }
    }

    pa_context_disconnect(ctx);
    pa_context_unref(ctx);
    pa_mainloop_free(g_mainloop);
    g_mainloop = nullptr;
}

} // namespace

void Start() {
    if (g_running.exchange(true)) {
        return;
    }
    g_thread = std::thread(RunThread);
}

void Stop() {
    if (!g_running.exchange(false)) {
        return;
    }
    if (g_mainloop) {
        pa_mainloop_wakeup(g_mainloop);
    }
    if (g_thread.joinable()) {
        g_thread.join();
    }
}

} // namespace SystemAudioLoopback

#else

namespace SystemAudioLoopback {
void Start() {}
void Stop() {}
} // namespace SystemAudioLoopback

#endif
