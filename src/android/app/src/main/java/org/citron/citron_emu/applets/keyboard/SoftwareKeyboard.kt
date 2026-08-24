// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: Copyright 2026 Citron Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.citron.citron_emu.applets.keyboard

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.text.InputType
import android.view.KeyEvent
import android.view.View
import android.view.WindowInsets
import android.view.inputmethod.InputMethodManager
import androidx.annotation.Keep
import androidx.core.view.ViewCompat
import java.io.Serializable
import org.citron.citron_emu.NativeLibrary
import org.citron.citron_emu.R
import org.citron.citron_emu.applets.keyboard.ui.KeyboardDialogFragment

@Keep
object SoftwareKeyboard {

    lateinit var data: KeyboardData
    @Suppress("PLATFORM_CLASS_MAPPED_TO_KOTLIN")
    val dataLock = Object()

    @Volatile
    private var inlineConfig: KeyboardConfig? = null

    private fun executeNormalImpl(config: KeyboardConfig) {
        val activity = NativeLibrary.sEmulationActivity.get() ?: return

        data = KeyboardData(SwkbdResult.Cancel.ordinal, "")

        val fragment = KeyboardDialogFragment.newInstance(config)
        fragment.show(activity.supportFragmentManager, KeyboardDialogFragment.TAG)
    }

    private fun executeInlineImpl(config: KeyboardConfig) {
        val activity = NativeLibrary.sEmulationActivity.get() ?: return
        val overlayView = activity.findViewById<View>(R.id.surface_input_overlay) ?: return
        val previousVisibility = overlayView.visibility
        val previousAlpha = overlayView.alpha

        inlineConfig = config

        // Make sure the overlay can receive input
        overlayView.visibility = View.VISIBLE
        if (previousVisibility != View.VISIBLE) {
            overlayView.alpha = 0f
        }
        overlayView.isFocusableInTouchMode = true

        overlayView.post {
            overlayView.requestFocus()

            val imm =
                overlayView.context.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager

            // Restart input to ensure clean state
            imm.restartInput(overlayView)
            @Suppress("DEPRECATION")
            imm.showSoftInput(overlayView, InputMethodManager.SHOW_FORCED)

            // Poll every 500ms to detect when the keyboard is closed
            startKeyboardDismissPolling(overlayView, previousVisibility, previousAlpha)
        }
    }

    private fun startKeyboardDismissPolling(
        overlayView: View,
        previousVisibility: Int,
        previousAlpha: Float
    ) {
        val handler = Handler(Looper.myLooper()!!)
        val delayMs = 500L

        handler.postDelayed(object : Runnable {
            override fun run() {
                val insets = ViewCompat.getRootWindowInsets(overlayView)
                val isKeyboardVisible = insets?.isVisible(WindowInsets.Type.ime()) == true

                if (isKeyboardVisible) {
                    handler.postDelayed(this, delayMs)
                    return
                }

                // Keyboard was dismissed without a key event. Treat it as cancellation.
                overlayView.visibility = previousVisibility
                overlayView.alpha = previousAlpha
                NativeLibrary.submitInlineKeyboardInput(KeyEvent.KEYCODE_BACK)
                clearInlineConfig()
            }
        }, delayMs)
    }

    fun getInlineInitialText(): String = inlineConfig?.initial_text.orEmpty()

    fun getInlineInitialCursorPosition(): Int = inlineConfig?.initial_cursor_position ?: 0

    fun getInlineEditorInputType(): Int =
        inlineConfig?.let(::getEditorInputType) ?: InputType.TYPE_CLASS_TEXT

    fun getEditorInputType(config: KeyboardConfig): Int {
        val passwordEnabled = config.password_mode == SwkbdPasswordMode.Enabled.ordinal
        return when (config.type) {
            SwkbdType.NumberPad.ordinal -> {
                InputType.TYPE_CLASS_NUMBER or
                    if (passwordEnabled) InputType.TYPE_NUMBER_VARIATION_PASSWORD else 0
            }
            else -> {
                InputType.TYPE_CLASS_TEXT or
                    if (passwordEnabled) {
                        InputType.TYPE_TEXT_VARIATION_PASSWORD
                    } else {
                        InputType.TYPE_TEXT_VARIATION_NORMAL
                    }
            }
        }
    }

    fun clearInlineConfig() {
        inlineConfig = null
    }

    @JvmStatic
    fun executeNormal(config: KeyboardConfig): KeyboardData {
        NativeLibrary.sEmulationActivity.get()!!.runOnUiThread {
            executeNormalImpl(config)
        }

        synchronized(dataLock) {
            dataLock.wait()
        }
        return data
    }

    @JvmStatic
    fun executeInline(config: KeyboardConfig) {
        NativeLibrary.sEmulationActivity.get()!!.runOnUiThread {
            executeInlineImpl(config)
        }
    }

    // Corresponds to Service::AM::Applets::SwkbdType
    enum class SwkbdType {
        Normal,
        NumberPad,
        Qwerty,
        Unknown3,
        Latin,
        SimplifiedChinese,
        TraditionalChinese,
        Korean
    }

    // Corresponds to Service::AM::Applets::SwkbdPasswordMode
    enum class SwkbdPasswordMode {
        Disabled,
        Enabled
    }

    // Corresponds to Service::AM::Applets::SwkbdResult
    enum class SwkbdResult {
        Ok,
        Cancel
    }

    @Keep
    data class KeyboardConfig(
        var ok_text: String? = null,
        var header_text: String? = null,
        var sub_text: String? = null,
        var guide_text: String? = null,
        var initial_text: String? = null,
        var left_optional_symbol_key: Short = 0,
        var right_optional_symbol_key: Short = 0,
        var max_text_length: Int = 0,
        var min_text_length: Int = 0,
        var initial_cursor_position: Int = 0,
        var type: Int = 0,
        var password_mode: Int = 0,
        var text_draw_type: Int = 0,
        var key_disable_flags: Int = 0,
        var use_blur_background: Boolean = false,
        var enable_backspace_button: Boolean = false,
        var enable_return_button: Boolean = false,
        var disable_cancel_button: Boolean = false
    ) : Serializable

    @Keep
    data class KeyboardData(var result: Int, var text: String)
}
