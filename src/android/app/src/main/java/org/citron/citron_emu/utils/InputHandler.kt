// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.citron.citron_emu.utils

import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import org.citron.citron_emu.BuildConfig
import org.citron.citron_emu.features.input.CitronInputOverlayDevice
import org.citron.citron_emu.features.input.CitronPhysicalDevice
import org.citron.citron_emu.features.input.NativeInput

object InputHandler {
    var androidControllers = mapOf<Int, CitronPhysicalDevice>()
    var registeredControllers = mutableListOf<ParamPackage>()
    private val controllerStates = mutableMapOf<Int, ControllerInputState>()
    private var changedAxesScratch = IntArray(16)
    private var changedValuesScratch = FloatArray(16)

    /** Returns whether Android currently exposes a physical game controller. */
    fun hasPhysicalController(): Boolean = InputDevice.getDeviceIds().any { deviceId ->
        InputDevice.getDevice(deviceId)?.isGameController() == true
    }

    fun dispatchKeyEvent(event: KeyEvent): Boolean {
        val dispatchStartNs = inputTimingStartNs()
        if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount > 0) {
            return true
        }

        val action = when (event.action) {
            KeyEvent.ACTION_DOWN -> NativeInput.ButtonState.PRESSED
            KeyEvent.ACTION_UP -> NativeInput.ButtonState.RELEASED
            else -> return false
        }

        var controllerData = androidControllers[event.device.controllerNumber]
        if (controllerData == null) {
            updateControllerData()
            controllerData = androidControllers[event.device.controllerNumber] ?: return false
        }

        val inputState = controllerStates.getOrPut(event.device.controllerNumber) {
            ControllerInputState()
        }
        if (inputState.getButtonState(event.keyCode) == action) {
            return true
        }
        inputState.setButtonState(event.keyCode, action)

        if (BuildConfig.DEBUG) {
            NativeInput.onGamePadButtonEventByPortTimed(
                controllerData.getPort(),
                event.keyCode,
                action,
                dispatchStartNs
            )
        } else {
            NativeInput.onGamePadButtonEventByPort(
                controllerData.getPort(),
                event.keyCode,
                action
            )
        }
        return true
    }

    fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        val dispatchStartNs = inputTimingStartNs()
        val device = event.device ?: return false
        val controllerNumber = device.controllerNumber
        var controllerData = androidControllers[controllerNumber]
        if (controllerData == null) {
            updateControllerData()
            controllerData = androidControllers[controllerNumber] ?: return false
        }

        val axes = controllerData.getAxesForSource(event.source)
        if (axes.isEmpty()) {
            return true
        }

        val inputState = controllerStates.getOrPut(controllerNumber) {
            ControllerInputState()
        }
        ensureAxisScratchCapacity(axes.size)
        var changedCount = 0

        axes.forEach { axis ->
            val value = event.getAxisValue(axis)
            if (inputState.setAxisValueIfChanged(axis, value)) {
                changedAxesScratch[changedCount] = axis
                changedValuesScratch[changedCount] = value
                changedCount++
            }
        }

        if (changedCount > 0) {
            if (BuildConfig.DEBUG) {
                NativeInput.onGamePadAxisEventByPortTimed(
                    controllerData.getPort(),
                    changedAxesScratch,
                    changedValuesScratch,
                    changedCount,
                    dispatchStartNs
                )
            } else {
                NativeInput.onGamePadAxisEventByPort(
                    controllerData.getPort(),
                    changedAxesScratch,
                    changedValuesScratch,
                    changedCount
                )
            }
        }
        return true
    }

    fun releaseAllInputs() {
        NativeInput.resetGamePadInputState()
        controllerStates.clear()
    }

    private fun inputTimingStartNs(): Long =
        if (BuildConfig.DEBUG) {
            System.nanoTime()
        } else {
            0L
        }

    private fun ensureAxisScratchCapacity(size: Int) {
        if (changedAxesScratch.size >= size) {
            return
        }

        changedAxesScratch = IntArray(size)
        changedValuesScratch = FloatArray(size)
    }

    fun getDevices(): Map<Int, CitronPhysicalDevice> {
        val gameControllerDeviceIds = mutableMapOf<Int, CitronPhysicalDevice>()
        val deviceIds = InputDevice.getDeviceIds()
        var port = 0
        val inputSettings = NativeConfig.getInputSettings(true)
        deviceIds.forEach { deviceId ->
            InputDevice.getDevice(deviceId)?.apply {
                // Verify that the device has gamepad buttons, control sticks, or both.
                if (isGameController()) {
                    if (!gameControllerDeviceIds.contains(controllerNumber)) {
                        gameControllerDeviceIds[controllerNumber] = CitronPhysicalDevice(
                            this,
                            port,
                            inputSettings[port].useSystemVibrator
                        )
                    }
                    port++
                }
            }
        }
        return gameControllerDeviceIds
    }

    fun updateControllerData() {
        releaseAllInputs()
        NativeInput.clearRegisteredControllers()
        androidControllers = getDevices()
        androidControllers.forEach {
            NativeInput.registerController(it.value)
        }

        // Register the input overlay on a dedicated port for all player 1 vibrations
        NativeInput.registerController(CitronInputOverlayDevice(androidControllers.isEmpty(), 100))
        registeredControllers.clear()
        NativeInput.getInputDevices().forEach {
            registeredControllers.add(ParamPackage(it))
        }
        registeredControllers.sortBy { it.get("port", 0) }
    }

    fun InputDevice.getGUID(): String = String.format("%016x%016x", productId, vendorId)

    private fun InputDevice.isGameController(): Boolean =
        sources and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
            sources and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK

    private class ControllerInputState {
        private var buttonStates = IntArray(256) { UNKNOWN_BUTTON_STATE }
        private var axisValues = FloatArray(64)
        private var axisSeen = BooleanArray(64)

        fun getButtonState(button: Int): Int {
            if (button < 0 || button >= buttonStates.size) {
                return UNKNOWN_BUTTON_STATE
            }
            return buttonStates[button]
        }

        fun setButtonState(button: Int, state: Int) {
            if (button < 0) {
                return
            }
            ensureButtonCapacity(button)
            buttonStates[button] = state
        }

        fun setAxisValueIfChanged(axis: Int, value: Float): Boolean {
            if (axis < 0) {
                return false
            }
            ensureAxisCapacity(axis)

            if (axisSeen[axis] && axisValues[axis].toRawBits() == value.toRawBits()) {
                return false
            }

            axisSeen[axis] = true
            axisValues[axis] = value
            return true
        }

        private fun ensureButtonCapacity(button: Int) {
            if (button < buttonStates.size) {
                return
            }

            buttonStates = buttonStates.copyOf(button + 1).also {
                it.fill(UNKNOWN_BUTTON_STATE, buttonStates.size, it.size)
            }
        }

        private fun ensureAxisCapacity(axis: Int) {
            if (axis < axisValues.size) {
                return
            }

            val newSize = axis + 1
            axisValues = axisValues.copyOf(newSize)
            axisSeen = axisSeen.copyOf(newSize)
        }

        companion object {
            private const val UNKNOWN_BUTTON_STATE = -1
        }
    }
}
