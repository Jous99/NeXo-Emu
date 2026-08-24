// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.citron.citron_emu.adapters

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import org.citron.citron_emu.databinding.ListItemAddonBinding
import org.citron.citron_emu.model.Patch
import org.citron.citron_emu.model.AddonViewModel
import org.citron.citron_emu.viewholder.AbstractViewHolder

class AddonAdapter(val addonViewModel: AddonViewModel) :
    AbstractDiffAdapter<Patch, AddonAdapter.AddonViewHolder>() {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AddonViewHolder {
        ListItemAddonBinding.inflate(LayoutInflater.from(parent.context), parent, false)
            .also { return AddonViewHolder(it) }
    }

    inner class AddonViewHolder(val binding: ListItemAddonBinding) :
        AbstractViewHolder<Patch>(binding) {
        override fun bind(model: Patch) {
            binding.root.setOnClickListener {
                binding.addonCheckbox.isChecked = !binding.addonCheckbox.isChecked
            }
            binding.title.text = model.name
            binding.version.text = model.version
            binding.addonCheckbox.setOnCheckedChangeListener(null)
            binding.addonCheckbox.isChecked = model.enabled
            binding.addonCheckbox.setOnCheckedChangeListener { _, checked ->
                model.enabled = checked
            }
            binding.buttonDelete.visibility = if (model.removable) View.VISIBLE else View.GONE
            binding.buttonDelete.setOnClickListener(null)
            if (model.removable) {
                binding.buttonDelete.setOnClickListener {
                    addonViewModel.setAddonToDelete(model)
                }
            }
        }
    }
}
