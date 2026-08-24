// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cctype>
#include <span>

#include "common/hex_util.h"
#include "core/hle/service/bcat/bcat_result.h"
#include "core/hle/service/bcat/bcat_types.h"

namespace Service::BCAT {

// For a name to be valid it must be non-empty, must contain a null terminating character
// somewhere in the buffer, and everything before that null can only contain numbers, letters,
// underscores and a hyphen if directory and a period if file. Bytes after the null terminator
// are not part of the name (real callers don't always zero the rest of the buffer) and are
// ignored, matching Common::StringFromFixedZeroTerminatedBuffer's own semantics.
constexpr Result VerifyNameValidInternal(std::array<char, 0x20> name, char match_char) {
    const auto null_pos = std::find(name.begin(), name.end(), '\0');
    const bool bad_chars =
        null_pos == name.begin() ||
        std::any_of(name.begin(), null_pos, [match_char](char c) {
            return !std::isalnum(static_cast<u8>(c)) && c != '_' && c != match_char;
        });
    if (null_pos == name.end() || bad_chars) {
        LOG_ERROR(Service_BCAT, "Name passed was invalid! match_char={} raw={}", match_char,
                  Common::HexToString(std::span(reinterpret_cast<const u8*>(name.data()),
                                                name.size())));
        return ResultInvalidArgument;
    }

    return ResultSuccess;
}

constexpr Result VerifyNameValidDir(DirectoryName name) {
    return VerifyNameValidInternal(name, '-');
}

constexpr Result VerifyNameValidFile(FileName name) {
    return VerifyNameValidInternal(name, '.');
}

} // namespace Service::BCAT
