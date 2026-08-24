// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <span>
#include <vector>
#include <SDL.h>

#include "audio_core/common/common.h"
#include "audio_core/sink/sdl2_sink.h"
#include "audio_core/sink/sink_stream.h"
#include "common/logging.h"
#include "common/scope_exit.h"
#include "core/core.h"

namespace AudioCore::Sink {

class SDLSinkStream final : public SinkStream {
public:
    SDLSinkStream(u32 device_channels_, u32 system_channels_, const std::string& output_device,
                  const std::string& input_device, StreamType type_, Core::System& system_)
        : SinkStream{system_, type_} {
        system_channels = system_channels_;
        device_channels = device_channels_;

        SDL_AudioSpec spec;
        spec.freq = TargetSampleRate;
        spec.channels = static_cast<int>(device_channels);
        spec.format = SDL_AUDIO_S16;

        std::string device_name{output_device};
        bool capture{false};
        if (type == StreamType::In) {
            device_name = input_device;
            capture = true;
        }

        SDL_AudioDeviceID devid = capture ? SDL_AUDIO_DEVICE_DEFAULT_RECORDING : SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
        if (!device_name.empty()) {
            int count = 0;
            SDL_AudioDeviceID* devices = capture ? SDL_GetAudioRecordingDevices(&count) : SDL_GetAudioPlaybackDevices(&count);
            if (devices) {
                for (int i = 0; i < count; ++i) {
                    if (const char* name = SDL_GetAudioDeviceName(devices[i])) {
                        if (device_name == name) {
                            devid = devices[i];
                            break;
                        }
                    }
                }
                SDL_free(devices);
            }
        }

        stream = SDL_OpenAudioDeviceStream(devid, &spec, &SDLSinkStream::DataCallback, this);
        if (!stream) {
            LOG_CRITICAL(Audio_Sink, "Error opening SDL audio device: {}", SDL_GetError());
            return;
        }

        SDL_AudioSpec device_spec;
        int sample_frames = 0;
        SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(stream), &device_spec, &sample_frames);

        LOG_INFO(Service_Audio,
                 "Opening SDL stream {} with: rate {} channels {} (system channels {}) "
                 " samples {}",
                 (void*)stream, device_spec.freq, device_spec.channels, system_channels, sample_frames);
    }

    ~SDLSinkStream() override {
        LOG_DEBUG(Service_Audio, "Destructing SDL stream {}", name);
        Finalize();
    }

    void Finalize() override {
        if (!stream) {
            return;
        }

        Stop();
        SDL_ClearAudioStream(stream);
        SDL_DestroyAudioStream(stream);
        stream = nullptr;
    }

    void Start(bool resume = false) override {
        if (!stream || !paused) {
            return;
        }

        paused = false;
        SDL_ResumeAudioStreamDevice(stream);
    }

    void Stop() override {
        if (!stream || paused) {
            return;
        }
        SignalPause();
        SDL_PauseAudioStreamDevice(stream);
    }

private:
    static void DataCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
        auto* impl = static_cast<SDLSinkStream*>(userdata);
        if (!impl) {
            return;
        }

        if (impl->type == StreamType::In) {
            std::vector<s16> buffer(additional_amount / sizeof(s16));
            int read_bytes = SDL_GetAudioStreamData(stream, buffer.data(), additional_amount);
            if (read_bytes > 0) {
                const std::size_t num_channels = impl->GetDeviceChannels();
                const std::size_t frame_size = num_channels;
                const std::size_t num_frames{static_cast<std::size_t>(read_bytes) / num_channels / sizeof(s16)};
                std::span<const s16> input_buffer{buffer.data(), num_frames * frame_size};
                impl->ProcessAudioIn(input_buffer, num_frames);
            }
        } else {
            const std::size_t num_channels = impl->GetDeviceChannels();
            const std::size_t frame_size = num_channels;
            const std::size_t num_frames{static_cast<std::size_t>(additional_amount) / num_channels / sizeof(s16)};
            std::vector<s16> buffer(num_frames * frame_size);
            std::span<s16> output_buffer{buffer.data(), num_frames * frame_size};
            impl->ProcessAudioOutAndRender(output_buffer, num_frames);
            SDL_PutAudioStreamData(stream, buffer.data(), additional_amount);
        }
    }

    SDL_AudioStream* stream{nullptr};
};

SDLSink::SDLSink(std::string_view target_device_name) {
    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            LOG_CRITICAL(Audio_Sink, "SDL_InitSubSystem audio failed: {}", SDL_GetError());
            return;
        }
    }

    if (target_device_name != auto_device_name && !target_device_name.empty()) {
        output_device = target_device_name;
    } else {
        output_device.clear();
    }

    device_channels = 2;
}

SDLSink::~SDLSink() = default;

SinkStream* SDLSink::AcquireSinkStream(Core::System& system, u32 system_channels_,
                                       const std::string&, StreamType type) {
    system_channels = system_channels_;
    SinkStreamPtr& stream = sink_streams.emplace_back(std::make_unique<SDLSinkStream>(
        device_channels, system_channels, output_device, input_device, type, system));
    return stream.get();
}

void SDLSink::CloseStream(SinkStream* stream) {
    for (size_t i = 0; i < sink_streams.size(); i++) {
        if (sink_streams[i].get() == stream) {
            sink_streams[i].reset();
            sink_streams.erase(sink_streams.begin() + i);
            break;
        }
    }
}

void SDLSink::CloseStreams() {
    sink_streams.clear();
}

f32 SDLSink::GetDeviceVolume() const {
    if (sink_streams.empty() || !sink_streams[0]) {
        return 1.0f;
    }

    return sink_streams[0]->GetDeviceVolume();
}

void SDLSink::SetDeviceVolume(f32 volume) {
    for (auto& stream : sink_streams) {
        stream->SetDeviceVolume(volume);
    }
}

void SDLSink::SetSystemVolume(f32 volume) {
    for (auto& stream : sink_streams) {
        stream->SetSystemVolume(volume);
    }
}

std::vector<std::string> ListSDLSinkDevices(bool capture) {
    std::vector<std::string> device_list;

    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            LOG_CRITICAL(Audio_Sink, "SDL_InitSubSystem audio failed: {}", SDL_GetError());
            return {};
        }
    }

    int count = 0;
    SDL_AudioDeviceID* devices = capture ? SDL_GetAudioRecordingDevices(&count) : SDL_GetAudioPlaybackDevices(&count);
    if (devices) {
        for (int i = 0; i < count; ++i) {
            if (const char* name = SDL_GetAudioDeviceName(devices[i])) {
                device_list.emplace_back(name);
            }
        }
        SDL_free(devices);
    }

    return device_list;
}

bool IsSDLSuitable() {
#if !defined(HAVE_SDL2)
    return false;
#else
    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            LOG_ERROR(Audio_Sink, "SDL failed to init, it is not suitable. Error: {}",
                      SDL_GetError());
            return false;
        }
    }

    SDL_AudioSpec spec;
    spec.freq = TargetSampleRate;
    spec.channels = 2;
    spec.format = SDL_AUDIO_S16;

    auto stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!stream) {
        LOG_ERROR(Audio_Sink, "SDL failed to open a device, it is not suitable. Error: {}",
                  SDL_GetError());
        return false;
    }

    SDL_DestroyAudioStream(stream);
    return true;
#endif
}

} // namespace AudioCore::Sink
