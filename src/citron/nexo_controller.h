// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <set>
#include <string>
#include <QObject>
#include <QString>
#include <QTimer>

#include "common/common_types.h"

namespace Core {
class System;
}

// Sign-in/out, friend-cache refresh, and the online-toast poll. One instance, owned by GMainWindow.
class NeXoController : public QObject {
    Q_OBJECT

public:
    explicit NeXoController(Core::System& system, QWidget* main_window,
                                QObject* parent = nullptr);
    ~NeXoController() override;

    bool IsLinked() const;

    // Title id -> display name via the locally installed game's NACP. Falls back to hint_name
    // (a name the PLAYING client already resolved, e.g. from a friend's presence) when this
    // game isn't in the local library; falls back further to a generic "a game" if both fail.
    QString ResolveGameName(const std::string& app_id_hex, const std::string& hint_name = {}) const;

    // Title id -> base64 icon via the locally installed game's NACP; empty if not installed.
    QString ResolveGameIcon(const std::string& app_id_hex) const;

    // 16 hex digits of the locally running title, matching Friend::app_id; empty if not running.
    std::string GetLocalAppId() const;

    void SignIn();
    void SignOut();
    void RefreshFriendCache();
    void NotifyFriendRequestSent(const QString& friend_code);

    void ManualSaveDownload(u64 title_id);
    void QuickStart(u64 title_id);

signals:
    void AccountLinked();
    void AccountUnlinked();
    void FriendCameOnline(u64 pid, QString name, QString game_name, QString avatar_base64);
    void FriendWentOffline(u64 pid, QString name, QString avatar_base64);
    void FriendRequestReceived(u64 pid, QString name, QString avatar_base64);
    void FriendRequestSent(QString friend_code);
    void StatusChanged(QString message);
    void QuickStartRequested(u64 title_id);
    // Fired once the OAuth URL is known, whether or not the OS actually opened a visible
    // browser window for it (xdg-open/openUrl can both report success with nothing appearing).
    void SignInUrlReady(QString url);
    void SignInFinished();

private:
    void ApplyProfileName(const std::string& name);
    void PollFriends();

    Core::System& system;
    QWidget* main_window;
    QTimer friend_poll_timer;
    std::map<u64, s32> last_known_status;
    std::map<u64, int> offline_streak; // consecutive polls seen offline, not yet confirmed
    std::set<u64> last_known_requests;
    bool first_poll = true; // suppresses a toast burst for every friend already online at boot
};
