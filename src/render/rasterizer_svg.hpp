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

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/manifest/manifest_validator.hpp"
#include "core/scene/scene.hpp"
#include "core/util/mat2d.hpp"

namespace naxgc {

constexpr double kSvgRasterPaddingPx = 2.0;
constexpr uint32_t kMaxLayerTextureDimension = 8192;

struct RasterizedBitmap {
	std::vector<uint8_t> bgra_premultiplied;
	uint32_t width{0};
	uint32_t height{0};
	Rect2D canvas_rect{};

	bool empty() const noexcept { return width == 0 || height == 0 || bgra_premultiplied.empty(); }
};

class SvgRasterizer {
public:
	SvgRasterizer() = default;
	explicit SvgRasterizer(const LoadedTemplate &tpl);

	/**
	 * Populates `rest_bbox` on every layer in `scene` using exact SVG geometry in canvas coordinates.
	 */
	void populate_scene_rest_bboxes(Scene &scene) const;

	/**
	 * Rasterizes a single `type: "svg"` layer into a tight-bbox premultiplied BGRA bitmap.
	 * Descendant elements that belong to child layers are excluded (`display="none"`),
	 * and `fill` / `stroke` / `strokeWidth` property overrides are applied.
	 */
	RasterizedBitmap rasterize_layer(const SceneLayer &layer, double canvas_scale_x = 1.0,
					 double canvas_scale_y = 1.0) const;

	/**
	 * Returns true if the template has visual SVG primitives not covered by any declared layer (NXG-W001).
	 */
	bool has_implicit_background() const noexcept { return has_implicit_background_; }

	/**
	 * Rasterizes the uncovered implicit static background elements (if any).
	 */
	RasterizedBitmap rasterize_implicit_background(double canvas_scale_x = 1.0, double canvas_scale_y = 1.0) const;

private:
	uint32_t canvas_width_{1920};
	uint32_t canvas_height_{1080};
	std::string svg_content_;
	SvgInspectionResult svg_info_;
	std::unordered_set<std::string> declared_layer_svg_ids_;
	std::unordered_set<std::string> text_or_image_frame_svg_ids_;
	std::unordered_map<std::string, Rect2D> computed_bboxes_by_svg_id_;
	bool has_implicit_background_{false};
};

} // namespace naxgc
