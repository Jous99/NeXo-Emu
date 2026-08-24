// SPDX-FileCopyrightText: 2026 Citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.citron.citron_emu.features.settings.model.view

import androidx.annotation.StringRes
import org.citron.citron_emu.features.settings.model.AbstractStringSetting

class LogFilterSetting(
    private val stringSetting: AbstractStringSetting,
    @StringRes titleId: Int = 0,
    titleString: String = "",
    @StringRes descriptionId: Int = 0,
    descriptionString: String = "",
    private val customChoice: String
) : SettingsItem(stringSetting, titleId, titleString, descriptionId, descriptionString) {
    override val type = TYPE_LOG_FILTER

    private val presetValues = arrayOf(
        "*:Warning",
        "*:Info",
        "*:Debug",
        "*:Trace",
        "*:Warning Service.Audio:Debug HW.GPU:Debug"
    )

    val customIndex: Int
        get() = presetValues.size

    val choices: Array<String>
        get() = presetValues + customChoice

    val selectedValueIndex: Int
        get() {
            val index = presetValues.indexOf(getSelectedValue())
            return if (index >= 0) index else customIndex
        }

    fun getValueAt(index: Int): String =
        if (index >= 0 && index < presetValues.size) presetValues[index] else ""

    fun getSelectedValue(needsGlobal: Boolean = false) = stringSetting.getString(needsGlobal)

    fun getDisplayValue(): String {
        val value = getSelectedValue()
        return value.ifEmpty { customChoice }
    }

    fun setSelectedValue(value: String) = stringSetting.setString(value)
}
