// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QLabel;
class QButtonGroup;
class PopulationBarChart;

class NeXoPopulationDialog : public QDialog {
    Q_OBJECT

public:
    explicit NeXoPopulationDialog(QWidget* parent = nullptr);

private:
    void RefreshChart(int tab);

    PopulationBarChart* m_chart;
    QLabel* m_live_label;
    QLabel* m_empty_label;
    QLabel* m_updated_label;
    QButtonGroup* m_tabs;
};
