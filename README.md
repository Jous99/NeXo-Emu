<div align="center">
  <h1>NeXoEmu</h1>
  <p><strong>Nintendo Switch Emulator with NeXo Network Online Support</strong></p>
</div>

---

## What is NeXoEmu?

**NeXoEmu** is a Nintendo Switch emulator based on [Eden](https://eden-emu.dev/) with full **NeXoNetwork** online services integration. It allows you to play Nintendo Switch games on PC with online multiplayer support through Raptor Network's infrastructure.

- **Base**: Eden Emulator (2026, actively maintained fork of yuzu)
- **Online layer**: Ported from RaptorCitrus (Raptor Network client, 2021)
- **Project**: NexoEmu — a modern, online-capable Nintendo Switch emulator

---

## Features

### From Eden (base emulator)
- Full Nintendo Switch emulation (ARM64 via Dynarmic JIT)
- Vulkan and OpenGL rendering backends
- High accuracy mode for demanding titles
- Room-based local multiplayer (LDN)
- Android support
- Active development and bug fixes

### Added by NeXoEmu (Raptor Network integration)
- **Online authentication** — log in with your Raptor Network token
- **Hardware ID** — secure per-machine identification for account binding
- **URL rewriting** — transparent redirection of Nintendo servers to Raptor Network
- **FQDN resolver** — blocks Nintendo server connections, routes to Raptor endpoints
- **Subscription info** — displays your Raptor Network subscription status
- **Friends system** — see your friends list, send/accept requests
- **Notifications** — real-time alerts for friend requests and game invites
- **BCAT** — game content delivery via Raptor's servers
- **Online status monitor** — live connection status indicator in the UI

---

## Building

### Requirements

| Dependency  | Version | Notes                           |
|-------------|---------|---------------------------------|
| CMake       | >= 3.22 | Build system                    |
| C++ compiler| C++20   | GCC 12+, Clang 15+, MSVC 2022  |
| Qt          | 5 or 6  | GUI framework                   |
| OpenSSL     | >= 1.1  | HTTPS + hardware ID             |
| SDL2        | >= 2.0  | Input                           |
| Vulkan SDK  | latest  | Rendering                       |

### Clone

```bash
git clone https://github.com/your-fork/NeXoEmu.git
cd NeXoEmu
git submodule update --init --recursive
```

### Linux / macOS

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### Windows (MSVC)

```cmd
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

---

## Setting up NeXoNetwork

1. Create an account at **nexonetwork.space**
2. Generate a token from your account dashboard
3. Open NeXoEmu → **Settings** → **Raptor Network**
4. Paste your token and click **Connect**
5. The status indicator turns **green** when connected

---

## Architecture overview

```
NeXoEmu (Eden base + RaptorNetwork)
├── src/common/
│   ├── hardware_id.h/cpp        [NEW] Unique machine ID for Raptor auth
│   └── settings.h               [MOD] Added raptor_token setting
│
├── src/core/
│   ├── online_initiator.h/cpp   [NEW] Raptor Network connection manager
│   ├── core.h/cpp               [MOD] OnlineInitiator added to System
│   └── hle/service/sockets/
│       └── nsd.cpp              [MOD] FQDN resolver uses Raptor URL rewriting
│
├── src/web_service/
│   └── web_backend.cpp          [MOD] R-HardwareId header on all web requests
│
└── src/yuzu/
    ├── online/                  [NEW] Full online UI module
    │   ├── types.h              Connection status enums
    │   ├── monitor.h/cpp        Online status widget
    │   ├── friends.h/cpp        Friends list dialog
    │   ├── notification*.h/cpp  Real-time notification system
    │   ├── user_delegate.h/cpp  User display widgets
    │   └── online_util.h/cpp    Shared online utilities
    └── configuration/
        └── configure_raptor_online.*  [NEW] Raptor Network settings UI
```

---

## Raptor Network protocol

NeXoEmu communicates with Raptor Network using HTTPS with these custom headers:

| Header          | Value              | Purpose                  |
|-----------------|--------------------|--------------------------|
| `Authorization` | `Bearer <token>`   | User authentication      |
| `R-ClientId`    | `nexoemu`          | Identifies this emulator |
| `R-HardwareId`  | `<48-char ID>`     | Machine binding          |
| `R-Target`      | `config`, `friends`| Service routing          |
| `R-TitleId`     | `<hex game ID>`    | Per-game auth tokens     |

### Raptor Network endpoints

| Service      | Host                                  |
|--------------|---------------------------------------|
| Accounts     | `accounts-api-lp1.nexonetwork.space`     |
| Config       | `config-lp1.nexonetwork.space`           |
| Friends      | `friends-lp1.nexonetwork.space`          |
| Profile      | `profile-lp1.nexonetwork.space`          |
| Notifications| `notification-lp1.nexonetwork.space`     |
| P2P Connector| `connector-lp1.nexonetwork.space`        |
| BCAT         | `bcat-lp1.nexonetwork.space`             |
| Status       | `status-lp1.nexonetwork.space`           |

---

## Credits

- **Eden Emulator Project** — the modern base emulator
- **yuzu Emulator Project** — the original emulator this lineage is built on
- **Raptor Network / Ultramarine Holdings LLC** — online services and RaptorCitrus client
- **NeXoEmu Project** — integration, porting, and maintenance

---

## Legal

NeXoEmu is licensed under **GPL-3.0-or-later**.

NeXoEmu does not include or distribute Nintendo software. You must provide your own legally obtained game files. Nintendo Switch is a trademark of Nintendo Co., Ltd.
