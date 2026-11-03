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

#include <string>
#include <unordered_map>
#include <vector>

#include "core/manifest/manifest_validator.hpp"
#include "core/scene/scene.hpp"
#include "render/rasterizer_svg.hpp"

namespace naxgc {

class TextRasterizer {
public:
	TextRasterizer() = default;
	explicit TextRasterizer(const LoadedTemplate &tpl);
	~TextRasterizer();

	TextRasterizer(const TextRasterizer &) = delete;
	TextRasterizer &operator=(const TextRasterizer &) = delete;

	TextRasterizer(TextRasterizer &&other) noexcept;
	TextRasterizer &operator=(TextRasterizer &&other) noexcept;

	/**
	 * Rasterizes a single `type: "text"` layer into a premultiplied BGRA bitmap
	 * matching its SVG frame bounding box (`layer.rest_bbox`).
	 */
	RasterizedBitmap rasterize_layer(const SceneLayer &layer, double canvas_scale_x = 1.0,
					 double canvas_scale_y = 1.0) const;

private:
	void unload_fonts() noexcept;

	std::vector<int> registered_font_ids_;
	std::unordered_map<std::string, std::string> family_alias_to_real_;
};

} // namespace naxgc
