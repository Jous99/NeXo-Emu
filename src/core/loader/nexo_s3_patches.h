// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <vector>

#include "common/common_types.h"

namespace Loader::NeXoS3Patches {

// Applies citron's built-in Splatoon 3 patches (certificate-pinning bypass, peer hostname fix)
// to a loaded NSO, keyed by its build ID. Returns nso unchanged if build_id isn't one of the two
// known Splatoon 3 builds.
//
// Baked in rather than shipped as an exefs_patches mod, and for the same reason
// NeXoNetwork/Ryujinx-NeXo's NeXoS3Patches.cs is: Splatoon 3 refuses to boot with
// any mod enabled (see main.cpp's boot-time check), so a patch living on disk as a mod would be
// blocked by the very rule it needs to get past. These bytes never touch the mod-loading path
// at all, so they aren't affected by it. Without them the game's TLS stops at ClientHello and
// nothing connects -- this bypasses the game's own in-game certificate pinning, which is a
// separate, later check than the TLS-layer verification already bypassed for every other
// NeXo title in ssl_backend_openssl.cpp's SetVerifyOption.
//
// nso must be [NSOHeader][decompressed segment data], the exact same layout
// PatchManager::PatchNSO expects -- this is designed to run right alongside it in nso.cpp,
// independent of whether normal mod patches applied.
std::vector<u8> ApplyIfMatch(const std::array<u8, 0x20>& build_id, std::vector<u8> nso);

} // namespace Loader::NeXoS3Patches
