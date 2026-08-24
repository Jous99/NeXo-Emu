// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.citron.citron_emu.features.settings.model

import org.citron.citron_emu.utils.NativeConfig

enum class ByteSetting(override val key: String) : AbstractByteSetting {
    AUDIO_VOLUME("volume");

    override fun getByte(needsGlobal: Boolean): Byte {
        if (this == AUDIO_VOLUME) {
            val boost = NativeConfig.getByte(VOLUME_BOOST_KEY, needsGlobal)
            if ((boost.toInt() and 0xFF) > DEFAULT_VOLUME) {
                return boost
            }
        }
        return NativeConfig.getByte(key, needsGlobal)
    }

    override fun setByte(value: Byte) {
        if (NativeConfig.isPerGameConfigLoaded()) {
            global = false
        }

        if (this == AUDIO_VOLUME) {
            if ((value.toInt() and 0xFF) > DEFAULT_VOLUME) {
                NativeConfig.setByte(key, DEFAULT_VOLUME.toByte())
                NativeConfig.setByte(VOLUME_BOOST_KEY, value)
            } else {
                NativeConfig.setByte(key, value)
                NativeConfig.setByte(VOLUME_BOOST_KEY, DEFAULT_VOLUME.toByte())
            }
        } else {
            NativeConfig.setByte(key, value)
        }
    }

    override var global: Boolean
        get() = NativeConfig.usingGlobal(key)
        set(value) {
            NativeConfig.setGlobal(key, value)
            if (this == AUDIO_VOLUME) {
                NativeConfig.setGlobal(VOLUME_BOOST_KEY, value)
            }
        }

    override val defaultValue: Byte by lazy { NativeConfig.getDefaultToString(key).toByte() }

    override fun getValueAsString(needsGlobal: Boolean): String =
        (getByte(needsGlobal).toInt() and 0xFF).toString()

    override fun reset() {
        NativeConfig.setByte(key, defaultValue)
        if (this == AUDIO_VOLUME) {
            NativeConfig.setByte(VOLUME_BOOST_KEY, DEFAULT_VOLUME.toByte())
        }
    }

    companion object {
        private const val DEFAULT_VOLUME = 100
        private const val VOLUME_BOOST_KEY = "volume_boost"
    }
}
