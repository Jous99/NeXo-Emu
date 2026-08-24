// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QDateTime>
#include "citron/applets/qt_error.h"
#include "citron/main.h"

#ifdef ENABLE_WEB_SERVICE
#include "web_service/nexo_api.h"
#endif

namespace {

// NEX carries no field for why a login was refused, so every account gate reaches the player as a
// bare 2306-XXXX. Ask the account server about our own account and say what it actually was.
QString NeXoGateHint(Result error) {
#ifdef ENABLE_WEB_SERVICE
    constexpr u32 NexModule = 306;
    if (static_cast<u32>(error.GetModule()) != NexModule) {
        return {};
    }

    const auto status = WebService::NeXoApi::GetOnlineStatus();
    if (!status.queried || status.allow || status.message.empty()) {
        return {};
    }

    return QStringLiteral("\n\n%1").arg(QString::fromStdString(status.message));
#else
    return {};
#endif
}

} // Anonymous namespace

QtErrorDisplay::QtErrorDisplay(GMainWindow& parent) {
    connect(this, &QtErrorDisplay::MainWindowDisplayError, &parent,
            &GMainWindow::ErrorDisplayDisplayError, Qt::QueuedConnection);
    connect(this, &QtErrorDisplay::MainWindowRequestExit, &parent,
            &GMainWindow::ErrorDisplayRequestExit, Qt::QueuedConnection);
    connect(&parent, &GMainWindow::ErrorDisplayFinished, this,
            &QtErrorDisplay::MainWindowFinishedError, Qt::DirectConnection);
}

QtErrorDisplay::~QtErrorDisplay() = default;

void QtErrorDisplay::Close() const {
    callback = {};
    emit MainWindowRequestExit();
}

void QtErrorDisplay::ShowError(Result error, FinishedCallback finished) const {
    callback = std::move(finished);
    emit MainWindowDisplayError(
        tr("Error Code: %1-%2 (0x%3)")
            .arg(static_cast<u32>(error.GetModule()) + 2000, 4, 10, QChar::fromLatin1('0'))
            .arg(error.GetDescription(), 4, 10, QChar::fromLatin1('0'))
            .arg(error.raw, 8, 16, QChar::fromLatin1('0')),
        tr("An error has occurred.\nPlease try again or contact the developer of the software.") +
            NeXoGateHint(error));
}

void QtErrorDisplay::ShowErrorWithTimestamp(Result error, std::chrono::seconds time,
                                            FinishedCallback finished) const {
    callback = std::move(finished);

    const QDateTime date_time = QDateTime::fromSecsSinceEpoch(time.count());
    emit MainWindowDisplayError(
        tr("Error Code: %1-%2 (0x%3)")
            .arg(static_cast<u32>(error.GetModule()) + 2000, 4, 10, QChar::fromLatin1('0'))
            .arg(error.GetDescription(), 4, 10, QChar::fromLatin1('0'))
            .arg(error.raw, 8, 16, QChar::fromLatin1('0')),
        tr("An error occurred on %1 at %2.\nPlease try again or contact the developer of the "
           "software.")
            .arg(date_time.toString(QStringLiteral("dddd, MMMM d, yyyy")))
            .arg(date_time.toString(QStringLiteral("h:mm:ss A"))));
}

void QtErrorDisplay::ShowCustomErrorText(Result error, std::string dialog_text,
                                         std::string fullscreen_text,
                                         FinishedCallback finished) const {
    callback = std::move(finished);
    // [NeXo] ResultSuccess = informational notice, skip the error framing.
    if (error == ResultSuccess) {
        emit MainWindowDisplayError(QString::fromStdString(dialog_text),
                                    QString::fromStdString(fullscreen_text));
        return;
    }
    emit MainWindowDisplayError(
        tr("Error Code: %1-%2 (0x%3)")
            .arg(static_cast<u32>(error.GetModule()) + 2000, 4, 10, QChar::fromLatin1('0'))
            .arg(error.GetDescription(), 4, 10, QChar::fromLatin1('0'))
            .arg(error.raw, 8, 16, QChar::fromLatin1('0')),
        tr("An error has occurred.\n\n%1\n\n%2")
            .arg(QString::fromStdString(dialog_text))
            .arg(QString::fromStdString(fullscreen_text)));
}

void QtErrorDisplay::MainWindowFinishedError() {
    if (callback) {
        callback();
    }
}
