// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <thread>
#include <utility>

#include <QByteArray>
#include <QDesktopServices>
#include <QPointer>
#include <QProcess>
#include <QUrl>

#include <fmt/format.h>

#include "common/logging.h"
#include "common/nexo_account.h"
#include "common/nexo_friends.h"
#include "core/core.h"
#include "core/hle/service/friend/friend.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/vfs/vfs.h"
#include "core/hle/service/acc/profile_manager.h"
#include "common/nexo_compatible_titles.h"
#include "citron/nexo_controller.h"
#include "citron/nexo_save_sync.h"

#ifdef ENABLE_WEB_SERVICE
#include "web_service/nexo_api.h"
#endif

NeXoController::NeXoController(Core::System& system_, QWidget* main_window_,
                                       QObject* parent)
    : QObject(parent), system(system_), main_window(main_window_) {
    friend_poll_timer.setInterval(20000);
    connect(&friend_poll_timer, &QTimer::timeout, this, &NeXoController::PollFriends);
    friend_poll_timer.start();

    PollFriends();
}

NeXoController::~NeXoController() = default;

bool NeXoController::IsLinked() const {
    return Common::NeXoAccount::IsLinked();
}

QString NeXoController::ResolveGameName(const std::string& app_id_hex,
                                            const std::string& hint_name) const {
    if (app_id_hex.empty()) {
        return {};
    }

    u64 program_id = 0;
    try {
        program_id = std::stoull(app_id_hex, nullptr, 16);
    } catch (const std::exception&) {
        return {};
    }
    if (program_id == 0) {
        return {};
    }

    const FileSys::PatchManager pm{program_id, system.GetFileSystemController(),
                                   system.GetContentProvider()};
    const auto [nacp, icon] = pm.GetControlMetadata();
    if (nacp) {
        const auto name = nacp->GetApplicationName();
        if (!name.empty()) {
            return QString::fromStdString(name);
        }
    }
    if (!hint_name.empty()) {
        return QString::fromStdString(hint_name);
    }
    return tr("a game");
}

QString NeXoController::ResolveGameIcon(const std::string& app_id_hex) const {
    if (app_id_hex.empty()) {
        return {};
    }

    u64 program_id = 0;
    try {
        program_id = std::stoull(app_id_hex, nullptr, 16);
    } catch (const std::exception&) {
        return {};
    }
    if (program_id == 0) {
        return {};
    }

    const FileSys::PatchManager pm{program_id, system.GetFileSystemController(),
                                   system.GetContentProvider()};
    const auto [nacp, icon_file] = pm.GetControlMetadata();
    if (!icon_file) {
        return {};
    }
    const std::vector<u8> icon_bytes = icon_file->ReadAllBytes();
    if (icon_bytes.empty()) {
        return {};
    }
    return QString::fromLatin1(QByteArray::fromRawData(
        reinterpret_cast<const char*>(icon_bytes.data()), static_cast<int>(icon_bytes.size()))
                                    .toBase64());
}

std::string NeXoController::GetLocalAppId() const {
    if (!system.IsPoweredOn()) {
        return {};
    }
    return fmt::format("{:016X}", system.GetApplicationProcessProgramID());
}

void NeXoController::SignIn() {
#ifdef ENABLE_WEB_SERVICE
    emit StatusChanged(tr("Finish signing in in your browser, then come back here."));

    QPointer<NeXoController> self(this);
    std::thread{[this, self] {
        const auto open_url = [this, self](const std::string& url) {
            if (!self) {
                return;
            }
            QMetaObject::invokeMethod(
                this,
                [this, url] {
#ifdef __linux__
                    // xdg-desktop-portal can report success without a browser ever appearing.
                    qint64 pid = -1;
                    const bool ok = QProcess::startDetached(
                        QStringLiteral("xdg-open"), {QString::fromStdString(url)}, QString(), &pid);
                    LOG_INFO(Frontend, "NeXoController::SignIn: xdg-open -> ok={} pid={}", ok,
                             pid);
#else
                    const bool ok = QDesktopServices::openUrl(QUrl(QString::fromStdString(url)));
                    LOG_INFO(Frontend, "NeXoController::SignIn: QDesktopServices::openUrl -> {}",
                             ok);
#endif
                    emit SignInUrlReady(QString::fromStdString(url));
                },
                Qt::QueuedConnection);
        };

        auto login_result = WebService::NeXoApi::SignInWithBrowser(open_url);

        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(
            this,
            [this, self, result = std::move(login_result)] {
                if (!self) {
                    return;
                }
                if (!result.ok) {
                    emit StatusChanged(QString::fromStdString(result.error));
                    emit SignInFinished();
                    return;
                }

                Common::NeXoAccount::Save(result.pid, result.username, result.friend_code,
                                              result.token);
                ApplyProfileName(result.username);
                Common::NeXoFriends::SetLocalStatus(Common::NeXoFriends::PresenceOnline);
                first_poll = true;
                emit AccountLinked();
                emit SignInFinished();
                RefreshFriendCache();
            },
            Qt::QueuedConnection);
    }}.detach();
#else
    emit StatusChanged(tr("This build has no web services support."));
#endif
}

void NeXoController::SignOut() {
    Common::NeXoAccount::Clear();
    Common::NeXoFriends::Set({});
    last_known_status.clear();
    offline_streak.clear();
    emit AccountUnlinked();
}

void NeXoController::ManualSaveDownload(u64 title_id) {
#ifdef ENABLE_WEB_SERVICE
    if (!NeXo::CompatibleTitles::Table().count(title_id)) {
        emit StatusChanged(tr("This game doesn't support cloud saves."));
        return;
    }

    emit StatusChanged(tr("Downloading save from the cloud..."));
    QPointer<NeXoController> self(this);
    std::thread{[this, self, title_id] {
        NeXo::SaveSync::Pull(system, title_id, /*force=*/true);
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(
            this, [this, self] {
                if (!self) {
                    return;
                }
                emit StatusChanged(tr("Cloud save applied."));
            }, Qt::QueuedConnection);
    }}.detach();
#else
    emit StatusChanged(tr("This build has no web services support."));
#endif
}

void NeXoController::QuickStart(u64 title_id) {
    emit QuickStartRequested(title_id);
}

void NeXoController::ApplyProfileName(const std::string& name) {
    if (name.empty()) {
        return;
    }

    Service::Account::ProfileManager profile_manager;
    const auto uuid = profile_manager.GetLastOpenedUser();
    if (uuid.IsInvalid()) {
        return;
    }

    Service::Account::ProfileBase profile{};
    if (!profile_manager.GetProfileBase(uuid, profile)) {
        return;
    }

    const std::string trimmed = name.substr(0, profile.username.size() - 1);
    std::fill(profile.username.begin(), profile.username.end(), '\0');
    std::copy(trimmed.begin(), trimmed.end(), profile.username.begin());

    profile_manager.SetProfileBase(uuid, profile);
    profile_manager.WriteUserSaveFile();
    LOG_INFO(Frontend, "[NeXo] Renamed the active profile to the account nickname");
}

void NeXoController::RefreshFriendCache() {
    PollFriends();
}

void NeXoController::NotifyFriendRequestSent(const QString& friend_code) {
    emit FriendRequestSent(friend_code);
}

void NeXoController::PollFriends() {
#ifdef ENABLE_WEB_SERVICE
    if (!Common::NeXoAccount::IsLinked()) {
        return;
    }

    QPointer<NeXoController> self(this);
    std::thread{[this, self] {
        auto fetched = WebService::NeXoApi::GetFriends();
        if (!fetched.ok) {
            return;
        }
        if (!self) {
            return;
        }

        QMetaObject::invokeMethod(
            this,
            [this, self, list = std::move(fetched)] {
                if (!self) {
                    return;
                }
                std::vector<Common::NeXoFriends::Entry> cache;
                cache.reserve(list.friends.size());
                for (const auto& entry : list.friends) {
                    const auto decoded_image =
                        QByteArray::fromBase64(QByteArray::fromStdString(entry.image_base64));
                    cache.push_back(
                        {entry.pid, entry.name, entry.presence_status, entry.app_field,
                         std::vector<u8>(decoded_image.begin(), decoded_image.end())});
                }
                Common::NeXoFriends::Set(std::move(cache));
                // [NeXo] The guest's own INotificationService only ever signals once, at
                // construction -- before this first real poll has a chance to land. Without this,
                // a Friends viewer already on-screen never learns that real data showed up.
                Service::Friend::NotifyFriendsListUpdated();

                const bool suppress_toasts = first_poll;
                first_poll = false;

                std::map<u64, s32> current_status;
                for (const auto& entry : list.friends) {
                    const auto it = last_known_status.find(entry.pid);
                    const bool was_online = it != last_known_status.end() && it->second != 0;

                    // A status of 0 on a single poll can be a mid-transition blip on the
                    // server (e.g. switching presence when joining a match together) rather
                    // than a real disconnect. Require it to repeat on the next poll (~20s)
                    // before treating the friend as actually offline, so a one-poll blip can't
                    // fire a false "went offline" or the false "came online" right after it.
                    if (entry.presence_status == 0 && was_online) {
                        if (++offline_streak[entry.pid] < 2) {
                            current_status[entry.pid] = it->second;
                            continue;
                        }
                        if (!suppress_toasts) {
                            emit FriendWentOffline(entry.pid, QString::fromStdString(entry.name),
                                                   QString::fromStdString(entry.image_base64));
                        }
                    } else {
                        offline_streak.erase(entry.pid);
                        const bool was_offline = it == last_known_status.end() || it->second == 0;
                        if (!suppress_toasts && was_offline && entry.presence_status != 0) {
                            emit FriendCameOnline(entry.pid, QString::fromStdString(entry.name),
                                                  ResolveGameName(entry.app_id, entry.app_name),
                                                  QString::fromStdString(entry.image_base64));
                        }
                    }
                    current_status[entry.pid] = entry.presence_status;
                }
                last_known_status = std::move(current_status);

                std::set<u64> current_requests;
                for (const auto& entry : list.requests) {
                    current_requests.insert(entry.pid);
                    if (!suppress_toasts && !last_known_requests.contains(entry.pid)) {
                        emit FriendRequestReceived(entry.pid, QString::fromStdString(entry.name),
                                                   QString::fromStdString(entry.image_base64));
                    }
                }
                last_known_requests = std::move(current_requests);
            },
            Qt::QueuedConnection);
    }}.detach();
#endif
}
