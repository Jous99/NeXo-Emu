// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-FileCopyrightText: Copyright 2025 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>

#include "core/hle/service/service.h"
#include "core/internal_network/network.h"

namespace Core {
class System;
}

namespace Service::Sockets {

class SFDNSRES final : public ServiceFramework<SFDNSRES> {
public:
    explicit SFDNSRES(Core::System& system_);
    ~SFDNSRES() override;

private:
    void GetHostByNameRequest(HLERequestContext& ctx);
    void GetGaiStringErrorRequest(HLERequestContext& ctx);
    void GetHostByNameRequestWithOptions(HLERequestContext& ctx);
    void GetAddrInfoRequest(HLERequestContext& ctx);
    void GetAddrInfoRequestWithOptions(HLERequestContext& ctx);
    void ResolverSetOptionRequest(HLERequestContext& ctx);
    void SetDnsAddresses(HLERequestContext& ctx);
    void GetDnsAddressList(HLERequestContext& ctx);
    void GetHostByAddrRequest(HLERequestContext& ctx);
    void GetHostStringError(HLERequestContext& ctx);
    void GetCancelHandleRequest(HLERequestContext& ctx);
    void CancelRequest(HLERequestContext& ctx);
    void GetOptions(HLERequestContext& ctx);
    void GetAddrInfoRequestRaw(HLERequestContext& ctx);
    void GetNameInfoRequest(HLERequestContext& ctx);
    void GetNameInfoRequestWithOptions(HLERequestContext& ctx);
};

void SetLastHostForIp(const std::string& ip, const std::string& host);
std::string GetLastHostForIp(const std::string& ip);

// [NeXo] Splatoon 3's embedded gRPC/HTTP2 stack (not NEX) loses the resolved address
// somewhere in its own addrinfo handling and falls back to connecting with a zeroed IP,
// keeping only the port it originally resolved for. BSD::ConnectImpl uses this to recover
// the IP a redirected NeXo hostname resolved to for that exact port. Deliberately keyed
// by port, not "last resolution overall": a P2P socket targets another console's port, for
// which no redirect exists and none should be substituted -- see the .cpp for the real bug
// this guards against (observed and fixed the same way in Ryujinx-NeXo).
void SetLastIpForPort(u16 port, Network::IPv4Address ip);
std::optional<Network::IPv4Address> GetLastIpForPort(u16 port);

} // namespace Service::Sockets
