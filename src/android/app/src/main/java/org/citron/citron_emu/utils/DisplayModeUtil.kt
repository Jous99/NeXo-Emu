// SPDX-FileCopyrightText: 2026 Citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.citron.citron_emu.utils

import android.app.Activity
import android.os.Build
import android.view.Surface

object DisplayModeUtil {
    fun preferHighestRefreshRate(activity: Activity): Float? {
        val display = activity.display ?: return null
        val currentMode = display.mode
        val highestRefreshMode = display.supportedModes
            .filter {
                it.physicalWidth == currentMode.physicalWidth &&
                    it.physicalHeight == currentMode.physicalHeight
            }
            .maxByOrNull { it.refreshRate }
            ?: currentMode

        val layoutParams = activity.window.attributes
        if (layoutParams.preferredDisplayModeId != highestRefreshMode.modeId) {
            layoutParams.preferredDisplayModeId = highestRefreshMode.modeId
            activity.window.attributes = layoutParams
        }
        return highestRefreshMode.refreshRate
    }

    fun configureSurface(activity: Activity, surface: Surface) {
        val refreshRate = preferHighestRefreshRate(activity) ?: return
        if (!surface.isValid) {
            return
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            surface.setFrameRate(
                refreshRate,
                Surface.FRAME_RATE_COMPATIBILITY_DEFAULT,
                Surface.CHANGE_FRAME_RATE_ONLY_IF_SEAMLESS
            )
        } else {
            surface.setFrameRate(refreshRate, Surface.FRAME_RATE_COMPATIBILITY_DEFAULT)
        }
    }
}
