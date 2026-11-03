/*
NaxGC
Copyright (C) 2026 Bruno Cardoso <brunocardosofs@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "render/texture_cache.hpp"

#include <utility>

#include <obs.h>

namespace naxgc {

TextureCache::~TextureCache()
{
	clear();
}

TextureCache::TextureCache(TextureCache &&other) noexcept
	: slots_(std::move(other.slots_)),
	  implicit_background_(other.implicit_background_)
{
	other.implicit_background_ = LayerTextureSlot{};
}

TextureCache &TextureCache::operator=(TextureCache &&other) noexcept
{
	if (this != &other) {
		clear();
		slots_ = std::move(other.slots_);
		implicit_background_ = other.implicit_background_;
		other.implicit_background_ = LayerTextureSlot{};
	}
	return *this;
}

void TextureCache::reset(std::size_t layer_count)
{
	clear();
	slots_.resize(layer_count);
}

void TextureCache::clear() noexcept
{
	bool has_any_gpu_tex = (implicit_background_.texture != nullptr);
	if (!has_any_gpu_tex) {
		for (const auto &slot : slots_) {
			if (slot.texture != nullptr) {
				has_any_gpu_tex = true;
				break;
			}
		}
	}

	if (has_any_gpu_tex) {
		obs_enter_graphics();
		if (implicit_background_.texture != nullptr) {
			gs_texture_destroy(implicit_background_.texture);
			implicit_background_.texture = nullptr;
		}
		for (auto &slot : slots_) {
			if (slot.texture != nullptr) {
				gs_texture_destroy(slot.texture);
				slot.texture = nullptr;
			}
		}
		obs_leave_graphics();
	}

	slots_.clear();
	implicit_background_ = LayerTextureSlot{};
}

void TextureCache::upload_to_slot_in_graphics(LayerTextureSlot &slot, const RasterizedBitmap &bmp)
{
	slot.canvas_rect = bmp.canvas_rect;
	if (bmp.empty()) {
		if (slot.texture != nullptr) {
			gs_texture_destroy(slot.texture);
			slot.texture = nullptr;
		}
		slot.width = 0;
		slot.height = 0;
		slot.has_content = false;
		return;
	}

	if (slot.texture != nullptr && slot.width == bmp.width && slot.height == bmp.height) {
		gs_texture_set_image(slot.texture, bmp.bgra_premultiplied.data(), bmp.width * 4u, false);
		slot.has_content = true;
		return;
	}

	if (slot.texture != nullptr) {
		gs_texture_destroy(slot.texture);
		slot.texture = nullptr;
	}

	const uint8_t *data_ptr = bmp.bgra_premultiplied.data();
	slot.texture = gs_texture_create(bmp.width, bmp.height, GS_BGRA, 1, &data_ptr, GS_DYNAMIC);
	if (slot.texture != nullptr) {
		slot.width = bmp.width;
		slot.height = bmp.height;
		slot.has_content = true;
	} else {
		slot.width = 0;
		slot.height = 0;
		slot.has_content = false;
	}
}

void TextureCache::upload_in_graphics(std::size_t layer_index, const RasterizedBitmap &bmp)
{
	if (layer_index >= slots_.size()) {
		return;
	}
	upload_to_slot_in_graphics(slots_[layer_index], bmp);
}

void TextureCache::upload_implicit_background_in_graphics(const RasterizedBitmap &bmp)
{
	upload_to_slot_in_graphics(implicit_background_, bmp);
}

const LayerTextureSlot *TextureCache::get(std::size_t layer_index) const noexcept
{
	if (layer_index >= slots_.size()) {
		return nullptr;
	}
	return &slots_[layer_index];
}

} // namespace naxgc
