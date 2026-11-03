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

#include "core/manifest/manifest_parser.hpp"

#include <algorithm>
#include <initializer_list>
#include <string>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "core/util/safe_path.hpp"

namespace naxgc {

namespace {

using json = nlohmann::json;

void add_error(std::vector<Diagnostic> &diags, std::string code, std::string path, std::string msg)
{
	diags.push_back(Diagnostic{DiagnosticSeverity::Error, std::move(code), std::move(path), std::move(msg)});
}

void add_warning(std::vector<Diagnostic> &diags, std::string code, std::string path, std::string msg)
{
	diags.push_back(Diagnostic{DiagnosticSeverity::Warning, std::move(code), std::move(path), std::move(msg)});
}

std::string join_path(std::string_view base, std::string_view child)
{
	if (base.empty()) {
		return std::string(child);
	}
	std::string out(base);
	out.push_back('.');
	out.append(child);
	return out;
}

std::string index_path(std::string_view base, std::size_t idx)
{
	std::string out(base);
	out.push_back('[');
	out.append(std::to_string(idx));
	out.push_back(']');
	return out;
}

bool is_extension_field(std::string_view key) noexcept
{
	return key.size() >= 2 && key[0] == 'x' && key[1] == '-';
}

void check_unknown_fields(const json &obj, std::string_view base_path, std::initializer_list<std::string_view> allowed,
			  std::vector<Diagnostic> &diags, bool allow_dollar_schema = false)
{
	for (auto it = obj.begin(); it != obj.end(); ++it) {
		const std::string &key = it.key();
		if (is_extension_field(key)) {
			continue;
		}
		if (allow_dollar_schema && key == "$schema") {
			continue;
		}
		bool found = false;
		for (std::string_view allowed_key : allowed) {
			if (key == allowed_key) {
				found = true;
				break;
			}
		}
		if (!found) {
			add_warning(diags, "NXG-W004", join_path(base_path, key),
				    "unknown field '" + key + "' (ignored)");
		}
	}
}

bool is_known_layer_specific_field(std::string_view key) noexcept
{
	return key == "fill" || key == "stroke" || key == "strokeWidth" || key == "text" || key == "font" ||
	       key == "weight" || key == "italic" || key == "size" || key == "minSize" || key == "color" ||
	       key == "align" || key == "valign" || key == "overflow" || key == "maxLines" || key == "lineHeight" ||
	       key == "letterSpacing" || key == "uppercase" || key == "src" || key == "fit";
}

std::optional<PropertyValue> parse_raw_json_value(const json &val)
{
	if (val.is_boolean()) {
		return PropertyValue{val.get<bool>()};
	}
	if (val.is_number()) {
		return PropertyValue{val.get<double>()};
	}
	if (val.is_string()) {
		return PropertyValue{val.get<std::string>()};
	}
	return std::nullopt;
}

std::optional<EasingSpec> parse_easing(const json &val, std::string_view path, std::vector<Diagnostic> &diags)
{
	EasingSpec spec;
	if (val.is_string()) {
		const std::string name = val.get<std::string>();
		if (name == "linear") {
			spec.type = EasingType::Linear;
		} else if (name == "inQuad") {
			spec.type = EasingType::InQuad;
		} else if (name == "outQuad") {
			spec.type = EasingType::OutQuad;
		} else if (name == "inOutQuad") {
			spec.type = EasingType::InOutQuad;
		} else if (name == "inCubic") {
			spec.type = EasingType::InCubic;
		} else if (name == "outCubic") {
			spec.type = EasingType::OutCubic;
		} else if (name == "inOutCubic") {
			spec.type = EasingType::InOutCubic;
		} else if (name == "inSine") {
			spec.type = EasingType::InSine;
		} else if (name == "outSine") {
			spec.type = EasingType::OutSine;
		} else if (name == "inOutSine") {
			spec.type = EasingType::InOutSine;
		} else if (name == "inExpo") {
			spec.type = EasingType::InExpo;
		} else if (name == "outExpo") {
			spec.type = EasingType::OutExpo;
		} else if (name == "inOutExpo") {
			spec.type = EasingType::InOutExpo;
		} else if (name == "inBack") {
			spec.type = EasingType::InBack;
		} else if (name == "outBack") {
			spec.type = EasingType::OutBack;
		} else if (name == "inOutBack") {
			spec.type = EasingType::InOutBack;
		} else if (name == "outElastic") {
			spec.type = EasingType::OutElastic;
		} else if (name == "outBounce") {
			spec.type = EasingType::OutBounce;
		} else {
			add_error(diags, "NXG-E024", std::string(path), "unknown easing '" + name + "'");
			return std::nullopt;
		}
		return spec;
	}

	if (val.is_array() && val.size() == 4) {
		for (std::size_t i = 0; i < 4; ++i) {
			if (!val[i].is_number()) {
				add_error(diags, "NXG-E024", std::string(path),
					  "cubic-bezier elements must be numbers");
				return std::nullopt;
			}
			spec.bezier[i] = val[i].get<double>();
		}
		if (spec.bezier[0] < 0.0 || spec.bezier[0] > 1.0 || spec.bezier[2] < 0.0 || spec.bezier[2] > 1.0) {
			add_error(diags, "NXG-E024", std::string(path), "cubic-bezier x1 and x2 must be in [0, 1]");
			return std::nullopt;
		}
		spec.type = EasingType::CubicBezier;
		return spec;
	}

	add_error(diags, "NXG-E024", std::string(path), "ease must be an easing name or [x1, y1, x2, y2] array");
	return std::nullopt;
}

void parse_meta(const json &root, Manifest &manifest, std::vector<Diagnostic> &diags)
{
	auto it = root.find("meta");
	if (it == root.end() || !it->is_object()) {
		add_error(diags, "NXG-E005", "meta", "missing or invalid required object 'meta'");
		return;
	}

	const json &meta = *it;
	check_unknown_fields(meta, "meta", {"id", "name", "version", "author", "description", "requires"}, diags);

	auto id_it = meta.find("id");
	if (id_it == meta.end() || !id_it->is_string()) {
		add_error(diags, "NXG-E005", "meta.id", "missing or non-string 'meta.id'");
	} else {
		manifest.meta.id = id_it->get<std::string>();
		if (!is_valid_meta_id(manifest.meta.id)) {
			add_error(diags, "NXG-E005", "meta.id", "'meta.id' must match [a-z0-9.-]+");
		}
	}

	auto name_it = meta.find("name");
	if (name_it == meta.end() || !name_it->is_string() || name_it->get<std::string>().empty()) {
		add_error(diags, "NXG-E005", "meta.name", "missing or empty required string 'meta.name'");
	} else {
		manifest.meta.name = name_it->get<std::string>();
	}

	for (const char *opt_str : {"version", "author", "description"}) {
		auto opt_it = meta.find(opt_str);
		if (opt_it != meta.end()) {
			if (!opt_it->is_string()) {
				add_error(diags, "NXG-E005", join_path("meta", opt_str),
					  std::string("'") + opt_str + "' must be a string");
			} else if (std::string_view(opt_str) == "version") {
				manifest.meta.version = opt_it->get<std::string>();
			} else if (std::string_view(opt_str) == "author") {
				manifest.meta.author = opt_it->get<std::string>();
			} else {
				manifest.meta.description = opt_it->get<std::string>();
			}
		}
	}

	auto req_it = meta.find("requires");
	if (req_it != meta.end()) {
		if (!req_it->is_object()) {
			add_error(diags, "NXG-E005", "meta.requires", "'meta.requires' must be an object");
		} else {
			check_unknown_fields(*req_it, "meta.requires", {"naxgc"}, diags);
			auto nax_it = req_it->find("naxgc");
			if (nax_it != req_it->end()) {
				if (!nax_it->is_string()) {
					add_error(diags, "NXG-E005", "meta.requires.naxgc",
						  "'meta.requires.naxgc' must be a string");
				} else {
					manifest.meta.requires_naxgc = nax_it->get<std::string>();
				}
			}
		}
	}
}

void parse_canvas(const json &root, Manifest &manifest, std::vector<Diagnostic> &diags)
{
	auto it = root.find("canvas");
	if (it == root.end() || !it->is_object()) {
		add_error(diags, "NXG-E005", "canvas", "missing or invalid required object 'canvas'");
		return;
	}

	const json &canvas = *it;
	check_unknown_fields(canvas, "canvas", {"width", "height", "fps"}, diags);

	auto w_it = canvas.find("width");
	if (w_it == canvas.end() || !w_it->is_number_integer() || w_it->get<int64_t>() <= 0) {
		add_error(diags, "NXG-E005", "canvas.width", "'canvas.width' must be a positive integer");
	} else {
		manifest.canvas.width = static_cast<uint32_t>(w_it->get<int64_t>());
	}

	auto h_it = canvas.find("height");
	if (h_it == canvas.end() || !h_it->is_number_integer() || h_it->get<int64_t>() <= 0) {
		add_error(diags, "NXG-E005", "canvas.height", "'canvas.height' must be a positive integer");
	} else {
		manifest.canvas.height = static_cast<uint32_t>(h_it->get<int64_t>());
	}

	auto fps_it = canvas.find("fps");
	if (fps_it != canvas.end()) {
		if (!fps_it->is_number() || fps_it->get<double>() <= 0.0) {
			add_error(diags, "NXG-E005", "canvas.fps", "'canvas.fps' must be a positive number");
		} else {
			manifest.canvas.fps = fps_it->get<double>();
		}
	}
}

void parse_fonts(const json &root, Manifest &manifest, std::vector<Diagnostic> &diags)
{
	auto it = root.find("fonts");
	if (it == root.end()) {
		return;
	}
	if (!it->is_array()) {
		add_error(diags, "NXG-E005", "fonts", "'fonts' must be an array");
		return;
	}

	for (std::size_t i = 0; i < it->size(); ++i) {
		const std::string item_path = index_path("fonts", i);
		const json &f = (*it)[i];
		if (!f.is_object()) {
			add_error(diags, "NXG-E005", item_path, "font entry must be an object");
			continue;
		}
		check_unknown_fields(f, item_path, {"family", "file"}, diags);

		FontDef def;
		def.json_path = item_path;

		auto fam_it = f.find("family");
		if (fam_it == f.end() || !fam_it->is_string() || fam_it->get<std::string>().empty()) {
			add_error(diags, "NXG-E005", join_path(item_path, "family"),
				  "font 'family' must be a non-empty string");
		} else {
			def.family = fam_it->get<std::string>();
		}

		auto file_it = f.find("file");
		if (file_it == f.end() || !file_it->is_string() || file_it->get<std::string>().empty()) {
			add_error(diags, "NXG-E005", join_path(item_path, "file"),
				  "font 'file' must be a non-empty string");
		} else {
			def.file = file_it->get<std::string>();
		}

		manifest.fonts.push_back(std::move(def));
	}
}

void parse_single_layer(const std::string &name, const json &obj, Manifest &manifest, std::vector<Diagnostic> &diags)
{
	const std::string layer_path = join_path("layers", name);
	if (!is_valid_identifier(name)) {
		add_error(diags, "NXG-E005", layer_path, "invalid layer name '" + name + "'");
	}
	if (!obj.is_object()) {
		add_error(diags, "NXG-E005", layer_path, "layer definition must be an object");
		return;
	}

	LayerDef layer;
	layer.name = name;
	layer.json_path = layer_path;

	auto type_it = obj.find("type");
	if (type_it == obj.end() || !type_it->is_string()) {
		add_error(diags, "NXG-E005", join_path(layer_path, "type"), "missing or non-string layer 'type'");
		return;
	}

	const std::string type_str = type_it->get<std::string>();
	if (type_str == "svg") {
		layer.type = LayerType::Svg;
	} else if (type_str == "text") {
		layer.type = LayerType::Text;
	} else if (type_str == "image") {
		layer.type = LayerType::Image;
	} else {
		add_error(diags, "NXG-E013", join_path(layer_path, "type"), "unknown layer type '" + type_str + "'");
		return;
	}

	// Check unknown or incompatible fields on the layer object
	const std::unordered_set<std::string_view> common_fields = {
		"type", "id", "role", "origin", "opacity", "visible", "mask", "maskMode", "rasterScale"};
	std::unordered_set<std::string_view> type_fields;
	if (layer.type == LayerType::Svg) {
		type_fields = {"fill", "stroke", "strokeWidth"};
	} else if (layer.type == LayerType::Text) {
		type_fields = {"text",  "font",   "weight",   "italic",   "size",       "minSize",       "color",
			       "align", "valign", "overflow", "maxLines", "lineHeight", "letterSpacing", "uppercase"};
	} else if (layer.type == LayerType::Image) {
		type_fields = {"src", "fit"};
	}

	for (auto it = obj.begin(); it != obj.end(); ++it) {
		const std::string &k = it.key();
		if (is_extension_field(k) || common_fields.count(k) > 0 || type_fields.count(k) > 0) {
			continue;
		}
		if (is_known_layer_specific_field(k)) {
			add_error(diags, "NXG-E023", join_path(layer_path, k),
				  "property '" + k + "' is incompatible with layer type '" + type_str + "'");
		} else {
			add_warning(diags, "NXG-W004", join_path(layer_path, k), "unknown field '" + k + "' (ignored)");
		}
	}

	auto id_it = obj.find("id");
	if (id_it == obj.end() || !id_it->is_string() || id_it->get<std::string>().empty()) {
		add_error(diags, "NXG-E005", join_path(layer_path, "id"), "layer 'id' must be a non-empty string");
	} else {
		layer.id = id_it->get<std::string>();
	}

	auto role_it = obj.find("role");
	if (role_it != obj.end()) {
		if (!role_it->is_string() || role_it->get<std::string>() != "mask") {
			add_error(diags, "NXG-E005", join_path(layer_path, "role"), "layer 'role' must be \"mask\"");
		} else {
			layer.role = LayerRole::Mask;
		}
	}

	auto origin_it = obj.find("origin");
	if (origin_it != obj.end()) {
		if (!origin_it->is_array() || origin_it->size() != 2 || !(*origin_it)[0].is_number() ||
		    !(*origin_it)[1].is_number()) {
			add_error(diags, "NXG-E005", join_path(layer_path, "origin"),
				  "'origin' must be an array of 2 numbers [ox, oy]");
		} else {
			const double ox = (*origin_it)[0].get<double>();
			const double oy = (*origin_it)[1].get<double>();
			if (ox < 0.0 || ox > 1.0 || oy < 0.0 || oy > 1.0) {
				add_error(diags, "NXG-E005", join_path(layer_path, "origin"),
					  "'origin' coordinates must be between 0 and 1");
			} else {
				layer.origin = {ox, oy};
			}
		}
	}

	auto op_it = obj.find("opacity");
	if (op_it != obj.end()) {
		if (!op_it->is_number() || op_it->get<double>() < 0.0 || op_it->get<double>() > 1.0) {
			add_error(diags, "NXG-E005", join_path(layer_path, "opacity"),
				  "'opacity' must be a number in [0, 1]");
		} else {
			layer.opacity = op_it->get<double>();
		}
	}

	auto vis_it = obj.find("visible");
	if (vis_it != obj.end()) {
		if (!vis_it->is_boolean()) {
			add_error(diags, "NXG-E005", join_path(layer_path, "visible"), "'visible' must be a boolean");
		} else {
			layer.visible = vis_it->get<bool>();
		}
	}

	auto mask_it = obj.find("mask");
	if (mask_it != obj.end()) {
		if (!mask_it->is_string() || mask_it->get<std::string>().empty()) {
			add_error(diags, "NXG-E005", join_path(layer_path, "mask"),
				  "'mask' must be a non-empty string");
		} else {
			layer.mask = mask_it->get<std::string>();
		}
	}

	auto mm_it = obj.find("maskMode");
	if (mm_it != obj.end()) {
		if (!mm_it->is_string()) {
			add_error(diags, "NXG-E005", join_path(layer_path, "maskMode"), "'maskMode' must be a string");
		} else {
			const std::string mm = mm_it->get<std::string>();
			if (mm == "alpha") {
				layer.mask_mode = MaskMode::Alpha;
			} else if (mm == "inverse") {
				layer.mask_mode = MaskMode::Inverse;
			} else {
				add_error(diags, "NXG-E005", join_path(layer_path, "maskMode"),
					  "'maskMode' must be \"alpha\" or \"inverse\"");
			}
		}
	}

	auto rs_it = obj.find("rasterScale");
	if (rs_it != obj.end()) {
		if (!rs_it->is_number() || rs_it->get<double>() <= 0.0) {
			add_error(diags, "NXG-E005", join_path(layer_path, "rasterScale"),
				  "'rasterScale' must be a positive number");
		} else {
			layer.raster_scale = rs_it->get<double>();
		}
	}

	if (layer.type == LayerType::Svg) {
		for (const char *col_field : {"fill", "stroke"}) {
			auto c_it = obj.find(col_field);
			if (c_it != obj.end()) {
				if (!c_it->is_string()) {
					add_error(diags, "NXG-E005", join_path(layer_path, col_field),
						  std::string("'") + col_field + "' must be a color string");
				} else {
					auto parsed = parse_color(c_it->get<std::string>());
					if (parsed.is_err()) {
						add_error(diags, "NXG-E005", join_path(layer_path, col_field),
							  parsed.error().message);
					} else if (std::string_view(col_field) == "fill") {
						layer.fill = parsed.value();
					} else {
						layer.stroke = parsed.value();
					}
				}
			}
		}
		auto sw_it = obj.find("strokeWidth");
		if (sw_it != obj.end()) {
			if (!sw_it->is_number() || sw_it->get<double>() < 0.0) {
				add_error(diags, "NXG-E005", join_path(layer_path, "strokeWidth"),
					  "'strokeWidth' must be a non-negative number");
			} else {
				layer.stroke_width = sw_it->get<double>();
			}
		}
	} else if (layer.type == LayerType::Text) {
		auto txt_it = obj.find("text");
		if (txt_it != obj.end()) {
			if (!txt_it->is_string()) {
				add_error(diags, "NXG-E005", join_path(layer_path, "text"), "'text' must be a string");
			} else {
				layer.text = txt_it->get<std::string>();
			}
		}
		auto font_it = obj.find("font");
		if (font_it != obj.end()) {
			if (!font_it->is_string()) {
				add_error(diags, "NXG-E005", join_path(layer_path, "font"), "'font' must be a string");
			} else {
				layer.font = font_it->get<std::string>();
			}
		}
		auto weight_it = obj.find("weight");
		if (weight_it != obj.end()) {
			if (!weight_it->is_number_integer() || weight_it->get<int>() < 100 ||
			    weight_it->get<int>() > 900) {
				add_error(diags, "NXG-E005", join_path(layer_path, "weight"),
					  "'weight' must be an integer between 100 and 900");
			} else {
				layer.weight = weight_it->get<int>();
			}
		}
		auto italic_it = obj.find("italic");
		if (italic_it != obj.end()) {
			if (!italic_it->is_boolean()) {
				add_error(diags, "NXG-E005", join_path(layer_path, "italic"),
					  "'italic' must be a boolean");
			} else {
				layer.italic = italic_it->get<bool>();
			}
		}
		auto size_it = obj.find("size");
		if (size_it != obj.end()) {
			if (!size_it->is_number() || size_it->get<double>() <= 0.0) {
				add_error(diags, "NXG-E005", join_path(layer_path, "size"),
					  "'size' must be a positive number");
			} else {
				layer.size = size_it->get<double>();
				layer.min_size = layer.size;
			}
		}
		auto min_size_it = obj.find("minSize");
		if (min_size_it != obj.end()) {
			if (!min_size_it->is_number() || min_size_it->get<double>() <= 0.0) {
				add_error(diags, "NXG-E005", join_path(layer_path, "minSize"),
					  "'minSize' must be a positive number");
			} else {
				layer.min_size = min_size_it->get<double>();
			}
		}
		auto col_it = obj.find("color");
		if (col_it != obj.end()) {
			if (!col_it->is_string()) {
				add_error(diags, "NXG-E005", join_path(layer_path, "color"),
					  "'color' must be a hex color string");
			} else {
				auto parsed = parse_color(col_it->get<std::string>());
				if (parsed.is_err()) {
					add_error(diags, "NXG-E005", join_path(layer_path, "color"),
						  parsed.error().message);
				} else {
					layer.color = parsed.value();
				}
			}
		}
		auto align_it = obj.find("align");
		if (align_it != obj.end()) {
			if (!align_it->is_string()) {
				add_error(diags, "NXG-E005", join_path(layer_path, "align"),
					  "'align' must be a string");
			} else {
				const std::string a = align_it->get<std::string>();
				if (a == "left") {
					layer.align = TextAlign::Left;
				} else if (a == "center") {
					layer.align = TextAlign::Center;
				} else if (a == "right") {
					layer.align = TextAlign::Right;
				} else {
					add_error(diags, "NXG-E005", join_path(layer_path, "align"),
						  "'align' must be 'left', 'center', or 'right'");
				}
			}
		}
		auto valign_it = obj.find("valign");
		if (valign_it != obj.end()) {
			if (!valign_it->is_string()) {
				add_error(diags, "NXG-E005", join_path(layer_path, "valign"),
					  "'valign' must be a string");
			} else {
				const std::string va = valign_it->get<std::string>();
				if (va == "top") {
					layer.valign = TextVAlign::Top;
				} else if (va == "middle") {
					layer.valign = TextVAlign::Middle;
				} else if (va == "bottom") {
					layer.valign = TextVAlign::Bottom;
				} else {
					add_error(diags, "NXG-E005", join_path(layer_path, "valign"),
						  "'valign' must be 'top', 'middle', or 'bottom'");
				}
			}
		}
		auto ov_it = obj.find("overflow");
		if (ov_it != obj.end()) {
			if (!ov_it->is_string()) {
				add_error(diags, "NXG-E005", join_path(layer_path, "overflow"),
					  "'overflow' must be a string");
			} else {
				const std::string ov = ov_it->get<std::string>();
				if (ov == "shrink") {
					layer.overflow = TextOverflow::Shrink;
				} else if (ov == "clip") {
					layer.overflow = TextOverflow::Clip;
				} else if (ov == "ellipsis") {
					layer.overflow = TextOverflow::Ellipsis;
				} else if (ov == "wrap") {
					layer.overflow = TextOverflow::Wrap;
				} else {
					add_error(diags, "NXG-E005", join_path(layer_path, "overflow"),
						  "'overflow' must be 'shrink', 'clip', 'ellipsis', or 'wrap'");
				}
			}
		}
		auto ml_it = obj.find("maxLines");
		if (ml_it != obj.end()) {
			if (!ml_it->is_number_integer() || ml_it->get<int>() < 1) {
				add_error(diags, "NXG-E005", join_path(layer_path, "maxLines"),
					  "'maxLines' must be an integer >= 1");
			} else {
				layer.max_lines = ml_it->get<int>();
			}
		}
		auto lh_it = obj.find("lineHeight");
		if (lh_it != obj.end()) {
			if (!lh_it->is_number() || lh_it->get<double>() <= 0.0) {
				add_error(diags, "NXG-E005", join_path(layer_path, "lineHeight"),
					  "'lineHeight' must be a positive number");
			} else {
				layer.line_height = lh_it->get<double>();
			}
		}
		auto ls_it = obj.find("letterSpacing");
		if (ls_it != obj.end()) {
			if (!ls_it->is_number()) {
				add_error(diags, "NXG-E005", join_path(layer_path, "letterSpacing"),
					  "'letterSpacing' must be a number");
			} else {
				layer.letter_spacing = ls_it->get<double>();
			}
		}
		auto up_it = obj.find("uppercase");
		if (up_it != obj.end()) {
			if (!up_it->is_boolean()) {
				add_error(diags, "NXG-E005", join_path(layer_path, "uppercase"),
					  "'uppercase' must be a boolean");
			} else {
				layer.uppercase = up_it->get<bool>();
			}
		}
	} else if (layer.type == LayerType::Image) {
		auto src_it = obj.find("src");
		if (src_it == obj.end() || !src_it->is_string() || src_it->get<std::string>().empty()) {
			add_error(diags, "NXG-E005", join_path(layer_path, "src"),
				  "image layer requires non-empty string 'src'");
		} else {
			layer.src = src_it->get<std::string>();
		}
		auto fit_it = obj.find("fit");
		if (fit_it != obj.end()) {
			if (!fit_it->is_string()) {
				add_error(diags, "NXG-E005", join_path(layer_path, "fit"), "'fit' must be a string");
			} else {
				const std::string fit = fit_it->get<std::string>();
				if (fit == "contain") {
					layer.fit = ImageFit::Contain;
				} else if (fit == "cover") {
					layer.fit = ImageFit::Cover;
				} else if (fit == "stretch") {
					layer.fit = ImageFit::Stretch;
				} else {
					add_error(diags, "NXG-E005", join_path(layer_path, "fit"),
						  "'fit' must be 'contain', 'cover', or 'stretch'");
				}
			}
		}
	}

	manifest.layer_declaration_order.push_back(name);
	manifest.layers.emplace(name, std::move(layer));
}

void parse_layers(const json &root, Manifest &manifest, std::vector<Diagnostic> &diags)
{
	auto it = root.find("layers");
	if (it == root.end() || !it->is_object() || it->empty()) {
		add_error(diags, "NXG-E005", "layers", "missing or empty required object 'layers'");
		return;
	}

	for (auto l_it = it->begin(); l_it != it->end(); ++l_it) {
		parse_single_layer(l_it.key(), l_it.value(), manifest, diags);
	}
}

void parse_states(const json &root, Manifest &manifest, std::vector<Diagnostic> &diags)
{
	auto it = root.find("states");
	if (it == root.end()) {
		return;
	}
	if (!it->is_object()) {
		add_error(diags, "NXG-E005", "states", "'states' must be an object");
		return;
	}

	for (auto s_it = it->begin(); s_it != it->end(); ++s_it) {
		const std::string &state_name = s_it.key();
		const std::string state_path = join_path("states", state_name);
		if (!is_valid_identifier(state_name)) {
			add_error(diags, "NXG-E005", state_path, "invalid state name '" + state_name + "'");
		}
		if (!s_it.value().is_object()) {
			add_error(diags, "NXG-E005", state_path, "state entry must be an object");
			continue;
		}

		StateDef state;
		state.name = state_name;
		state.json_path = state_path;

		for (auto p_it = s_it.value().begin(); p_it != s_it.value().end(); ++p_it) {
			const std::string entry_path = join_path(state_path, p_it.key());
			auto val = parse_raw_json_value(p_it.value());
			if (!val.has_value()) {
				add_error(diags, "NXG-E033", entry_path,
					  "state value must be a boolean, number, or string");
				continue;
			}
			state.entries.push_back(StateEntryDef{entry_path, p_it.key(), std::move(*val)});
		}

		manifest.states.emplace(state_name, std::move(state));
	}
}

void parse_control(const json &ctrl_obj, std::string_view ctrl_path, PanelSectionDef &section,
		   std::vector<Diagnostic> &diags)
{
	if (!ctrl_obj.is_object()) {
		add_error(diags, "NXG-E005", std::string(ctrl_path), "control entry must be an object");
		return;
	}

	ControlDef ctrl;
	ctrl.json_path = std::string(ctrl_path);

	auto type_it = ctrl_obj.find("type");
	if (type_it == ctrl_obj.end() || !type_it->is_string()) {
		add_error(diags, "NXG-E005", join_path(ctrl_path, "type"), "missing or non-string control 'type'");
		return;
	}

	const std::string type_str = type_it->get<std::string>();
	if (type_str == "text") {
		ctrl.type = ControlType::Text;
	} else if (type_str == "number") {
		ctrl.type = ControlType::Number;
	} else if (type_str == "toggle") {
		ctrl.type = ControlType::Toggle;
	} else if (type_str == "color") {
		ctrl.type = ControlType::Color;
	} else if (type_str == "dropdown") {
		ctrl.type = ControlType::Dropdown;
	} else if (type_str == "button") {
		ctrl.type = ControlType::Button;
	} else if (type_str == "label") {
		ctrl.type = ControlType::Label;
	} else if (type_str == "separator") {
		ctrl.type = ControlType::Separator;
	} else {
		add_error(diags, "NXG-E005", join_path(ctrl_path, "type"), "unknown control type '" + type_str + "'");
		return;
	}

	check_unknown_fields(ctrl_obj, ctrl_path,
			     {"type", "key", "label", "bind", "default", "tooltip", "placeholder", "maxLength",
			      "multiline", "submitAction", "min", "max", "step", "slider", "unit", "options", "action",
			      "style", "hotkey"},
			     diags);

	const bool requires_key = (ctrl.type != ControlType::Button && ctrl.type != ControlType::Label &&
				   ctrl.type != ControlType::Separator);
	auto key_it = ctrl_obj.find("key");
	if (key_it != ctrl_obj.end()) {
		if (!key_it->is_string() || key_it->get<std::string>().empty()) {
			add_error(diags, "NXG-E005", join_path(ctrl_path, "key"),
				  "control 'key' must be a non-empty string");
		} else {
			ctrl.key = key_it->get<std::string>();
			if (!is_valid_identifier(ctrl.key)) {
				add_error(diags, "NXG-E005", join_path(ctrl_path, "key"),
					  "invalid control key '" + ctrl.key + "'");
			}
		}
	} else if (requires_key) {
		add_error(diags, "NXG-E005", join_path(ctrl_path, "key"),
			  "control of type '" + type_str + "' requires 'key'");
	}

	auto label_it = ctrl_obj.find("label");
	if (label_it != ctrl_obj.end()) {
		if (!label_it->is_string()) {
			add_error(diags, "NXG-E005", join_path(ctrl_path, "label"), "'label' must be a string");
		} else {
			ctrl.label = label_it->get<std::string>();
		}
	}

	auto tooltip_it = ctrl_obj.find("tooltip");
	if (tooltip_it != ctrl_obj.end()) {
		if (!tooltip_it->is_string()) {
			add_error(diags, "NXG-E005", join_path(ctrl_path, "tooltip"), "'tooltip' must be a string");
		} else {
			ctrl.tooltip = tooltip_it->get<std::string>();
		}
	}

	auto bind_it = ctrl_obj.find("bind");
	if (bind_it != ctrl_obj.end()) {
		if (bind_it->is_string()) {
			ctrl.binds.push_back(bind_it->get<std::string>());
		} else if (bind_it->is_array() && !bind_it->empty()) {
			for (std::size_t i = 0; i < bind_it->size(); ++i) {
				if (!(*bind_it)[i].is_string()) {
					add_error(diags, "NXG-E030", index_path(join_path(ctrl_path, "bind"), i),
						  "bind element must be a string");
				} else {
					ctrl.binds.push_back((*bind_it)[i].get<std::string>());
				}
			}
		} else {
			add_error(diags, "NXG-E030", join_path(ctrl_path, "bind"),
				  "'bind' must be a string or non-empty array of strings");
		}
	}

	auto def_it = ctrl_obj.find("default");
	if (def_it != ctrl_obj.end()) {
		auto val = parse_raw_json_value(*def_it);
		if (!val.has_value()) {
			add_error(diags, "NXG-E033", join_path(ctrl_path, "default"),
				  "invalid control 'default' value");
		} else {
			ctrl.default_value = std::move(*val);
		}
	}

	if (ctrl.type == ControlType::Text) {
		auto ph_it = ctrl_obj.find("placeholder");
		if (ph_it != ctrl_obj.end()) {
			if (!ph_it->is_string()) {
				add_error(diags, "NXG-E005", join_path(ctrl_path, "placeholder"),
					  "'placeholder' must be a string");
			} else {
				ctrl.placeholder = ph_it->get<std::string>();
			}
		}
		auto ml_it = ctrl_obj.find("maxLength");
		if (ml_it != ctrl_obj.end()) {
			if (!ml_it->is_number_integer() || ml_it->get<int>() <= 0) {
				add_error(diags, "NXG-E005", join_path(ctrl_path, "maxLength"),
					  "'maxLength' must be a positive integer");
			} else {
				ctrl.max_length = ml_it->get<int>();
			}
		}
		auto multi_it = ctrl_obj.find("multiline");
		if (multi_it != ctrl_obj.end()) {
			if (!multi_it->is_boolean()) {
				add_error(diags, "NXG-E005", join_path(ctrl_path, "multiline"),
					  "'multiline' must be a boolean");
			} else {
				ctrl.multiline = multi_it->get<bool>();
			}
		}
		auto sub_it = ctrl_obj.find("submitAction");
		if (sub_it != ctrl_obj.end()) {
			if (!sub_it->is_string() || sub_it->get<std::string>().empty()) {
				add_error(diags, "NXG-E005", join_path(ctrl_path, "submitAction"),
					  "'submitAction' must be a non-empty string");
			} else {
				ctrl.submit_action = sub_it->get<std::string>();
			}
		}
	} else if (ctrl.type == ControlType::Number) {
		for (const char *num_field : {"min", "max", "step"}) {
			auto n_it = ctrl_obj.find(num_field);
			if (n_it != ctrl_obj.end()) {
				if (!n_it->is_number()) {
					add_error(diags, "NXG-E005", join_path(ctrl_path, num_field),
						  std::string("'") + num_field + "' must be a number");
				} else if (std::string_view(num_field) == "min") {
					ctrl.min = n_it->get<double>();
				} else if (std::string_view(num_field) == "max") {
					ctrl.max = n_it->get<double>();
				} else {
					ctrl.step = n_it->get<double>();
				}
			}
		}
		auto sl_it = ctrl_obj.find("slider");
		if (sl_it != ctrl_obj.end()) {
			if (!sl_it->is_boolean()) {
				add_error(diags, "NXG-E005", join_path(ctrl_path, "slider"),
					  "'slider' must be a boolean");
			} else {
				ctrl.slider = sl_it->get<bool>();
			}
		}
		auto unit_it = ctrl_obj.find("unit");
		if (unit_it != ctrl_obj.end()) {
			if (!unit_it->is_string()) {
				add_error(diags, "NXG-E005", join_path(ctrl_path, "unit"), "'unit' must be a string");
			} else {
				ctrl.unit = unit_it->get<std::string>();
			}
		}
	} else if (ctrl.type == ControlType::Dropdown) {
		auto opt_it = ctrl_obj.find("options");
		if (opt_it == ctrl_obj.end() || !opt_it->is_array() || opt_it->empty()) {
			add_error(diags, "NXG-E005", join_path(ctrl_path, "options"),
				  "dropdown requires a non-empty 'options' array");
		} else {
			const std::string opts_base = join_path(ctrl_path, "options");
			for (std::size_t i = 0; i < opt_it->size(); ++i) {
				const std::string opt_path = index_path(opts_base, i);
				const json &o = (*opt_it)[i];
				if (!o.is_object()) {
					add_error(diags, "NXG-E005", opt_path, "dropdown option must be an object");
					continue;
				}
				check_unknown_fields(o, opt_path, {"label", "value", "apply"}, diags);

				DropdownOptionDef opt;
				opt.json_path = opt_path;

				auto l_it = o.find("label");
				if (l_it == o.end() || !l_it->is_string() || l_it->get<std::string>().empty()) {
					add_error(diags, "NXG-E005", join_path(opt_path, "label"),
						  "dropdown option requires non-empty 'label'");
				} else {
					opt.label = l_it->get<std::string>();
				}

				const bool has_val = o.contains("value");
				const bool has_apply = o.contains("apply");
				if (has_val == has_apply) {
					add_error(diags, "NXG-E005", opt_path,
						  "dropdown option must have exactly one of 'value' or 'apply'");
				} else if (has_val) {
					auto v = parse_raw_json_value(o["value"]);
					if (!v.has_value()) {
						add_error(diags, "NXG-E033", join_path(opt_path, "value"),
							  "invalid dropdown option 'value'");
					} else {
						opt.value = std::move(*v);
					}
				} else {
					if (!o["apply"].is_string() || o["apply"].get<std::string>().empty()) {
						add_error(diags, "NXG-E005", join_path(opt_path, "apply"),
							  "dropdown option 'apply' must be a non-empty string");
					} else {
						opt.apply = o["apply"].get<std::string>();
					}
				}

				ctrl.options.push_back(std::move(opt));
			}
		}
	} else if (ctrl.type == ControlType::Button) {
		auto act_it = ctrl_obj.find("action");
		if (act_it == ctrl_obj.end() || !act_it->is_string() || act_it->get<std::string>().empty()) {
			add_error(diags, "NXG-E005", join_path(ctrl_path, "action"),
				  "button control requires non-empty 'action'");
		} else {
			ctrl.action = act_it->get<std::string>();
		}
		auto st_it = ctrl_obj.find("style");
		if (st_it != ctrl_obj.end()) {
			if (!st_it->is_string()) {
				add_error(diags, "NXG-E005", join_path(ctrl_path, "style"), "'style' must be a string");
			} else {
				const std::string st = st_it->get<std::string>();
				if (st == "default") {
					ctrl.style = ButtonStyle::Default;
				} else if (st == "primary") {
					ctrl.style = ButtonStyle::Primary;
				} else if (st == "danger") {
					ctrl.style = ButtonStyle::Danger;
				} else {
					add_error(diags, "NXG-E005", join_path(ctrl_path, "style"),
						  "'style' must be 'default', 'primary', or 'danger'");
				}
			}
		}
		auto hk_it = ctrl_obj.find("hotkey");
		if (hk_it != ctrl_obj.end()) {
			if (!hk_it->is_string()) {
				add_error(diags, "NXG-E005", join_path(ctrl_path, "hotkey"),
					  "'hotkey' must be a string");
			} else {
				ctrl.hotkey = hk_it->get<std::string>();
			}
		}
	}

	section.controls.push_back(std::move(ctrl));
}

void parse_panel(const json &root, Manifest &manifest, std::vector<Diagnostic> &diags)
{
	auto it = root.find("panel");
	if (it == root.end()) {
		return;
	}
	if (!it->is_object()) {
		add_error(diags, "NXG-E005", "panel", "'panel' must be an object");
		return;
	}

	const json &panel_obj = *it;
	check_unknown_fields(panel_obj, "panel", {"title", "sections"}, diags);

	PanelDef panel;
	auto title_it = panel_obj.find("title");
	if (title_it != panel_obj.end()) {
		if (!title_it->is_string()) {
			add_error(diags, "NXG-E005", "panel.title", "'panel.title' must be a string");
		} else {
			panel.title = title_it->get<std::string>();
		}
	}

	auto sec_it = panel_obj.find("sections");
	if (sec_it != panel_obj.end()) {
		if (!sec_it->is_array()) {
			add_error(diags, "NXG-E005", "panel.sections", "'panel.sections' must be an array");
		} else {
			for (std::size_t i = 0; i < sec_it->size(); ++i) {
				const std::string sec_path = index_path("panel.sections", i);
				const json &s_obj = (*sec_it)[i];
				if (!s_obj.is_object()) {
					add_error(diags, "NXG-E005", sec_path, "panel section must be an object");
					continue;
				}
				check_unknown_fields(s_obj, sec_path, {"title", "collapsed", "columns", "controls"},
						     diags);

				PanelSectionDef sec;
				sec.json_path = sec_path;

				auto st_it = s_obj.find("title");
				if (st_it == s_obj.end() || !st_it->is_string()) {
					add_error(diags, "NXG-E005", join_path(sec_path, "title"),
						  "section requires string 'title'");
				} else {
					sec.title = st_it->get<std::string>();
				}

				auto col_it = s_obj.find("collapsed");
				if (col_it != s_obj.end()) {
					if (!col_it->is_boolean()) {
						add_error(diags, "NXG-E005", join_path(sec_path, "collapsed"),
							  "'collapsed' must be a boolean");
					} else {
						sec.collapsed = col_it->get<bool>();
					}
				}

				auto cols_it = s_obj.find("columns");
				if (cols_it != s_obj.end()) {
					if (!cols_it->is_number_integer() || cols_it->get<int>() < 1 ||
					    cols_it->get<int>() > 3) {
						add_error(diags, "NXG-E005", join_path(sec_path, "columns"),
							  "'columns' must be an integer between 1 and 3");
					} else {
						sec.columns = cols_it->get<int>();
					}
				}

				auto ctrls_it = s_obj.find("controls");
				if (ctrls_it == s_obj.end() || !ctrls_it->is_array()) {
					add_error(diags, "NXG-E005", join_path(sec_path, "controls"),
						  "section requires 'controls' array");
				} else {
					const std::string ctrls_base = join_path(sec_path, "controls");
					for (std::size_t j = 0; j < ctrls_it->size(); ++j) {
						parse_control((*ctrls_it)[j], index_path(ctrls_base, j), sec, diags);
					}
				}

				panel.sections.push_back(std::move(sec));
			}
		}
	}

	manifest.panel = std::move(panel);
}

void parse_track(const json &t_obj, std::string_view track_path, ClipDef &clip, std::vector<Diagnostic> &diags)
{
	if (!t_obj.is_object()) {
		add_error(diags, "NXG-E005", std::string(track_path), "track entry must be an object");
		return;
	}

	check_unknown_fields(t_obj, track_path,
			     {"layer", "prop", "from", "to", "keys", "start", "dur", "ease", "blend"}, diags);

	TrackDef track;
	track.json_path = std::string(track_path);

	auto layer_it = t_obj.find("layer");
	if (layer_it == t_obj.end() || !layer_it->is_string() || layer_it->get<std::string>().empty()) {
		add_error(diags, "NXG-E005", join_path(track_path, "layer"), "track requires non-empty string 'layer'");
	} else {
		track.layer = layer_it->get<std::string>();
	}

	auto prop_it = t_obj.find("prop");
	if (prop_it == t_obj.end() || !prop_it->is_string() || prop_it->get<std::string>().empty()) {
		add_error(diags, "NXG-E005", join_path(track_path, "prop"), "track requires non-empty string 'prop'");
	} else {
		track.prop = prop_it->get<std::string>();
	}

	auto from_it = t_obj.find("from");
	if (from_it != t_obj.end()) {
		auto val = parse_raw_json_value(*from_it);
		if (!val.has_value()) {
			add_error(diags, "NXG-E024", join_path(track_path, "from"), "invalid track 'from' value");
		} else {
			track.from = std::move(*val);
		}
	}

	auto to_it = t_obj.find("to");
	if (to_it != t_obj.end()) {
		auto val = parse_raw_json_value(*to_it);
		if (!val.has_value()) {
			add_error(diags, "NXG-E024", join_path(track_path, "to"), "invalid track 'to' value");
		} else {
			track.to = std::move(*val);
		}
	}

	auto keys_it = t_obj.find("keys");
	if (keys_it != t_obj.end()) {
		track.has_keys = true;
		if (!keys_it->is_array() || keys_it->size() < 2) {
			add_error(diags, "NXG-E024", join_path(track_path, "keys"),
				  "'keys' must be an array with at least 2 keyframes");
		} else {
			const std::string keys_base = join_path(track_path, "keys");
			for (std::size_t k = 0; k < keys_it->size(); ++k) {
				const std::string kf_path = index_path(keys_base, k);
				const json &kf = (*keys_it)[k];
				if (!kf.is_array() || kf.size() != 2 || !kf[0].is_number()) {
					add_error(diags, "NXG-E024", kf_path,
						  "keyframe must be a [time, value] pair with numeric time");
					continue;
				}
				auto kv = parse_raw_json_value(kf[1]);
				if (!kv.has_value()) {
					add_error(diags, "NXG-E024", kf_path, "invalid keyframe value");
					continue;
				}
				track.keys.push_back(Keyframe{kf[0].get<double>(), std::move(*kv)});
			}
		}
	}

	auto start_it = t_obj.find("start");
	if (start_it != t_obj.end()) {
		if (!start_it->is_number() || start_it->get<double>() < 0.0) {
			add_error(diags, "NXG-E005", join_path(track_path, "start"),
				  "track 'start' must be a non-negative number");
		} else {
			track.start = start_it->get<double>();
		}
	}

	auto dur_it = t_obj.find("dur");
	if (dur_it != t_obj.end()) {
		if (!dur_it->is_number() || dur_it->get<double>() <= 0.0) {
			add_error(diags, "NXG-E024", join_path(track_path, "dur"),
				  "track 'dur' must be a positive number");
		} else {
			track.dur = dur_it->get<double>();
		}
	}

	auto ease_it = t_obj.find("ease");
	if (ease_it != t_obj.end()) {
		auto parsed_ease = parse_easing(*ease_it, join_path(track_path, "ease"), diags);
		if (parsed_ease.has_value()) {
			track.ease = *parsed_ease;
		}
	}

	auto blend_it = t_obj.find("blend");
	if (blend_it != t_obj.end()) {
		if (!blend_it->is_string()) {
			add_error(diags, "NXG-E005", join_path(track_path, "blend"), "track 'blend' must be a string");
		} else {
			const std::string b = blend_it->get<std::string>();
			if (b == "override") {
				track.blend = BlendMode::Override;
			} else if (b == "add") {
				track.blend = BlendMode::Add;
			} else {
				add_error(diags, "NXG-E005", join_path(track_path, "blend"),
					  "track 'blend' must be 'override' or 'add'");
			}
		}
	}

	clip.tracks.push_back(std::move(track));
}

void parse_trigger(const json &tr_obj, std::string_view trig_path, ClipDef &clip, std::vector<Diagnostic> &diags)
{
	if (!tr_obj.is_object()) {
		add_error(diags, "NXG-E005", std::string(trig_path), "trigger entry must be an object");
		return;
	}

	check_unknown_fields(tr_obj, trig_path, {"play", "at", "on", "delay", "mode", "speed"}, diags);

	TriggerDef trig;
	trig.json_path = std::string(trig_path);

	auto play_it = tr_obj.find("play");
	if (play_it == tr_obj.end() || !play_it->is_string() || play_it->get<std::string>().empty()) {
		add_error(diags, "NXG-E005", join_path(trig_path, "play"), "trigger requires non-empty string 'play'");
	} else {
		trig.play = play_it->get<std::string>();
	}

	const bool has_at = tr_obj.contains("at");
	const bool has_on = tr_obj.contains("on");
	if (has_at && has_on) {
		add_error(diags, "NXG-E005", std::string(trig_path), "trigger cannot specify both 'at' and 'on'");
	}

	if (has_at) {
		const json &at_val = tr_obj["at"];
		if (!at_val.is_number() || at_val.get<double>() < 0.0) {
			add_error(diags, "NXG-E005", join_path(trig_path, "at"),
				  "trigger 'at' must be a non-negative number");
		} else {
			trig.event_kind = TriggerEventKind::AtTime;
			trig.at = at_val.get<double>();
		}
	} else if (has_on) {
		const json &on_val = tr_obj["on"];
		if (!on_val.is_string()) {
			add_error(diags, "NXG-E005", join_path(trig_path, "on"), "trigger 'on' must be a string");
		} else {
			const std::string on_str = on_val.get<std::string>();
			if (on_str == "end") {
				trig.event_kind = TriggerEventKind::OnEnd;
			} else if (on_str.rfind("marker:", 0) == 0 && on_str.size() > 7) {
				trig.event_kind = TriggerEventKind::OnMarker;
				trig.marker_name = on_str.substr(7);
			} else {
				add_error(diags, "NXG-E005", join_path(trig_path, "on"),
					  "trigger 'on' must be 'end' or 'marker:<name>'");
			}
		}
	}

	auto delay_it = tr_obj.find("delay");
	if (delay_it != tr_obj.end()) {
		if (!delay_it->is_number() || delay_it->get<double>() < 0.0) {
			add_error(diags, "NXG-E005", join_path(trig_path, "delay"),
				  "trigger 'delay' must be a non-negative number");
		} else {
			trig.delay = delay_it->get<double>();
		}
	}

	auto mode_it = tr_obj.find("mode");
	if (mode_it != tr_obj.end()) {
		if (!mode_it->is_string()) {
			add_error(diags, "NXG-E005", join_path(trig_path, "mode"), "trigger 'mode' must be a string");
		} else {
			const std::string m = mode_it->get<std::string>();
			if (m == "linked") {
				trig.mode = TriggerMode::Linked;
			} else if (m == "detached") {
				trig.mode = TriggerMode::Detached;
			} else {
				add_error(diags, "NXG-E005", join_path(trig_path, "mode"),
					  "trigger 'mode' must be 'linked' or 'detached'");
			}
		}
	}

	auto speed_it = tr_obj.find("speed");
	if (speed_it != tr_obj.end()) {
		if (speed_it->is_string() && speed_it->get<std::string>() == "inherit") {
			trig.inherit_speed = true;
		} else if (speed_it->is_number() && speed_it->get<double>() > 0.0) {
			trig.inherit_speed = false;
			trig.speed = speed_it->get<double>();
		} else {
			add_error(diags, "NXG-E005", join_path(trig_path, "speed"),
				  "trigger 'speed' must be a positive number or \"inherit\"");
		}
	}

	clip.triggers.push_back(std::move(trig));
}

void parse_animations(const json &root, Manifest &manifest, std::vector<Diagnostic> &diags)
{
	auto it = root.find("animations");
	if (it == root.end()) {
		return;
	}
	if (!it->is_object()) {
		add_error(diags, "NXG-E005", "animations", "'animations' must be an object");
		return;
	}

	for (auto c_it = it->begin(); c_it != it->end(); ++c_it) {
		const std::string &clip_name = c_it.key();
		const std::string clip_path = join_path("animations", clip_name);
		if (!is_valid_identifier(clip_name)) {
			add_error(diags, "NXG-E005", clip_path, "invalid clip name '" + clip_name + "'");
		}
		if (!c_it.value().is_object()) {
			add_error(diags, "NXG-E005", clip_path, "clip definition must be an object");
			continue;
		}

		const json &c_obj = c_it.value();
		check_unknown_fields(c_obj, clip_path,
				     {"tracks", "triggers", "markers", "duration", "loop", "onEnd", "retrigger"},
				     diags);

		ClipDef clip;
		clip.name = clip_name;
		clip.json_path = clip_path;

		auto dur_it = c_obj.find("duration");
		if (dur_it != c_obj.end()) {
			if (!dur_it->is_number() || dur_it->get<double>() <= 0.0) {
				add_error(diags, "NXG-E024", join_path(clip_path, "duration"),
					  "clip 'duration' must be a positive number");
			} else {
				clip.duration = dur_it->get<double>();
			}
		}

		auto loop_it = c_obj.find("loop");
		if (loop_it != c_obj.end()) {
			if (!loop_it->is_boolean()) {
				add_error(diags, "NXG-E005", join_path(clip_path, "loop"),
					  "clip 'loop' must be a boolean");
			} else {
				clip.loop = loop_it->get<bool>();
			}
		}

		auto oe_it = c_obj.find("onEnd");
		if (oe_it != c_obj.end()) {
			if (!oe_it->is_string()) {
				add_error(diags, "NXG-E005", join_path(clip_path, "onEnd"),
					  "clip 'onEnd' must be a string");
			} else {
				const std::string oe = oe_it->get<std::string>();
				if (oe == "hold") {
					clip.on_end = OnEndPolicy::Hold;
				} else if (oe == "revert") {
					clip.on_end = OnEndPolicy::Revert;
				} else {
					add_error(diags, "NXG-E005", join_path(clip_path, "onEnd"),
						  "clip 'onEnd' must be 'hold' or 'revert'");
				}
			}
		}

		auto rt_it = c_obj.find("retrigger");
		if (rt_it != c_obj.end()) {
			if (!rt_it->is_string()) {
				add_error(diags, "NXG-E005", join_path(clip_path, "retrigger"),
					  "clip 'retrigger' must be a string");
			} else {
				const std::string rt = rt_it->get<std::string>();
				if (rt == "restart") {
					clip.retrigger = RetriggerPolicy::Restart;
				} else if (rt == "ignore") {
					clip.retrigger = RetriggerPolicy::Ignore;
				} else if (rt == "stack") {
					clip.retrigger = RetriggerPolicy::Stack;
				} else {
					add_error(diags, "NXG-E005", join_path(clip_path, "retrigger"),
						  "clip 'retrigger' must be 'restart', 'ignore', or 'stack'");
				}
			}
		}

		auto mk_it = c_obj.find("markers");
		if (mk_it != c_obj.end()) {
			const std::string mk_base = join_path(clip_path, "markers");
			if (!mk_it->is_object()) {
				add_error(diags, "NXG-E005", mk_base, "clip 'markers' must be an object");
			} else {
				for (auto m_it = mk_it->begin(); m_it != mk_it->end(); ++m_it) {
					if (!m_it.value().is_number() || m_it.value().get<double>() < 0.0) {
						add_error(diags, "NXG-E005", join_path(mk_base, m_it.key()),
							  "marker time must be a non-negative number");
					} else {
						clip.markers.emplace(m_it.key(), m_it.value().get<double>());
					}
				}
			}
		}

		auto trk_it = c_obj.find("tracks");
		if (trk_it != c_obj.end()) {
			const std::string trk_base = join_path(clip_path, "tracks");
			if (!trk_it->is_array()) {
				add_error(diags, "NXG-E005", trk_base, "clip 'tracks' must be an array");
			} else {
				for (std::size_t i = 0; i < trk_it->size(); ++i) {
					parse_track((*trk_it)[i], index_path(trk_base, i), clip, diags);
				}
			}
		}

		auto trig_it = c_obj.find("triggers");
		if (trig_it != c_obj.end()) {
			const std::string trig_base = join_path(clip_path, "triggers");
			if (!trig_it->is_array()) {
				add_error(diags, "NXG-E005", trig_base, "clip 'triggers' must be an array");
			} else {
				for (std::size_t i = 0; i < trig_it->size(); ++i) {
					parse_trigger((*trig_it)[i], index_path(trig_base, i), clip, diags);
				}
			}
		}

		manifest.animations.emplace(clip_name, std::move(clip));
	}
}

void parse_actions(const json &root, Manifest &manifest, std::vector<Diagnostic> &diags)
{
	auto it = root.find("actions");
	if (it == root.end()) {
		return;
	}
	if (!it->is_object()) {
		add_error(diags, "NXG-E005", "actions", "'actions' must be an object");
		return;
	}

	for (auto a_it = it->begin(); a_it != it->end(); ++a_it) {
		const std::string &act_name = a_it.key();
		const std::string act_path = join_path("actions", act_name);
		if (!is_valid_identifier(act_name)) {
			add_error(diags, "NXG-E005", act_path, "invalid action name '" + act_name + "'");
		}
		if (!a_it.value().is_array()) {
			add_error(diags, "NXG-E005", act_path, "action definition must be an array of steps");
			continue;
		}

		std::vector<ActionStepDef> steps;
		const json &arr = a_it.value();
		for (std::size_t i = 0; i < arr.size(); ++i) {
			const std::string step_path = index_path(act_path, i);
			const json &s_obj = arr[i];
			if (!s_obj.is_object()) {
				add_error(diags, "NXG-E005", step_path, "action step must be an object");
				continue;
			}
			check_unknown_fields(s_obj, step_path, {"play", "stop", "set", "call", "delay"}, diags);

			int verb_count = 0;
			ActionStepDef step;
			step.json_path = step_path;

			for (const auto &[verb, kind] : std::initializer_list<std::pair<const char *, ActionStepType>>{
				     {"play", ActionStepType::Play},
				     {"stop", ActionStepType::Stop},
				     {"set", ActionStepType::Set},
				     {"call", ActionStepType::Call},
			     }) {
				auto v_it = s_obj.find(verb);
				if (v_it != s_obj.end()) {
					++verb_count;
					step.type = kind;
					if (!v_it->is_string() || v_it->get<std::string>().empty()) {
						add_error(diags, "NXG-E005", join_path(step_path, verb),
							  std::string("action step '") + verb +
								  "' must be a non-empty string");
					} else {
						step.target = v_it->get<std::string>();
					}
				}
			}

			if (verb_count != 1) {
				add_error(diags, "NXG-E005", step_path,
					  "action step must contain exactly one of 'play', 'stop', 'set', or 'call'");
				continue;
			}

			auto delay_it = s_obj.find("delay");
			if (delay_it != s_obj.end()) {
				if (!delay_it->is_number() || delay_it->get<double>() < 0.0) {
					add_error(diags, "NXG-E005", join_path(step_path, "delay"),
						  "action step 'delay' must be a non-negative number");
				} else {
					step.delay = delay_it->get<double>();
				}
			}

			steps.push_back(std::move(step));
		}

		manifest.actions.emplace(act_name, std::move(steps));
	}
}

} // namespace

bool is_valid_identifier(std::string_view name) noexcept
{
	if (name.empty()) {
		return false;
	}
	for (char c : name) {
		const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
				c == '_' || c == '.' || c == '-';
		if (!ok) {
			return false;
		}
	}
	return true;
}

bool is_valid_meta_id(std::string_view id) noexcept
{
	if (id.empty()) {
		return false;
	}
	for (char c : id) {
		const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-';
		if (!ok) {
			return false;
		}
	}
	return true;
}

std::optional<Manifest> parse_manifest_json(std::string_view json_text, std::vector<Diagnostic> &diagnostics)
{
	if (json_text.size() > kMaxManifestBytes) {
		add_error(diagnostics, "NXG-E001", "", "manifest.json exceeds maximum allowed size (1 MB)");
		return std::nullopt;
	}

	if (!is_valid_utf8(json_text)) {
		add_error(diagnostics, "NXG-E001", "", "manifest.json is not valid UTF-8");
		return std::nullopt;
	}

	// Track duplicate layer keys during SAX/DOM parsing
	bool inside_layers_object = false;
	int layers_depth = -1;
	std::unordered_set<std::string> seen_layer_keys;
	std::vector<std::string> duplicate_layer_keys;

	json::parser_callback_t cb = [&](int depth, json::parse_event_t event, json &parsed) -> bool {
		if (depth == 1 && event == json::parse_event_t::key && parsed.is_string() &&
		    parsed.get<std::string>() == "layers") {
			inside_layers_object = true;
			layers_depth = 2;
		} else if (inside_layers_object) {
			if (depth == layers_depth && event == json::parse_event_t::key && parsed.is_string()) {
				const std::string key = parsed.get<std::string>();
				if (!seen_layer_keys.insert(key).second) {
					duplicate_layer_keys.push_back(key);
				}
			} else if (depth < layers_depth && event == json::parse_event_t::object_end) {
				inside_layers_object = false;
			}
		}
		return true;
	};

	const json root = json::parse(json_text.begin(), json_text.end(), cb, /*allow_exceptions=*/false,
				      /*ignore_comments=*/false);
	if (root.is_discarded()) {
		add_error(diagnostics, "NXG-E001", "", "invalid JSON syntax in manifest.json");
		return std::nullopt;
	}

	if (!root.is_object()) {
		add_error(diagnostics, "NXG-E005", "", "manifest root must be a JSON object");
		return std::nullopt;
	}

	for (const std::string &dup_key : duplicate_layer_keys) {
		add_error(diagnostics, "NXG-E011", join_path("layers", dup_key),
			  "duplicate layer name '" + dup_key + "'");
	}

	check_unknown_fields(root, "",
			     {"schema", "meta", "canvas", "svg", "fonts", "layers", "states", "initialState", "panel",
			      "animations", "actions"},
			     diagnostics, /*allow_dollar_schema=*/true);

	Manifest manifest;

	auto schema_it = root.find("schema");
	if (schema_it == root.end() || !schema_it->is_number_integer()) {
		add_error(diagnostics, "NXG-E005", "schema", "missing or non-integer required field 'schema'");
	} else {
		manifest.schema = schema_it->get<int>();
		if (manifest.schema != kSupportedSchemaVersion) {
			add_error(diagnostics, "NXG-E002", "schema",
				  "unsupported schema version " + std::to_string(manifest.schema) +
					  " (supported: " + std::to_string(kSupportedSchemaVersion) + ")");
		}
	}

	auto svg_it = root.find("svg");
	if (svg_it == root.end() || !svg_it->is_string() || svg_it->get<std::string>().empty()) {
		add_error(diagnostics, "NXG-E005", "svg", "missing or empty required string 'svg'");
	} else {
		manifest.svg = svg_it->get<std::string>();
	}

	auto init_it = root.find("initialState");
	if (init_it != root.end()) {
		if (!init_it->is_string() || init_it->get<std::string>().empty()) {
			add_error(diagnostics, "NXG-E005", "initialState", "'initialState' must be a non-empty string");
		} else {
			manifest.initial_state = init_it->get<std::string>();
		}
	}

	parse_meta(root, manifest, diagnostics);
	parse_canvas(root, manifest, diagnostics);
	parse_fonts(root, manifest, diagnostics);
	parse_layers(root, manifest, diagnostics);
	parse_states(root, manifest, diagnostics);
	parse_panel(root, manifest, diagnostics);
	parse_animations(root, manifest, diagnostics);
	parse_actions(root, manifest, diagnostics);

	return manifest;
}

} // namespace naxgc
