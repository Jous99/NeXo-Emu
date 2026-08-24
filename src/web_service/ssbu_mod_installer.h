// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <string>
#include <vector>

#include "common/common_types.h"

namespace WebService::SkylineMods {

// List, not a single constant, so an alternate SKU's ID can be added later.
constexpr std::array<u64, 1> SSBU_TITLE_IDS{
    0x01006A800016E000ULL,
};

constexpr bool IsSsbuTitleId(u64 program_id) {
    for (const u64 id : SSBU_TITLE_IDS) {
        if (id == program_id) {
            return true;
        }
    }
    return false;
}

struct ModFetchResult {
    std::string display_name;
    bool success = false;
    bool is_zip = false;
    std::string filename;
    std::vector<u8> data;
    std::string error;
    std::string release_page_url; // For a manual-download link on failure.
};

// Per-repo failures are recorded in ModFetchResult rather than thrown; the loop always
// continues, so the result always has one entry per known repo.
std::vector<ModFetchResult> FetchAllSkylineModAssets();

} // namespace WebService::SkylineMods
