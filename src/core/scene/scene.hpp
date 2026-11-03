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

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/manifest/manifest_types.hpp"
#include "core/manifest/manifest_validator.hpp"
#include "core/util/color.hpp"
#include "core/util/mat2d.hpp"
#include "core/util/result.hpp"

namespace naxgc {

constexpr double kMinAutoRasterScale = 1.0;
constexpr double kMaxRasterScale = 4.0;
constexpr double kMinManualRasterScale = 0.1;

template<typename T> struct AnimatableProperty {
	T base{};
	std::optional<T> animated{};

	constexpr AnimatableProperty() = default;
	constexpr explicit AnimatableProperty(T initial) : base(std::move(initial)) {}

	const T &effective() const noexcept { return animated.has_value() ? *animated : base; }

	bool set_base(T val)
	{
		const T prev = effective();
		base = std::move(val);
		return !(effective() == prev);
	}

	bool set_animated(T val)
	{
		const T prev = effective();
		animated = std::move(val);
		return !(effective() == prev);
	}

	bool clear_animated()
	{
		if (!animated.has_value()) {
			return false;
		}
		const T prev = effective();
		animated.reset();
		return !(effective() == prev);
	}
};

struct LayerProperties {
	// Transform & visibility properties (GPU-only; changing them never triggers re-rasterization)
	AnimatableProperty<double> x{0.0};
	AnimatableProperty<double> y{0.0};
	AnimatableProperty<double> scale{1.0};
	AnimatableProperty<double> scale_x{1.0};
	AnimatableProperty<double> scale_y{1.0};
	AnimatableProperty<double> rotation{0.0};
	AnimatableProperty<double> opacity{1.0};
	AnimatableProperty<bool> visible{true};

	// SVG content properties (changing effective value triggers re-rasterization)
	AnimatableProperty<std::optional<Color>> fill{std::nullopt};
	AnimatableProperty<std::optional<Color>> stroke{std::nullopt};
	AnimatableProperty<std::optional<double>> stroke_width{std::nullopt};

	// Text content properties (changing effective value triggers re-rasterization)
	AnimatableProperty<std::string> text{std::string{}};
	AnimatableProperty<Color> color{Color{255, 255, 255, 255}};
	AnimatableProperty<double> size{32.0};

	// Image content properties (changing effective value triggers re-rasterization)
	AnimatableProperty<std::string> src{std::string{}};
};

struct SceneLayer {
	std::size_t index{0};
	std::string name;
	LayerType type{LayerType::Svg};
	LayerRole role{LayerRole::Normal};
	std::string svg_id;
	std::array<double, 2> origin{0.0, 0.0};

	std::optional<std::size_t> mask_layer_index;
	MaskMode mask_mode{MaskMode::Alpha};

	std::optional<std::size_t> parent_index;
	std::vector<std::size_t> children_indices;

	double effective_raster_scale{1.0};

	// Static text layout configuration from LayerDef
	std::string font;
	int weight{400};
	bool italic{false};
	double min_size{32.0};
	TextAlign align{TextAlign::Left};
	TextVAlign valign{TextVAlign::Middle};
	TextOverflow overflow{TextOverflow::Clip};
	int max_lines{1};
	double line_height{1.2};
	double letter_spacing{0.0};
	bool uppercase{false};

	// Static image layout configuration from LayerDef
	ImageFit fit{ImageFit::Contain};

	// Dynamic properties
	LayerProperties props;

	// Geometry and evaluated transform state
	Rect2D rest_bbox{};
	Mat2D local_matrix{};
	Mat2D world_matrix{};
	double effective_opacity{1.0};
	bool effective_visible{true};
	bool content_dirty{true};
};

class Scene {
public:
	Scene() = default;

	/**
	 * Constructs a Scene from a validated LoadedTemplate.
	 * Layers are ordered in SVG z-order, parent/child hierarchy is derived from SVG nesting,
	 * and hybrid `effective_raster_scale` is computed per layer.
	 * If `apply_initial_state` is true and `manifest.initial_state` is set, that state is applied to base values.
	 */
	static Scene from_template(const LoadedTemplate &tpl, bool apply_initial_state = false);

	const CanvasDef &canvas() const noexcept { return canvas_; }

	const std::vector<SceneLayer> &layers() const noexcept { return layers_; }
	std::vector<SceneLayer> &layers() noexcept { return layers_; }

	const SceneLayer *find_layer(std::string_view name) const noexcept;
	SceneLayer *find_layer(std::string_view name) noexcept;
	std::optional<std::size_t> find_layer_index(std::string_view name) const noexcept;

	/**
	 * Updates the rest-pose bounding box of a layer (e.g. refined by lunasvg for groups/paths)
	 * and recomputes world transforms.
	 */
	void set_layer_rest_bbox(std::size_t layer_index, const Rect2D &bbox);

	/**
	 * Sets the base value of a layer property by layer name and property name.
	 * Marks `content_dirty = true` if and only if a content property's effective value changes.
	 * Returns NXG-R001 if layer/property does not exist, or NXG-R002 if value type is incompatible.
	 */
	Result<void> set_layer_property_base(std::string_view layer_name, std::string_view prop_name,
					     const PropertyValue &value);

	/**
	 * Sets the base value of a layer property by full path `"layers.<name>.<prop>"`.
	 */
	Result<void> set_property_base(std::string_view path, const PropertyValue &value);

	/**
	 * Sets the animated value of a layer property by layer name and property name.
	 * Marks `content_dirty = true` if and only if a content property's effective value changes.
	 */
	Result<void> set_layer_property_animated(std::string_view layer_name, std::string_view prop_name,
						 const PropertyValue &value);

	/**
	 * Clears the animated override on a layer property, reverting to its base value.
	 */
	Result<void> clear_layer_property_animated(std::string_view layer_name, std::string_view prop_name);

	/**
	 * Reads the current effective value of a layer property.
	 */
	Result<PropertyValue> get_layer_property_effective(std::string_view layer_name,
							   std::string_view prop_name) const;

	/**
	 * Reads the current effective value by path `"layers.<name>.<prop>"`.
	 */
	Result<PropertyValue> get_property_effective(std::string_view path) const;

	/**
	 * Applies a named state from the manifest to base properties.
	 * Returns NXG-R003 if the state does not exist.
	 */
	Result<void> apply_state(std::string_view state_name);

	/**
	 * Evaluates `local_matrix`, `world_matrix`, `effective_opacity`, and `effective_visible`
	 * for all layers in parent-first order.
	 */
	void evaluate_transforms() noexcept;

	const std::unordered_map<std::string, PropertyValue> &control_values() const noexcept
	{
		return control_values_;
	}

private:
	CanvasDef canvas_{};
	std::vector<SceneLayer> layers_;
	std::unordered_map<std::string, std::size_t> name_to_index_;
	std::unordered_map<std::string, StateDef> states_;
	std::unordered_map<std::string, PropertyValue> control_values_;
};

} // namespace naxgc
