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
#include <memory>

#include <graphics/graphics.h>

#include "core/manifest/manifest_validator.hpp"
#include "core/scene/scene.hpp"
#include "render/rasterizer_svg.hpp"
#include "render/rasterizer_text.hpp"
#include "render/texrender_pool.hpp"
#include "render/texture_cache.hpp"

namespace naxgc {

constexpr std::size_t kDefaultMaxRerastersPerFrame = 64;

class Renderer {
public:
	Renderer() = default;
	~Renderer();

	Renderer(const Renderer &) = delete;
	Renderer &operator=(const Renderer &) = delete;

	/**
	 * Loads a validated template into the renderer, building the Scene, SvgRasterizer,
	 * and TextRasterizer, and marking all layers dirty for initial rasterization.
	 */
	void set_template(std::shared_ptr<const LoadedTemplate> tpl, bool apply_initial_state = false);

	/**
	 * Unloads the current template and frees all GPU textures.
	 */
	void clear();

	bool has_template() const noexcept { return active_template_ != nullptr; }

	Scene &scene() noexcept { return scene_; }
	const Scene &scene() const noexcept { return scene_; }

	/**
	 * Re-rasterizes any layers marked `content_dirty` (up to `max_layers_per_frame`)
	 * and uploads their updated bitmaps to `TextureCache`.
	 * Must be called inside the OBS graphics context (`video_render` or `obs_enter_graphics`).
	 */
	void update_dirty_layers_in_graphics(uint32_t output_width, uint32_t output_height,
					     std::size_t max_layers_per_frame = kDefaultMaxRerastersPerFrame);

	/**
	 * Draws all visible non-mask layers in SVG z-order using GPU matrices and premultiplied alpha blending.
	 * Must be called inside `video_render`.
	 */
	void render(uint32_t output_width, uint32_t output_height);

private:
	bool ensure_effect_in_graphics();
	bool ensure_mask_effect_in_graphics();
	void draw_slot_with_matrix(const LayerTextureSlot &slot, const Mat2D &canvas_to_output,
				   const Mat2D &world_matrix, double opacity, bool linear_srgb);
	void draw_masked_layer(std::size_t layer_index, uint32_t output_width, uint32_t output_height,
			       const Mat2D &canvas_to_output, bool linear_srgb);
	void draw_mask_composite(gs_texture_t *layer_tex, gs_texture_t *mask_tex, uint32_t width, uint32_t height,
				 MaskMode mode, bool linear_srgb);

	std::shared_ptr<const LoadedTemplate> active_template_;
	Scene scene_;
	SvgRasterizer svg_rasterizer_;
	TextRasterizer text_rasterizer_;
	TextureCache texture_cache_;
	TexrenderPool texrender_pool_;

	bool implicit_bg_dirty_{false};
	double last_canvas_scale_x_{1.0};
	double last_canvas_scale_y_{1.0};

	gs_effect_t *layer_effect_{nullptr};
	gs_eparam_t *image_param_{nullptr};
	gs_eparam_t *opacity_param_{nullptr};

	gs_effect_t *mask_effect_{nullptr};
	gs_eparam_t *mask_image_param_{nullptr};
	gs_eparam_t *mask_target_param_{nullptr};
};

} // namespace naxgc
