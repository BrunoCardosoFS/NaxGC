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

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/util/result.hpp"

namespace naxgc {

struct SvgElementInfo {
	std::string id;
	std::string tag;
	std::vector<std::string> ancestor_ids;
	std::optional<double> x;
	std::optional<double> y;
	std::optional<double> width;
	std::optional<double> height;
};

struct SvgVisualNode {
	std::string tag;
	std::string id;
	std::vector<std::string> ancestor_ids;
};

struct SvgInspectionResult {
	std::optional<double> width;
	std::optional<double> height;
	std::optional<double> viewbox_width;
	std::optional<double> viewbox_height;
	std::vector<std::string> ordered_ids;
	std::vector<std::string> duplicate_ids;
	std::unordered_map<std::string, SvgElementInfo> elements_by_id;
	std::vector<SvgVisualNode> visual_primitives;
	std::vector<std::string> external_hrefs;
};

/**
 * Parses and inspects an SVG document for M2 validation:
 * - XML well-formedness and root <svg> presence (NXG-E004)
 * - Root width/height/viewBox extraction (NXG-W005)
 * - Element IDs in z-order, ancestor ID hierarchy, and duplicate IDs (NXG-E010, NXG-E011)
 * - Visual leaf primitives for implicit static background check (NXG-W001)
 * - Bounding width/height of <rect> frames for text overflow estimation (NXG-W003)
 */
Result<SvgInspectionResult> inspect_svg(std::string_view svg_content);

} // namespace naxgc
