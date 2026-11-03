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

#include "core/manifest/svg_inspector.hpp"

#include <cctype>
#include <cstdlib>
#include <sstream>

#include "core/util/safe_path.hpp"

namespace naxgc {

namespace {

bool is_space(char c) noexcept
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool is_name_start_char(char c) noexcept
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == ':';
}

bool is_name_char(char c) noexcept
{
	return is_name_start_char(c) || (c >= '0' && c <= '9') || c == '-' || c == '.';
}

bool is_visual_primitive_tag(std::string_view tag) noexcept
{
	return tag == "rect" || tag == "path" || tag == "circle" || tag == "ellipse" || tag == "line" ||
	       tag == "polyline" || tag == "polygon" || tag == "image" || tag == "use" || tag == "text";
}

std::optional<double> parse_svg_number(std::string_view str)
{
	while (!str.empty() && is_space(str.front())) {
		str.remove_prefix(1);
	}
	while (!str.empty() && is_space(str.back())) {
		str.remove_suffix(1);
	}
	if (str.empty()) {
		return std::nullopt;
	}
	if (str.size() >= 2 && str.substr(str.size() - 2) == "px") {
		str.remove_suffix(2);
	}
	if (str.empty()) {
		return std::nullopt;
	}

	std::string owned(str);
	char *end_ptr = nullptr;
	const double val = std::strtod(owned.c_str(), &end_ptr);
	if (end_ptr == owned.c_str() || *end_ptr != '\0') {
		return std::nullopt;
	}
	return val;
}

bool parse_viewbox(std::string_view str, double &out_w, double &out_h)
{
	std::string normalized;
	normalized.reserve(str.size());
	for (char c : str) {
		normalized.push_back(c == ',' ? ' ' : c);
	}
	std::istringstream iss(normalized);
	double min_x = 0.0;
	double min_y = 0.0;
	double w = 0.0;
	double h = 0.0;
	if (!(iss >> min_x >> min_y >> w >> h)) {
		return false;
	}
	if (w <= 0.0 || h <= 0.0) {
		return false;
	}
	out_w = w;
	out_h = h;
	return true;
}

bool is_external_href(std::string_view href)
{
	while (!href.empty() && is_space(href.front())) {
		href.remove_prefix(1);
	}
	if (href.empty() || href[0] == '#' || href.rfind("data:", 0) == 0) {
		return false;
	}
	if (href.rfind("http://", 0) == 0 || href.rfind("https://", 0) == 0 || href.rfind("file:", 0) == 0) {
		return true;
	}
	return !is_safe_relative_path_syntax(href);
}

struct ActiveElement {
	std::string tag;
	std::string id;
};

} // namespace

Result<SvgInspectionResult> inspect_svg(std::string_view svg_content)
{
	if (!is_valid_utf8(svg_content)) {
		return Result<SvgInspectionResult>::err(Error{"NXG-E004", "SVG file is not valid UTF-8"});
	}

	SvgInspectionResult result;
	std::vector<ActiveElement> stack;
	bool found_svg_root = false;
	bool root_closed = false;

	std::size_t pos = 0;
	const std::size_t len = svg_content.size();

	// Skip UTF-8 BOM if present
	if (len >= 3 && static_cast<unsigned char>(svg_content[0]) == 0xEF &&
	    static_cast<unsigned char>(svg_content[1]) == 0xBB && static_cast<unsigned char>(svg_content[2]) == 0xBF) {
		pos = 3;
	}

	while (pos < len) {
		const std::size_t lt = svg_content.find('<', pos);
		if (lt == std::string_view::npos) {
			break;
		}
		pos = lt;
		if (pos + 1 >= len) {
			return Result<SvgInspectionResult>::err(Error{"NXG-E004", "unexpected end of SVG after '<'"});
		}

		// XML comment <!-- ... -->
		if (svg_content.substr(pos, 4) == "<!--") {
			const std::size_t close_comment = svg_content.find("-->", pos + 4);
			if (close_comment == std::string_view::npos) {
				return Result<SvgInspectionResult>::err(
					Error{"NXG-E004", "unclosed XML comment in SVG"});
			}
			pos = close_comment + 3;
			continue;
		}

		// Processing instruction <? ... ?>
		if (svg_content.substr(pos, 2) == "<?") {
			const std::size_t close_pi = svg_content.find("?>", pos + 2);
			if (close_pi == std::string_view::npos) {
				return Result<SvgInspectionResult>::err(
					Error{"NXG-E004", "unclosed XML processing instruction in SVG"});
			}
			pos = close_pi + 2;
			continue;
		}

		// CDATA <![CDATA[ ... ]]>
		if (svg_content.substr(pos, 9) == "<![CDATA[") {
			const std::size_t close_cdata = svg_content.find("]]>", pos + 9);
			if (close_cdata == std::string_view::npos) {
				return Result<SvgInspectionResult>::err(
					Error{"NXG-E004", "unclosed CDATA section in SVG"});
			}
			pos = close_cdata + 3;
			continue;
		}

		// DOCTYPE <!DOCTYPE ... >
		if (svg_content.substr(pos, 2) == "<!") {
			const std::size_t close_decl = svg_content.find('>', pos + 2);
			if (close_decl == std::string_view::npos) {
				return Result<SvgInspectionResult>::err(
					Error{"NXG-E004", "unclosed XML declaration in SVG"});
			}
			pos = close_decl + 1;
			continue;
		}

		// Closing tag </tag>
		if (svg_content[pos + 1] == '/') {
			pos += 2;
			const std::size_t name_start = pos;
			while (pos < len && is_name_char(svg_content[pos])) {
				++pos;
			}
			const std::string_view close_tag = svg_content.substr(name_start, pos - name_start);
			while (pos < len && is_space(svg_content[pos])) {
				++pos;
			}
			if (pos >= len || svg_content[pos] != '>' || close_tag.empty()) {
				return Result<SvgInspectionResult>::err(
					Error{"NXG-E004", "malformed closing tag in SVG"});
			}
			++pos;

			if (stack.empty() || stack.back().tag != close_tag) {
				return Result<SvgInspectionResult>::err(Error{
					"NXG-E004", "mismatched closing tag '</" + std::string(close_tag) + ">'"});
			}
			stack.pop_back();
			if (stack.empty() && found_svg_root) {
				root_closed = true;
			}
			continue;
		}

		// Opening tag <tag attr="val" ...> or <tag .../>
		++pos;
		if (pos >= len || !is_name_start_char(svg_content[pos])) {
			return Result<SvgInspectionResult>::err(Error{"NXG-E004", "invalid element tag name in SVG"});
		}
		const std::size_t tag_start = pos;
		while (pos < len && is_name_char(svg_content[pos])) {
			++pos;
		}
		std::string tag_name(svg_content.substr(tag_start, pos - tag_start));

		// Strip optional XML namespace prefix (e.g. svg:rect -> rect)
		const std::size_t colon = tag_name.find(':');
		const std::string local_tag = (colon == std::string::npos) ? tag_name : tag_name.substr(colon + 1);

		if (!found_svg_root) {
			if (local_tag != "svg") {
				return Result<SvgInspectionResult>::err(
					Error{"NXG-E004", "root element of layout file must be <svg>"});
			}
			found_svg_root = true;
		} else if (root_closed) {
			return Result<SvgInspectionResult>::err(
				Error{"NXG-E004", "extra element found after closing </svg> tag"});
		}

		std::unordered_map<std::string, std::string> attrs;
		bool self_closing = false;

		while (pos < len) {
			while (pos < len && is_space(svg_content[pos])) {
				++pos;
			}
			if (pos >= len) {
				return Result<SvgInspectionResult>::err(
					Error{"NXG-E004", "unclosed element tag in SVG"});
			}
			if (svg_content[pos] == '>') {
				++pos;
				break;
			}
			if (svg_content.substr(pos, 2) == "/>") {
				self_closing = true;
				pos += 2;
				break;
			}

			if (!is_name_start_char(svg_content[pos])) {
				return Result<SvgInspectionResult>::err(
					Error{"NXG-E004", "invalid attribute name in SVG"});
			}
			const std::size_t attr_start = pos;
			while (pos < len && is_name_char(svg_content[pos])) {
				++pos;
			}
			const std::string attr_name(svg_content.substr(attr_start, pos - attr_start));

			while (pos < len && is_space(svg_content[pos])) {
				++pos;
			}
			if (pos >= len || svg_content[pos] != '=') {
				return Result<SvgInspectionResult>::err(
					Error{"NXG-E004", "expected '=' after attribute '" + attr_name + "' in SVG"});
			}
			++pos;
			while (pos < len && is_space(svg_content[pos])) {
				++pos;
			}
			if (pos >= len || (svg_content[pos] != '"' && svg_content[pos] != '\'')) {
				return Result<SvgInspectionResult>::err(Error{
					"NXG-E004", "expected quoted value for attribute '" + attr_name + "' in SVG"});
			}
			const char quote = svg_content[pos];
			++pos;
			const std::size_t val_start = pos;
			const std::size_t val_end = svg_content.find(quote, val_start);
			if (val_end == std::string_view::npos) {
				return Result<SvgInspectionResult>::err(
					Error{"NXG-E004", "unclosed attribute value in SVG"});
			}
			attrs[attr_name] = std::string(svg_content.substr(val_start, val_end - val_start));
			pos = val_end + 1;
		}

		// Collect ancestor IDs
		std::vector<std::string> ancestor_ids;
		for (const auto &anc : stack) {
			if (!anc.id.empty()) {
				ancestor_ids.push_back(anc.id);
			}
		}

		// Root <svg> dimensions
		if (stack.empty() && local_tag == "svg") {
			auto w_it = attrs.find("width");
			if (w_it != attrs.end()) {
				result.width = parse_svg_number(w_it->second);
			}
			auto h_it = attrs.find("height");
			if (h_it != attrs.end()) {
				result.height = parse_svg_number(h_it->second);
			}
			auto vb_it = attrs.find("viewBox");
			if (vb_it != attrs.end()) {
				double vbw = 0.0;
				double vbh = 0.0;
				if (parse_viewbox(vb_it->second, vbw, vbh)) {
					result.viewbox_width = vbw;
					result.viewbox_height = vbh;
				}
			}
		}

		// Check external href / xlink:href
		for (const char *href_attr : {"href", "xlink:href"}) {
			auto h_it = attrs.find(href_attr);
			if (h_it != attrs.end() && is_external_href(h_it->second)) {
				result.external_hrefs.push_back(h_it->second);
			}
		}

		std::string elem_id;
		auto id_it = attrs.find("id");
		if (id_it != attrs.end() && !id_it->second.empty()) {
			elem_id = id_it->second;
			if (result.elements_by_id.find(elem_id) != result.elements_by_id.end()) {
				result.duplicate_ids.push_back(elem_id);
			} else {
				SvgElementInfo info;
				info.id = elem_id;
				info.tag = local_tag;
				info.ancestor_ids = ancestor_ids;
				auto w_it = attrs.find("width");
				if (w_it != attrs.end()) {
					info.width = parse_svg_number(w_it->second);
				}
				auto h_it = attrs.find("height");
				if (h_it != attrs.end()) {
					info.height = parse_svg_number(h_it->second);
				}
				result.ordered_ids.push_back(elem_id);
				result.elements_by_id.emplace(elem_id, std::move(info));
			}
		}

		if (is_visual_primitive_tag(local_tag)) {
			result.visual_primitives.push_back(SvgVisualNode{local_tag, elem_id, ancestor_ids});
		}

		if (!self_closing) {
			stack.push_back(ActiveElement{tag_name, elem_id});
		} else if (stack.empty() && found_svg_root) {
			root_closed = true;
		}
	}

	if (!found_svg_root) {
		return Result<SvgInspectionResult>::err(Error{"NXG-E004", "no <svg> root element found"});
	}

	if (!stack.empty()) {
		return Result<SvgInspectionResult>::err(
			Error{"NXG-E004", "unclosed tag '<" + stack.back().tag + ">' in SVG"});
	}

	return Result<SvgInspectionResult>::ok(std::move(result));
}

} // namespace naxgc
