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

#include "render/rasterizer_svg.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <lunasvg.h>
#include <utility>

#include "core/util/safe_path.hpp"

namespace naxgc {

namespace {

bool is_xml_space(char c) noexcept
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool is_xml_name_start(char c) noexcept
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == ':';
}

bool is_xml_name_char(char c) noexcept
{
	return is_xml_name_start(c) || (c >= '0' && c <= '9') || c == '-' || c == '.';
}

bool is_definition_tag(std::string_view local_tag) noexcept
{
	return local_tag == "defs" || local_tag == "style" || local_tag == "linearGradient" ||
	       local_tag == "radialGradient" || local_tag == "stop" || local_tag == "clipPath" ||
	       local_tag == "pattern" || local_tag == "mask" || local_tag == "filter" || local_tag == "symbol";
}

bool is_visual_shape_tag(std::string_view local_tag) noexcept
{
	return local_tag == "rect" || local_tag == "path" || local_tag == "circle" || local_tag == "ellipse" ||
	       local_tag == "line" || local_tag == "polyline" || local_tag == "polygon";
}

bool is_blocked_external_href(std::string_view href)
{
	while (!href.empty() && is_xml_space(href.front())) {
		href.remove_prefix(1);
	}
	if (href.empty() || href[0] == '#' || href.rfind("data:", 0) == 0) {
		return false;
	}
	return true;
}

std::string format_rgb_hex(const Color &c)
{
	char buf[8];
	std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", c.r, c.g, c.b);
	return std::string(buf);
}

std::string format_number(double val)
{
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%.4f", val);
	return std::string(buf);
}

struct ParsedAttr {
	std::string name;
	std::string value;
};

struct SvgFilterOptions {
	// If non-empty, only this target element (and its non-layer descendants) is visible.
	std::string target_id;
	std::unordered_set<std::string> target_ancestors;
	const std::unordered_set<std::string> *declared_layer_ids{nullptr};
	const std::unordered_set<std::string> *force_fill_ids{nullptr};

	// If true, hides all declared layers and renders only uncovered static primitives.
	bool implicit_background_mode{false};

	// Optional style overrides for the target element (and its non-layer descendants)
	std::optional<Color> fill_override;
	std::optional<Color> stroke_override;
	std::optional<double> stroke_width_override;
};

/**
 * Rewrites the SVG XML to isolate a single layer (or implicit background),
 * apply fill/stroke/strokeWidth overrides, and strip any external hrefs.
 */
std::string rewrite_svg_document(std::string_view svg_content, const SvgFilterOptions &opts)
{
	std::string out;
	out.reserve(svg_content.size() + 256);

	struct StackEntry {
		std::string local_tag;
		std::string id;
		bool in_defs{false};
		bool in_target{false};
		bool in_hidden_subtree{false};
	};

	std::vector<StackEntry> stack;
	std::size_t pos = 0;
	const std::size_t len = svg_content.size();

	while (pos < len) {
		const std::size_t lt = svg_content.find('<', pos);
		if (lt == std::string_view::npos) {
			out.append(svg_content.substr(pos));
			break;
		}
		if (lt > pos) {
			out.append(svg_content.substr(pos, lt - pos));
		}
		pos = lt;

		if (pos + 1 >= len) {
			out.push_back('<');
			break;
		}

		// Comments, processing instructions, CDATA, DOCTYPE
		if (svg_content.substr(pos, 4) == "<!--") {
			const std::size_t end_c = svg_content.find("-->", pos + 4);
			if (end_c == std::string_view::npos) {
				break;
			}
			pos = end_c + 3;
			continue;
		}
		if (svg_content.substr(pos, 2) == "<?") {
			const std::size_t end_pi = svg_content.find("?>", pos + 2);
			if (end_pi == std::string_view::npos) {
				break;
			}
			out.append(svg_content.substr(pos, end_pi + 2 - pos));
			pos = end_pi + 2;
			continue;
		}
		if (svg_content.substr(pos, 9) == "<![CDATA[") {
			const std::size_t end_cd = svg_content.find("]]>", pos + 9);
			if (end_cd == std::string_view::npos) {
				break;
			}
			out.append(svg_content.substr(pos, end_cd + 3 - pos));
			pos = end_cd + 3;
			continue;
		}
		if (svg_content.substr(pos, 2) == "<!") {
			const std::size_t end_dt = svg_content.find('>', pos + 2);
			if (end_dt == std::string_view::npos) {
				break;
			}
			out.append(svg_content.substr(pos, end_dt + 1 - pos));
			pos = end_dt + 1;
			continue;
		}

		// Closing tag </tag>
		if (svg_content[pos + 1] == '/') {
			const std::size_t close_gt = svg_content.find('>', pos + 2);
			if (close_gt == std::string_view::npos) {
				break;
			}
			out.append(svg_content.substr(pos, close_gt + 1 - pos));
			pos = close_gt + 1;
			if (!stack.empty()) {
				stack.pop_back();
			}
			continue;
		}

		// Opening tag
		std::size_t cur = pos + 1;
		while (cur < len && is_xml_name_char(svg_content[cur])) {
			++cur;
		}
		const std::string tag_name(svg_content.substr(pos + 1, cur - (pos + 1)));
		const std::size_t colon = tag_name.find(':');
		const std::string local_tag = (colon == std::string::npos) ? tag_name : tag_name.substr(colon + 1);

		std::vector<ParsedAttr> attrs;
		bool self_closing = false;

		while (cur < len) {
			while (cur < len && is_xml_space(svg_content[cur])) {
				++cur;
			}
			if (cur >= len) {
				break;
			}
			if (svg_content[cur] == '>') {
				++cur;
				break;
			}
			if (svg_content.substr(cur, 2) == "/>") {
				self_closing = true;
				cur += 2;
				break;
			}
			const std::size_t a_start = cur;
			while (cur < len && is_xml_name_char(svg_content[cur])) {
				++cur;
			}
			if (cur == a_start) {
				++cur;
				continue;
			}
			const std::string a_name(svg_content.substr(a_start, cur - a_start));
			while (cur < len && is_xml_space(svg_content[cur])) {
				++cur;
			}
			if (cur >= len || svg_content[cur] != '=') {
				continue;
			}
			++cur;
			while (cur < len && is_xml_space(svg_content[cur])) {
				++cur;
			}
			if (cur >= len || (svg_content[cur] != '"' && svg_content[cur] != '\'')) {
				continue;
			}
			const char quote = svg_content[cur++];
			const std::size_t v_start = cur;
			const std::size_t v_end = svg_content.find(quote, v_start);
			if (v_end == std::string_view::npos) {
				cur = len;
				break;
			}
			attrs.push_back(ParsedAttr{a_name, std::string(svg_content.substr(v_start, v_end - v_start))});
			cur = v_end + 1;
		}
		pos = cur;

		std::string elem_id;
		std::string orig_fill;
		std::string orig_stroke;
		for (const auto &attr : attrs) {
			if (attr.name == "id") {
				elem_id = attr.value;
			} else if (attr.name == "fill") {
				orig_fill = attr.value;
			} else if (attr.name == "stroke") {
				orig_stroke = attr.value;
			}
		}

		const bool parent_in_defs = !stack.empty() && stack.back().in_defs;
		const bool parent_in_target = !stack.empty() && stack.back().in_target;
		const bool parent_hidden = !stack.empty() && stack.back().in_hidden_subtree;

		const bool is_defs = parent_in_defs || is_definition_tag(local_tag);
		const bool is_target_self = (!opts.target_id.empty() && elem_id == opts.target_id);
		const bool is_target_ancestor = (!elem_id.empty() && opts.target_ancestors.count(elem_id) > 0);
		const bool is_other_declared_layer = (!is_target_self && !is_target_ancestor && !elem_id.empty() &&
						      opts.declared_layer_ids != nullptr &&
						      opts.declared_layer_ids->count(elem_id) > 0);

		const bool in_target = is_target_self || (parent_in_target && !is_other_declared_layer);

		bool hide_element = parent_hidden;
		if (!hide_element && local_tag != "svg" && !is_defs) {
			if (opts.implicit_background_mode) {
				if (opts.declared_layer_ids != nullptr && !elem_id.empty() &&
				    opts.declared_layer_ids->count(elem_id) > 0) {
					hide_element = true;
				}
			} else if (!opts.target_id.empty()) {
				if (is_other_declared_layer) {
					hide_element = true;
				} else if (!in_target && !is_target_ancestor) {
					hide_element = true;
				}
			}
		}

		auto set_or_replace_attr = [&](const std::string &name, const std::string &val) {
			for (auto &a : attrs) {
				if (a.name == name) {
					a.value = val;
					return;
				}
			}
			attrs.push_back(ParsedAttr{name, val});
		};

		// Remove blocked external hrefs
		attrs.erase(std::remove_if(attrs.begin(), attrs.end(),
					   [](const ParsedAttr &a) {
						   if (a.name == "href" || a.name == "xlink:href") {
							   return is_blocked_external_href(a.value);
						   }
						   return false;
					   }),
			    attrs.end());

		if (hide_element) {
			set_or_replace_attr("display", "none");
		} else if (opts.force_fill_ids != nullptr && !elem_id.empty() &&
			   opts.force_fill_ids->count(elem_id) > 0) {
			set_or_replace_attr("fill", "#ffffff");
		} else if (in_target && !hide_element && is_visual_shape_tag(local_tag)) {
			const bool apply_fill = opts.fill_override.has_value() &&
						(is_target_self || orig_fill != "none");
			if (apply_fill) {
				set_or_replace_attr("fill", format_rgb_hex(*opts.fill_override));
				if (opts.fill_override->a < 255) {
					set_or_replace_attr(
						"fill-opacity",
						format_number(static_cast<double>(opts.fill_override->a) / 255.0));
				}
			}

			const bool apply_stroke = opts.stroke_override.has_value() &&
						  (is_target_self || (!orig_stroke.empty() && orig_stroke != "none"));
			if (apply_stroke) {
				set_or_replace_attr("stroke", format_rgb_hex(*opts.stroke_override));
				if (opts.stroke_override->a < 255) {
					set_or_replace_attr(
						"stroke-opacity",
						format_number(static_cast<double>(opts.stroke_override->a) / 255.0));
				}
			}

			const bool apply_sw = opts.stroke_width_override.has_value() &&
					      (is_target_self || (!orig_stroke.empty() && orig_stroke != "none"));
			if (apply_sw) {
				set_or_replace_attr("stroke-width", format_number(*opts.stroke_width_override));
			}
		}

		out.push_back('<');
		out.append(tag_name);
		for (const auto &a : attrs) {
			out.push_back(' ');
			out.append(a.name);
			out.append("=\"");
			out.append(a.value);
			out.push_back('"');
		}
		if (self_closing) {
			out.append("/>");
		} else {
			out.push_back('>');
			stack.push_back(StackEntry{local_tag, elem_id, is_defs, in_target, hide_element});
		}
	}

	return out;
}

} // namespace

SvgRasterizer::SvgRasterizer(const LoadedTemplate &tpl)
	: canvas_width_(tpl.manifest.canvas.width),
	  canvas_height_(tpl.manifest.canvas.height),
	  svg_content_(tpl.svg_content),
	  svg_info_(tpl.svg_info)
{
	for (const auto &[lname, ldef] : tpl.manifest.layers) {
		(void)lname;
		if (!ldef.id.empty()) {
			declared_layer_svg_ids_.insert(ldef.id);
			if (ldef.type == LayerType::Text || ldef.type == LayerType::Image) {
				text_or_image_frame_svg_ids_.insert(ldef.id);
			}
		}
	}

	// Check if any visual primitive is uncovered (NXG-W001 implicit static background)
	for (const auto &prim : svg_info_.visual_primitives) {
		bool covered = (!prim.id.empty() && declared_layer_svg_ids_.count(prim.id) > 0);
		if (!covered) {
			for (const std::string &anc : prim.ancestor_ids) {
				if (declared_layer_svg_ids_.count(anc) > 0) {
					covered = true;
					break;
				}
			}
		}
		if (!covered) {
			has_implicit_background_ = true;
			break;
		}
	}

	// Probe bounding boxes of all declared layers in canvas coordinates.
	// For text/image frames (<rect id="txt_*" fill="none"/>), we force fill="#ffffff" in the probe
	// so lunasvg computes their exact transformed bounding box.
	SvgFilterOptions probe_opts;
	probe_opts.force_fill_ids = &text_or_image_frame_svg_ids_;
	const std::string probe_svg = rewrite_svg_document(svg_content_, probe_opts);

	auto probe_doc = lunasvg::Document::loadFromData(probe_svg.data(), probe_svg.size());
	const double doc_w = (probe_doc && probe_doc->width() > 0.0) ? probe_doc->width()
								     : static_cast<double>(canvas_width_);
	const double doc_h = (probe_doc && probe_doc->height() > 0.0) ? probe_doc->height()
								      : static_cast<double>(canvas_height_);
	const double sx = static_cast<double>(canvas_width_) / doc_w;
	const double sy = static_cast<double>(canvas_height_) / doc_h;

	for (const auto &[lname, ldef] : tpl.manifest.layers) {
		(void)lname;
		if (ldef.id.empty()) {
			continue;
		}

		// For SVG group layers that contain child layers, probe the isolated layer so child layers
		// don't artificially inflate the parent's own raster bbox.
		Rect2D bbox{};
		if (ldef.type == LayerType::Svg) {
			SvgFilterOptions iso_opts;
			iso_opts.target_id = ldef.id;
			auto elem_it = svg_info_.elements_by_id.find(ldef.id);
			if (elem_it != svg_info_.elements_by_id.end()) {
				iso_opts.target_ancestors.insert(elem_it->second.ancestor_ids.begin(),
								 elem_it->second.ancestor_ids.end());
			}
			iso_opts.declared_layer_ids = &declared_layer_svg_ids_;
			const std::string iso_svg = rewrite_svg_document(svg_content_, iso_opts);
			auto iso_doc = lunasvg::Document::loadFromData(iso_svg.data(), iso_svg.size());
			if (iso_doc) {
				auto elem = iso_doc->getElementById(ldef.id);
				if (!elem.isNull()) {
					const lunasvg::Box b = elem.getBBox().transformed(elem.getAbsoluteTransform());
					if (b.w > 0.0 && b.h > 0.0) {
						bbox = Rect2D{b.x * sx, b.y * sy, b.w * sx, b.h * sy};
					}
				}
			}
		}

		if (bbox.is_empty() && probe_doc) {
			auto elem = probe_doc->getElementById(ldef.id);
			if (!elem.isNull()) {
				const lunasvg::Box b = elem.getBBox().transformed(elem.getAbsoluteTransform());
				if (b.w > 0.0 && b.h > 0.0) {
					bbox = Rect2D{b.x * sx, b.y * sy, b.w * sx, b.h * sy};
				}
			}
		}

		if (bbox.is_empty()) {
			auto elem_it = svg_info_.elements_by_id.find(ldef.id);
			if (elem_it != svg_info_.elements_by_id.end()) {
				const SvgElementInfo &info = elem_it->second;
				bbox = Rect2D{
					info.x.value_or(0.0) * sx,
					info.y.value_or(0.0) * sy,
					info.width.value_or(0.0) * sx,
					info.height.value_or(0.0) * sy,
				};
			}
		}

		if (!bbox.is_empty()) {
			computed_bboxes_by_svg_id_.emplace(ldef.id, bbox);
		}
	}
}

void SvgRasterizer::populate_scene_rest_bboxes(Scene &scene) const
{
	for (std::size_t i = 0; i < scene.layers().size(); ++i) {
		const SceneLayer &layer = scene.layers()[i];
		auto it = computed_bboxes_by_svg_id_.find(layer.svg_id);
		if (it != computed_bboxes_by_svg_id_.end()) {
			scene.set_layer_rest_bbox(i, it->second);
		}
	}
}

RasterizedBitmap SvgRasterizer::rasterize_layer(const SceneLayer &layer, double canvas_scale_x,
						double canvas_scale_y) const
{
	RasterizedBitmap out;
	if (layer.type != LayerType::Svg || layer.svg_id.empty()) {
		return out;
	}

	SvgFilterOptions opts;
	opts.target_id = layer.svg_id;
	auto elem_it = svg_info_.elements_by_id.find(layer.svg_id);
	if (elem_it != svg_info_.elements_by_id.end()) {
		opts.target_ancestors.insert(elem_it->second.ancestor_ids.begin(), elem_it->second.ancestor_ids.end());
	}
	opts.declared_layer_ids = &declared_layer_svg_ids_;
	opts.fill_override = layer.props.fill.effective();
	opts.stroke_override = layer.props.stroke.effective();
	opts.stroke_width_override = layer.props.stroke_width.effective();

	const std::string isolated_svg = rewrite_svg_document(svg_content_, opts);
	auto doc = lunasvg::Document::loadFromData(isolated_svg.data(), isolated_svg.size());
	if (!doc || doc->width() <= 0.0 || doc->height() <= 0.0) {
		return out;
	}

	auto elem = doc->getElementById(layer.svg_id);
	if (elem.isNull()) {
		return out;
	}

	const lunasvg::Box raw_box = elem.getBBox().transformed(elem.getAbsoluteTransform());
	if (raw_box.w <= 0.0 || raw_box.h <= 0.0) {
		return out;
	}

	const double svg_to_canvas_x = static_cast<double>(canvas_width_) / doc->width();
	const double svg_to_canvas_y = static_cast<double>(canvas_height_) / doc->height();

	const double cx = raw_box.x * svg_to_canvas_x;
	const double cy = raw_box.y * svg_to_canvas_y;
	const double cw = raw_box.w * svg_to_canvas_x;
	const double ch = raw_box.h * svg_to_canvas_y;

	// Snap padded rect to integer canvas coordinates so 1:1 renders have zero resampling blur
	const double rx = std::floor(cx - kSvgRasterPaddingPx);
	const double ry = std::floor(cy - kSvgRasterPaddingPx);
	const double rw = std::ceil(cx + cw + kSvgRasterPaddingPx) - rx;
	const double rh = std::ceil(cy + ch + kSvgRasterPaddingPx) - ry;
	if (rw <= 0.0 || rh <= 0.0) {
		return out;
	}

	const double eff_sx = std::max(0.05, canvas_scale_x * layer.effective_raster_scale);
	const double eff_sy = std::max(0.05, canvas_scale_y * layer.effective_raster_scale);

	const uint32_t bmp_w =
		std::clamp(static_cast<uint32_t>(std::lround(rw * eff_sx)), 1u, kMaxLayerTextureDimension);
	const uint32_t bmp_h =
		std::clamp(static_cast<uint32_t>(std::lround(rh * eff_sy)), 1u, kMaxLayerTextureDimension);

	lunasvg::Bitmap bitmap(bmp_w, bmp_h);
	if (!bitmap.valid()) {
		return out;
	}
	bitmap.clear(0x00000000);

	// Map SVG document space into the tight bitmap [0..bmp_w, 0..bmp_h]
	const double pixel_per_canvas_x = static_cast<double>(bmp_w) / rw;
	const double pixel_per_canvas_y = static_cast<double>(bmp_h) / rh;
	const double mat_sx = svg_to_canvas_x * pixel_per_canvas_x;
	const double mat_sy = svg_to_canvas_y * pixel_per_canvas_y;
	const double mat_tx = -rx * pixel_per_canvas_x;
	const double mat_ty = -ry * pixel_per_canvas_y;

	const lunasvg::Matrix matrix(mat_sx, 0.0, 0.0, mat_sy, mat_tx, mat_ty);
	doc->render(bitmap, matrix);

	const std::size_t byte_count = static_cast<std::size_t>(bmp_w) * static_cast<std::size_t>(bmp_h) * 4u;
	out.bgra_premultiplied.resize(byte_count);
	std::memcpy(out.bgra_premultiplied.data(), bitmap.data(), byte_count);
	out.width = bmp_w;
	out.height = bmp_h;
	out.canvas_rect = Rect2D{rx, ry, rw, rh};
	return out;
}

RasterizedBitmap SvgRasterizer::rasterize_implicit_background(double canvas_scale_x, double canvas_scale_y) const
{
	RasterizedBitmap out;
	if (!has_implicit_background_) {
		return out;
	}

	SvgFilterOptions opts;
	opts.declared_layer_ids = &declared_layer_svg_ids_;
	opts.implicit_background_mode = true;

	const std::string bg_svg = rewrite_svg_document(svg_content_, opts);
	auto doc = lunasvg::Document::loadFromData(bg_svg.data(), bg_svg.size());
	if (!doc || doc->width() <= 0.0 || doc->height() <= 0.0) {
		return out;
	}

	const double cw = static_cast<double>(canvas_width_);
	const double ch = static_cast<double>(canvas_height_);
	const uint32_t bmp_w = std::clamp(static_cast<uint32_t>(std::lround(cw * std::max(0.05, canvas_scale_x))), 1u,
					  kMaxLayerTextureDimension);
	const uint32_t bmp_h = std::clamp(static_cast<uint32_t>(std::lround(ch * std::max(0.05, canvas_scale_y))), 1u,
					  kMaxLayerTextureDimension);

	lunasvg::Bitmap bitmap(bmp_w, bmp_h);
	if (!bitmap.valid()) {
		return out;
	}
	bitmap.clear(0x00000000);

	const lunasvg::Matrix matrix(static_cast<double>(bmp_w) / doc->width(), 0.0, 0.0,
				     static_cast<double>(bmp_h) / doc->height(), 0.0, 0.0);
	doc->render(bitmap, matrix);

	const std::size_t byte_count = static_cast<std::size_t>(bmp_w) * static_cast<std::size_t>(bmp_h) * 4u;
	out.bgra_premultiplied.resize(byte_count);
	std::memcpy(out.bgra_premultiplied.data(), bitmap.data(), byte_count);
	out.width = bmp_w;
	out.height = bmp_h;
	out.canvas_rect = Rect2D{0.0, 0.0, cw, ch};
	return out;
}

} // namespace naxgc
