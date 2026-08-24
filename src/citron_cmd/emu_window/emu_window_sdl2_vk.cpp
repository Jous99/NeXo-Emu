// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <memory>
#include <string>

#include <fmt/format.h>

#include "common/logging.h"
#include "common/scm_rev.h"
#include "video_core/renderer_vulkan/renderer_vulkan.h"
#include "citron_cmd/emu_window/emu_window_sdl2_vk.h"

#include <SDL.h>
#ifdef __APPLE__
#include <SDL_metal.h>
#endif

EmuWindow_SDL2_VK::EmuWindow_SDL2_VK(InputCommon::InputSubsystem* input_subsystem_,
                                     Core::System& system_, bool fullscreen)
    : EmuWindow_SDL2{input_subsystem_, system_} {
    const std::string window_title = fmt::format("citron {} | {}-{} (Vulkan)", Common::g_build_name,
                                                 Common::g_scm_branch, Common::g_scm_desc);
    render_window =
        SDL_CreateWindow(window_title.c_str(),
                         Layout::ScreenUndocked::Width, Layout::ScreenUndocked::Height,
                         SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_VULKAN);

    if (render_window == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to create SDL window: {}", SDL_GetError());
        std::exit(EXIT_FAILURE);
    }

    SetWindowIcon();

    if (fullscreen) {
        Fullscreen();
        ShowCursor(false);
    }

    SDL_PropertiesID props = SDL_GetWindowProperties(render_window);
    bool found_driver = false;

    if (SDL_HasProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER)) {
        window_info.type = Core::Frontend::WindowSystemType::Windows;
        window_info.render_surface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        found_driver = true;
    }
    if (!found_driver && SDL_HasProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER)) {
        window_info.type = Core::Frontend::WindowSystemType::X11;
        window_info.display_connection = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
        window_info.render_surface = reinterpret_cast<void*>(SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
        found_driver = true;
    }
    if (!found_driver && SDL_HasProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER)) {
        window_info.type = Core::Frontend::WindowSystemType::Wayland;
        window_info.display_connection = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
        window_info.render_surface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
        found_driver = true;
    }
#ifdef __APPLE__
    if (!found_driver && SDL_HasProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER)) {
        window_info.type = Core::Frontend::WindowSystemType::Cocoa;
        window_info.render_surface = SDL_Metal_CreateView(render_window);
        found_driver = true;
    }
#endif
    if (!found_driver && SDL_HasProperty(props, SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER)) {
        window_info.type = Core::Frontend::WindowSystemType::Android;
        window_info.render_surface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr);
        found_driver = true;
    }

    if (!found_driver) {
        LOG_CRITICAL(Frontend, "Window manager subsystem not supported or recognized");
        std::exit(EXIT_FAILURE);
    }

    OnResize();
    OnMinimalClientAreaChangeRequest(GetActiveConfig().min_client_area_size);
    SDL_PumpEvents();
    LOG_INFO(Frontend, "citron Version: {} | {}-{} (Vulkan)", Common::g_build_name,
             Common::g_scm_branch, Common::g_scm_desc);
}

EmuWindow_SDL2_VK::~EmuWindow_SDL2_VK() = default;

std::unique_ptr<Core::Frontend::GraphicsContext> EmuWindow_SDL2_VK::CreateSharedContext() const {
    return std::make_unique<DummyContext>();
}
