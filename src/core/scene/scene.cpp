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

#include "core/scene/scene.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace naxgc {

namespace {

bool split_layer_prop_path(std::string_view path, std::string_view &out_layer, std::string_view &out_prop) noexcept
{
	constexpr std::string_view kPrefix = "layers.";
	if (path.rfind(kPrefix, 0) != 0) {
		return false;
	}
	const std::string_view rest = path.substr(kPrefix.size());
	const std::size_t dot = rest.find('.');
	if (dot == std::string_view::npos || dot == 0 || dot + 1 >= rest.size()) {
		return false;
	}
	out_layer = rest.substr(0, dot);
	out_prop = rest.substr(dot + 1);
	return out_prop.find('.') == std::string_view::npos;
}

Result<Color> extract_color(const PropertyValue &val)
{
	if (const auto *c = std::get_if<Color>(&val)) {
		return Result<Color>::ok(*c);
	}
	if (const auto *s = std::get_if<std::string>(&val)) {
		auto parsed = parse_color(*s);
		if (parsed.is_ok()) {
			return Result<Color>::ok(parsed.value());
		}
	}
	return Result<Color>::err(Error{"NXG-R002", "expected color value (#RRGGBB or #RRGGBBAA)"});
}

Result<double> extract_number(const PropertyValue &val)
{
	if (const auto *d = std::get_if<double>(&val)) {
		if (std::isfinite(*d)) {
			return Result<double>::ok(*d);
		}
	}
	return Result<double>::err(Error{"NXG-R002", "expected finite number value"});
}

Result<bool> extract_bool(const PropertyValue &val)
{
	if (const auto *b = std::get_if<bool>(&val)) {
		return Result<bool>::ok(*b);
	}
	return Result<bool>::err(Error{"NXG-R002", "expected boolean value"});
}

Result<std::string> extract_string(const PropertyValue &val)
{
	if (const auto *s = std::get_if<std::string>(&val)) {
		return Result<std::string>::ok(*s);
	}
	return Result<std::string>::err(Error{"NXG-R002", "expected string value"});
}

double detect_layer_auto_scale(const Manifest &manifest, std::string_view layer_name) noexcept
{
	double max_uniform_scale = 1.0;
	double max_axis_scale = 1.0;

	auto consider_prop = [&](std::string_view prop, const PropertyValue &val) {
		const auto *num = std::get_if<double>(&val);
		if (!num || !std::isfinite(*num)) {
			return;
		}
		const double abs_v = std::abs(*num);
		if (prop == "scale") {
			max_uniform_scale = std::max(max_uniform_scale, abs_v);
		} else if (prop == "scaleX" || prop == "scaleY") {
			max_axis_scale = std::max(max_axis_scale, abs_v);
		}
	};

	for (const auto &[sname, sdef] : manifest.states) {
		(void)sname;
		for (const auto &entry : sdef.entries) {
			std::string_view l_name;
			std::string_view p_name;
			if (split_layer_prop_path(entry.path, l_name, p_name) && l_name == layer_name) {
				consider_prop(p_name, entry.value);
			}
		}
	}

	for (const auto &[cname, cdef] : manifest.animations) {
		(void)cname;
		for (const auto &track : cdef.tracks) {
			if (track.layer != layer_name) {
				continue;
			}
			if (track.from.has_value()) {
				consider_prop(track.prop, *track.from);
			}
			if (track.to.has_value()) {
				consider_prop(track.prop, *track.to);
			}
			for (const auto &kf : track.keys) {
				consider_prop(track.prop, kf.value);
			}
		}
	}

	const double combined = max_uniform_scale * max_axis_scale;
	return std::clamp(combined, kMinAutoRasterScale, kMaxRasterScale);
}

Result<void> apply_property_mutation(SceneLayer &layer, std::string_view prop_name, const PropertyValue &value,
				     bool is_animated)
{
	const auto expected_kind = get_layer_property_kind(layer.type, prop_name);
	if (!expected_kind.has_value()) {
		return Result<void>::err(Error{"NXG-R001", "property '" + std::string(prop_name) +
								   "' does not exist on layer '" + layer.name + "'"});
	}

	auto mutate_scalar = [&](auto &anim_prop, auto &&extracted_res, bool marks_content_dirty) -> Result<void> {
		if (extracted_res.is_err()) {
			return Result<void>::err(extracted_res.error());
		}
		const bool changed = is_animated ? anim_prop.set_animated(std::move(extracted_res.value()))
						 : anim_prop.set_base(std::move(extracted_res.value()));
		if (changed && marks_content_dirty) {
			layer.content_dirty = true;
		}
		return Result<void>::ok();
	};

	if (prop_name == "x") {
		return mutate_scalar(layer.props.x, extract_number(value), false);
	}
	if (prop_name == "y") {
		return mutate_scalar(layer.props.y, extract_number(value), false);
	}
	if (prop_name == "scale") {
		return mutate_scalar(layer.props.scale, extract_number(value), false);
	}
	if (prop_name == "scaleX") {
		return mutate_scalar(layer.props.scale_x, extract_number(value), false);
	}
	if (prop_name == "scaleY") {
		return mutate_scalar(layer.props.scale_y, extract_number(value), false);
	}
	if (prop_name == "rotation") {
		return mutate_scalar(layer.props.rotation, extract_number(value), false);
	}
	if (prop_name == "opacity") {
		return mutate_scalar(layer.props.opacity, extract_number(value), false);
	}
	if (prop_name == "visible") {
		return mutate_scalar(layer.props.visible, extract_bool(value), false);
	}

	if (layer.type == LayerType::Svg) {
		if (prop_name == "fill") {
			auto c_res = extract_color(value);
			if (c_res.is_err()) {
				return Result<void>::err(c_res.error());
			}
			const std::optional<Color> opt_c = c_res.value();
			const bool changed = is_animated ? layer.props.fill.set_animated(opt_c)
							 : layer.props.fill.set_base(opt_c);
			if (changed) {
				layer.content_dirty = true;
			}
			return Result<void>::ok();
		}
		if (prop_name == "stroke") {
			auto c_res = extract_color(value);
			if (c_res.is_err()) {
				return Result<void>::err(c_res.error());
			}
			const std::optional<Color> opt_c = c_res.value();
			const bool changed = is_animated ? layer.props.stroke.set_animated(opt_c)
							 : layer.props.stroke.set_base(opt_c);
			if (changed) {
				layer.content_dirty = true;
			}
			return Result<void>::ok();
		}
		if (prop_name == "strokeWidth") {
			auto n_res = extract_number(value);
			if (n_res.is_err()) {
				return Result<void>::err(n_res.error());
			}
			if (n_res.value() < 0.0) {
				return Result<void>::err(Error{"NXG-R002", "strokeWidth must be non-negative"});
			}
			const std::optional<double> opt_w = n_res.value();
			const bool changed = is_animated ? layer.props.stroke_width.set_animated(opt_w)
							 : layer.props.stroke_width.set_base(opt_w);
			if (changed) {
				layer.content_dirty = true;
			}
			return Result<void>::ok();
		}
	} else if (layer.type == LayerType::Text) {
		if (prop_name == "text") {
			return mutate_scalar(layer.props.text, extract_string(value), true);
		}
		if (prop_name == "color") {
			return mutate_scalar(layer.props.color, extract_color(value), true);
		}
		if (prop_name == "size") {
			auto n_res = extract_number(value);
			if (n_res.is_err()) {
				return Result<void>::err(n_res.error());
			}
			if (n_res.value() <= 0.0) {
				return Result<void>::err(Error{"NXG-R002", "text size must be > 0"});
			}
			return mutate_scalar(layer.props.size, std::move(n_res), true);
		}
	} else if (layer.type == LayerType::Image) {
		if (prop_name == "src") {
			return mutate_scalar(layer.props.src, extract_string(value), true);
		}
	}

	return Result<void>::err(Error{"NXG-R001", "property '" + std::string(prop_name) +
							   "' not supported on layer '" + layer.name + "'"});
}

} // namespace

Scene Scene::from_template(const LoadedTemplate &tpl, bool apply_initial_state)
{
	Scene scene;
	const Manifest &manifest = tpl.manifest;
	const SvgInspectionResult &svg = tpl.svg_info;

	scene.canvas_ = manifest.canvas;
	scene.states_ = manifest.states;

	// Build SVG id -> order rank map so layers are sorted in exact SVG z-order
	std::unordered_map<std::string, std::size_t> svg_id_order;
	svg_id_order.reserve(svg.ordered_ids.size());
	for (std::size_t i = 0; i < svg.ordered_ids.size(); ++i) {
		svg_id_order.emplace(svg.ordered_ids[i], i);
	}

	struct OrderedLayerRef {
		std::string name;
		std::size_t svg_rank;
		std::size_t decl_rank;
	};

	std::vector<OrderedLayerRef> sorted_refs;
	sorted_refs.reserve(manifest.layer_declaration_order.size());
	for (std::size_t i = 0; i < manifest.layer_declaration_order.size(); ++i) {
		const std::string &lname = manifest.layer_declaration_order[i];
		const LayerDef &ldef = manifest.layers.at(lname);
		auto it = svg_id_order.find(ldef.id);
		const std::size_t rank = (it != svg_id_order.end()) ? it->second
								    : (std::numeric_limits<std::size_t>::max() / 2 + i);
		sorted_refs.push_back(OrderedLayerRef{lname, rank, i});
	}

	std::sort(sorted_refs.begin(), sorted_refs.end(), [](const OrderedLayerRef &a, const OrderedLayerRef &b) {
		if (a.svg_rank != b.svg_rank) {
			return a.svg_rank < b.svg_rank;
		}
		return a.decl_rank < b.decl_rank;
	});

	std::unordered_map<std::string, std::size_t> svg_id_to_layer_idx;
	scene.layers_.reserve(sorted_refs.size());

	for (std::size_t idx = 0; idx < sorted_refs.size(); ++idx) {
		const LayerDef &ldef = manifest.layers.at(sorted_refs[idx].name);
		SceneLayer layer;
		layer.index = idx;
		layer.name = ldef.name;
		layer.type = ldef.type;
		layer.role = ldef.role;
		layer.svg_id = ldef.id;
		layer.origin = ldef.origin;
		layer.mask_mode = ldef.mask_mode;

		// Text config
		layer.font = ldef.font;
		layer.weight = ldef.weight;
		layer.italic = ldef.italic;
		layer.min_size = ldef.min_size;
		layer.align = ldef.align;
		layer.valign = ldef.valign;
		layer.overflow = ldef.overflow;
		layer.max_lines = ldef.max_lines;
		layer.line_height = ldef.line_height;
		layer.letter_spacing = ldef.letter_spacing;
		layer.uppercase = ldef.uppercase;

		// Image config
		layer.fit = ldef.fit;

		// Initial base properties from LayerDef
		layer.props.opacity = AnimatableProperty<double>(ldef.opacity);
		layer.props.visible = AnimatableProperty<bool>(ldef.visible);
		layer.props.fill = AnimatableProperty<std::optional<Color>>(ldef.fill);
		layer.props.stroke = AnimatableProperty<std::optional<Color>>(ldef.stroke);
		layer.props.stroke_width = AnimatableProperty<std::optional<double>>(ldef.stroke_width);
		layer.props.text = AnimatableProperty<std::string>(ldef.text);
		layer.props.color = AnimatableProperty<Color>(ldef.color);
		layer.props.size = AnimatableProperty<double>(ldef.size);
		layer.props.src = AnimatableProperty<std::string>(ldef.src);

		// Initial rest bbox from SVG element attributes if available
		auto elem_it = svg.elements_by_id.find(ldef.id);
		if (elem_it != svg.elements_by_id.end()) {
			const SvgElementInfo &info = elem_it->second;
			layer.rest_bbox.x = info.x.value_or(0.0);
			layer.rest_bbox.y = info.y.value_or(0.0);
			layer.rest_bbox.width = info.width.value_or(0.0);
			layer.rest_bbox.height = info.height.value_or(0.0);
		}

		scene.name_to_index_.emplace(layer.name, idx);
		if (!layer.svg_id.empty()) {
			svg_id_to_layer_idx.emplace(layer.svg_id, idx);
		}
		scene.layers_.push_back(std::move(layer));
	}

	// Resolve parent-child hierarchy from SVG nesting and mask layer indices
	for (std::size_t idx = 0; idx < scene.layers_.size(); ++idx) {
		SceneLayer &layer = scene.layers_[idx];
		const LayerDef &ldef = manifest.layers.at(layer.name);

		if (ldef.mask.has_value()) {
			auto m_it = scene.name_to_index_.find(*ldef.mask);
			if (m_it != scene.name_to_index_.end()) {
				layer.mask_layer_index = m_it->second;
			}
		}

		auto elem_it = svg.elements_by_id.find(layer.svg_id);
		if (elem_it != svg.elements_by_id.end()) {
			const auto &ancestors = elem_it->second.ancestor_ids;
			// Search from innermost ancestor to outermost
			for (auto it = ancestors.rbegin(); it != ancestors.rend(); ++it) {
				auto p_it = svg_id_to_layer_idx.find(*it);
				if (p_it != svg_id_to_layer_idx.end() && p_it->second != idx) {
					layer.parent_index = p_it->second;
					scene.layers_[p_it->second].children_indices.push_back(idx);
					break;
				}
			}
		}
	}

	// Compute hybrid effective_raster_scale per layer (in parent-first order)
	for (std::size_t idx = 0; idx < scene.layers_.size(); ++idx) {
		SceneLayer &layer = scene.layers_[idx];
		const LayerDef &ldef = manifest.layers.at(layer.name);

		if (ldef.raster_scale.has_value()) {
			layer.effective_raster_scale =
				std::clamp(*ldef.raster_scale, kMinManualRasterScale, kMaxRasterScale);
		} else {
			double auto_scale = detect_layer_auto_scale(manifest, layer.name);
			if (layer.parent_index.has_value()) {
				const double parent_scale = scene.layers_[*layer.parent_index].effective_raster_scale;
				auto_scale =
					std::clamp(auto_scale * parent_scale, kMinAutoRasterScale, kMaxRasterScale);
			}
			layer.effective_raster_scale = auto_scale;
		}
	}

	// Initialize control values from panel definition
	if (manifest.panel.has_value()) {
		for (const auto &sec : manifest.panel->sections) {
			for (const auto &ctrl : sec.controls) {
				if (ctrl.key.empty()) {
					continue;
				}
				if (ctrl.default_value.has_value()) {
					scene.control_values_[ctrl.key] = *ctrl.default_value;
				} else if (!ctrl.binds.empty()) {
					auto cur = scene.get_property_effective(ctrl.binds.front());
					if (cur.is_ok()) {
						scene.control_values_[ctrl.key] = cur.value();
					}
				}
			}
		}
	}

	if (apply_initial_state && manifest.initial_state.has_value()) {
		scene.apply_state(*manifest.initial_state);
	}

	// All layers start dirty so the initial render rasterizes them once
	for (auto &layer : scene.layers_) {
		layer.content_dirty = true;
	}

	scene.evaluate_transforms();
	return scene;
}

const SceneLayer *Scene::find_layer(std::string_view name) const noexcept
{
	auto it = name_to_index_.find(std::string(name));
	if (it == name_to_index_.end()) {
		return nullptr;
	}
	return &layers_[it->second];
}

SceneLayer *Scene::find_layer(std::string_view name) noexcept
{
	auto it = name_to_index_.find(std::string(name));
	if (it == name_to_index_.end()) {
		return nullptr;
	}
	return &layers_[it->second];
}

std::optional<std::size_t> Scene::find_layer_index(std::string_view name) const noexcept
{
	auto it = name_to_index_.find(std::string(name));
	if (it == name_to_index_.end()) {
		return std::nullopt;
	}
	return it->second;
}

void Scene::set_layer_rest_bbox(std::size_t layer_index, const Rect2D &bbox)
{
	if (layer_index >= layers_.size()) {
		return;
	}
	layers_[layer_index].rest_bbox = bbox;
	evaluate_transforms();
}

Result<void> Scene::set_layer_property_base(std::string_view layer_name, std::string_view prop_name,
					    const PropertyValue &value)
{
	SceneLayer *layer = find_layer(layer_name);
	if (!layer) {
		return Result<void>::err(Error{"NXG-R001", "layer '" + std::string(layer_name) + "' does not exist"});
	}
	return apply_property_mutation(*layer, prop_name, value, false);
}

Result<void> Scene::set_property_base(std::string_view path, const PropertyValue &value)
{
	std::string_view layer_name;
	std::string_view prop_name;
	if (!split_layer_prop_path(path, layer_name, prop_name)) {
		return Result<void>::err(Error{"NXG-R001", "invalid property path '" + std::string(path) + "'"});
	}
	return set_layer_property_base(layer_name, prop_name, value);
}

Result<void> Scene::set_layer_property_animated(std::string_view layer_name, std::string_view prop_name,
						const PropertyValue &value)
{
	SceneLayer *layer = find_layer(layer_name);
	if (!layer) {
		return Result<void>::err(Error{"NXG-R001", "layer '" + std::string(layer_name) + "' does not exist"});
	}
	return apply_property_mutation(*layer, prop_name, value, true);
}

Result<void> Scene::clear_layer_property_animated(std::string_view layer_name, std::string_view prop_name)
{
	SceneLayer *layer = find_layer(layer_name);
	if (!layer) {
		return Result<void>::err(Error{"NXG-R001", "layer '" + std::string(layer_name) + "' does not exist"});
	}

	if (prop_name == "x") {
		layer->props.x.clear_animated();
		return Result<void>::ok();
	}
	if (prop_name == "y") {
		layer->props.y.clear_animated();
		return Result<void>::ok();
	}
	if (prop_name == "scale") {
		layer->props.scale.clear_animated();
		return Result<void>::ok();
	}
	if (prop_name == "scaleX") {
		layer->props.scale_x.clear_animated();
		return Result<void>::ok();
	}
	if (prop_name == "scaleY") {
		layer->props.scale_y.clear_animated();
		return Result<void>::ok();
	}
	if (prop_name == "rotation") {
		layer->props.rotation.clear_animated();
		return Result<void>::ok();
	}
	if (prop_name == "opacity") {
		layer->props.opacity.clear_animated();
		return Result<void>::ok();
	}
	if (prop_name == "visible") {
		layer->props.visible.clear_animated();
		return Result<void>::ok();
	}
	if (layer->type == LayerType::Svg) {
		if (prop_name == "fill") {
			if (layer->props.fill.clear_animated()) {
				layer->content_dirty = true;
			}
			return Result<void>::ok();
		}
		if (prop_name == "stroke") {
			if (layer->props.stroke.clear_animated()) {
				layer->content_dirty = true;
			}
			return Result<void>::ok();
		}
		if (prop_name == "strokeWidth") {
			if (layer->props.stroke_width.clear_animated()) {
				layer->content_dirty = true;
			}
			return Result<void>::ok();
		}
	} else if (layer->type == LayerType::Text) {
		if (prop_name == "text") {
			if (layer->props.text.clear_animated()) {
				layer->content_dirty = true;
			}
			return Result<void>::ok();
		}
		if (prop_name == "color") {
			if (layer->props.color.clear_animated()) {
				layer->content_dirty = true;
			}
			return Result<void>::ok();
		}
		if (prop_name == "size") {
			if (layer->props.size.clear_animated()) {
				layer->content_dirty = true;
			}
			return Result<void>::ok();
		}
	} else if (layer->type == LayerType::Image) {
		if (prop_name == "src") {
			if (layer->props.src.clear_animated()) {
				layer->content_dirty = true;
			}
			return Result<void>::ok();
		}
	}

	return Result<void>::err(Error{"NXG-R001", "property '" + std::string(prop_name) +
							   "' does not exist on layer '" + layer->name + "'"});
}

Result<PropertyValue> Scene::get_layer_property_effective(std::string_view layer_name, std::string_view prop_name) const
{
	const SceneLayer *layer = find_layer(layer_name);
	if (!layer) {
		return Result<PropertyValue>::err(
			Error{"NXG-R001", "layer '" + std::string(layer_name) + "' does not exist"});
	}

	if (prop_name == "x") {
		return Result<PropertyValue>::ok(layer->props.x.effective());
	}
	if (prop_name == "y") {
		return Result<PropertyValue>::ok(layer->props.y.effective());
	}
	if (prop_name == "scale") {
		return Result<PropertyValue>::ok(layer->props.scale.effective());
	}
	if (prop_name == "scaleX") {
		return Result<PropertyValue>::ok(layer->props.scale_x.effective());
	}
	if (prop_name == "scaleY") {
		return Result<PropertyValue>::ok(layer->props.scale_y.effective());
	}
	if (prop_name == "rotation") {
		return Result<PropertyValue>::ok(layer->props.rotation.effective());
	}
	if (prop_name == "opacity") {
		return Result<PropertyValue>::ok(layer->props.opacity.effective());
	}
	if (prop_name == "visible") {
		return Result<PropertyValue>::ok(layer->props.visible.effective());
	}

	if (layer->type == LayerType::Svg) {
		if (prop_name == "fill") {
			const auto &opt = layer->props.fill.effective();
			return Result<PropertyValue>::ok(opt.value_or(Color{0, 0, 0, 255}));
		}
		if (prop_name == "stroke") {
			const auto &opt = layer->props.stroke.effective();
			return Result<PropertyValue>::ok(opt.value_or(Color{0, 0, 0, 255}));
		}
		if (prop_name == "strokeWidth") {
			const auto &opt = layer->props.stroke_width.effective();
			return Result<PropertyValue>::ok(opt.value_or(0.0));
		}
	} else if (layer->type == LayerType::Text) {
		if (prop_name == "text") {
			return Result<PropertyValue>::ok(layer->props.text.effective());
		}
		if (prop_name == "color") {
			return Result<PropertyValue>::ok(layer->props.color.effective());
		}
		if (prop_name == "size") {
			return Result<PropertyValue>::ok(layer->props.size.effective());
		}
	} else if (layer->type == LayerType::Image) {
		if (prop_name == "src") {
			return Result<PropertyValue>::ok(layer->props.src.effective());
		}
	}

	return Result<PropertyValue>::err(Error{"NXG-R001", "property '" + std::string(prop_name) +
								    "' does not exist on layer '" + layer->name + "'"});
}

Result<PropertyValue> Scene::get_property_effective(std::string_view path) const
{
	std::string_view layer_name;
	std::string_view prop_name;
	if (!split_layer_prop_path(path, layer_name, prop_name)) {
		return Result<PropertyValue>::err(
			Error{"NXG-R001", "invalid property path '" + std::string(path) + "'"});
	}
	return get_layer_property_effective(layer_name, prop_name);
}

Result<void> Scene::apply_state(std::string_view state_name)
{
	auto it = states_.find(std::string(state_name));
	if (it == states_.end()) {
		return Result<void>::err(Error{"NXG-R003", "state '" + std::string(state_name) + "' does not exist"});
	}

	for (const auto &entry : it->second.entries) {
		auto res = set_property_base(entry.path, entry.value);
		if (res.is_err()) {
			return res;
		}
	}
	evaluate_transforms();
	return Result<void>::ok();
}

void Scene::evaluate_transforms() noexcept
{
	for (std::size_t i = 0; i < layers_.size(); ++i) {
		SceneLayer &layer = layers_[i];
		const double dx = layer.props.x.effective();
		const double dy = layer.props.y.effective();
		const double s = layer.props.scale.effective();
		const double sx = s * layer.props.scale_x.effective();
		const double sy = s * layer.props.scale_y.effective();
		const double rot = layer.props.rotation.effective();
		const double local_op = std::clamp(layer.props.opacity.effective(), 0.0, 1.0);
		const bool local_vis = layer.props.visible.effective();

		layer.local_matrix = compute_layer_local_matrix(layer.rest_bbox, layer.origin, dx, dy, sx, sy, rot);

		if (layer.parent_index.has_value() && *layer.parent_index < i) {
			const SceneLayer &parent = layers_[*layer.parent_index];
			layer.world_matrix = parent.world_matrix * layer.local_matrix;
			layer.effective_opacity = parent.effective_opacity * local_op;
			layer.effective_visible = parent.effective_visible && local_vis;
		} else {
			layer.world_matrix = layer.local_matrix;
			layer.effective_opacity = local_op;
			layer.effective_visible = local_vis;
		}
	}
}

} // namespace naxgc
