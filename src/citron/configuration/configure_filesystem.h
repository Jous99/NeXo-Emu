// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-FileCopyrightText: Copyright 2025 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <QWidget>

class QLineEdit;
class QProgressDialog;

namespace Ui {
    class ConfigureFilesystem;
}

class ConfigureFilesystem : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureFilesystem(QWidget* parent = nullptr);
    ~ConfigureFilesystem() override;

    void ApplyConfiguration();

signals:
    void RequestGameListRefresh();

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();
    void SetConfiguration();
    enum class DirectoryTarget { NAND, SD, Gamecard, Dump, Load, GlobalSave };
    void SetDirectory(DirectoryTarget target, QLineEdit* edit);
    void ResetMetadata();
    void UpdateEnabledControls();

    void MigrateSavesToGlobal(const QString& new_global_path);
    bool CopyDirRecursive(const QString& src, const QString& dest, QProgressDialog& progress, qint64& copied, qint64 total);

    std::unique_ptr<Ui::ConfigureFilesystem> ui;
};
