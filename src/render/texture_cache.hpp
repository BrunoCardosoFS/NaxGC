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

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <graphics/graphics.h>

#include "core/util/mat2d.hpp"
#include "render/rasterizer_svg.hpp"

namespace naxgc {

struct LayerTextureSlot {
	gs_texture_t *texture{nullptr};
	uint32_t width{0};
	uint32_t height{0};
	Rect2D canvas_rect{};
	bool has_content{false};
};

class TextureCache {
public:
	TextureCache() = default;
	~TextureCache();

	TextureCache(const TextureCache &) = delete;
	TextureCache &operator=(const TextureCache &) = delete;

	TextureCache(TextureCache &&other) noexcept;
	TextureCache &operator=(TextureCache &&other) noexcept;

	/**
	 * Resizes the cache to hold `layer_count` slots, destroying any previous GPU textures safely.
	 */
	void reset(std::size_t layer_count);

	/**
	 * Releases all GPU textures safely (acquiring `obs_enter_graphics()`).
	 */
	void clear() noexcept;

	/**
	 * Uploads or updates the texture for `layer_index`.
	 * Must be called while already inside the OBS graphics context (`video_render` or `obs_enter_graphics`).
	 */
	void upload_in_graphics(std::size_t layer_index, const RasterizedBitmap &bmp);

	/**
	 * Uploads or updates the optional implicit static background texture.
	 * Must be called while already inside the OBS graphics context.
	 */
	void upload_implicit_background_in_graphics(const RasterizedBitmap &bmp);

	const LayerTextureSlot *get(std::size_t layer_index) const noexcept;
	const LayerTextureSlot &implicit_background() const noexcept { return implicit_background_; }

	std::size_t size() const noexcept { return slots_.size(); }

private:
	static void upload_to_slot_in_graphics(LayerTextureSlot &slot, const RasterizedBitmap &bmp);

	std::vector<LayerTextureSlot> slots_;
	LayerTextureSlot implicit_background_{};
};

} // namespace naxgc
