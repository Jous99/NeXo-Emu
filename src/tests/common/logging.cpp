// SPDX-FileCopyrightText: 2026 Citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "common/logging.h"

namespace Common::Log {
namespace {

[[nodiscard]] bool Allows(const Filter& filter, Class log_class, Level level) {
    return filter.CheckMessage(log_class, level);
}

} // Anonymous namespace

TEST_CASE("Log filter parser accepts canonical warning", "[common][logging]") {
    Filter filter{Level::Info};

    filter.ParseFilterString("*:Warning");

    REQUIRE_FALSE(Allows(filter, Class::Service, Level::Info));
    REQUIRE(Allows(filter, Class::Service, Level::Warning));
}

TEST_CASE("Log filter parser accepts lowercase warning", "[common][logging]") {
    Filter filter{Level::Info};

    filter.ParseFilterString("*:warning");

    REQUIRE_FALSE(Allows(filter, Class::Service, Level::Info));
    REQUIRE(Allows(filter, Class::Service, Level::Warning));
}

TEST_CASE("Log filter parser accepts uppercase warning", "[common][logging]") {
    Filter filter{Level::Info};

    filter.ParseFilterString("*:WARNING");

    REQUIRE_FALSE(Allows(filter, Class::Service, Level::Info));
    REQUIRE(Allows(filter, Class::Service, Level::Warning));
}

TEST_CASE("Log filter parser accepts service debug", "[common][logging]") {
    Filter filter{Level::Warning};

    filter.ParseFilterString("Service:Debug");

    REQUIRE(Allows(filter, Class::Service, Level::Debug));
    REQUIRE_FALSE(Allows(filter, Class::Service, Level::Trace));
    REQUIRE_FALSE(Allows(filter, Class::HW_GPU, Level::Debug));
}

TEST_CASE("Log filter parser rejects unknown level", "[common][logging]") {
    Filter filter{Level::Warning};

    filter.ParseFilterString("Service:notalevel");

    REQUIRE_FALSE(Allows(filter, Class::Service, Level::Debug));
    REQUIRE(Allows(filter, Class::Service, Level::Warning));
}

TEST_CASE("Log filter canonicalization normalizes known names", "[common][logging]") {
    REQUIRE(CanonicalizeFilterString("*:Warning") == "*:Warning");
    REQUIRE(CanonicalizeFilterString("*:warning") == "*:Warning");
    REQUIRE(CanonicalizeFilterString("*:WARNING") == "*:Warning");
    REQUIRE(CanonicalizeFilterString("service:debug") == "Service:Debug");
    REQUIRE(CanonicalizeFilterString("Service:notalevel") == "Service:notalevel");
}

} // namespace Common::Log
