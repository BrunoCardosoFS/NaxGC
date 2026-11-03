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
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "core/util/color.hpp"

namespace naxgc {

constexpr int kSupportedSchemaVersion = 1;

constexpr std::size_t kMaxManifestBytes = 1 * 1024 * 1024; // 1 MB
constexpr std::size_t kMaxSvgBytes = 8 * 1024 * 1024;      // 8 MB
constexpr std::size_t kMaxFontBytes = 20 * 1024 * 1024;    // 20 MB
constexpr std::size_t kMaxImageBytes = 20 * 1024 * 1024;   // 20 MB

constexpr std::size_t kMaxLayers = 512;
constexpr std::size_t kMaxClips = 512;
constexpr std::size_t kMaxTriggersPerClip = 64;
constexpr std::size_t kMaxTriggerDepth = 16;
constexpr std::size_t kMaxActiveClipInstances = 256;

using PropertyValue = std::variant<bool, double, std::string, Color>;

enum class PropertyValueKind {
	Bool,
	Number,
	String,
	Color,
};

inline PropertyValueKind get_property_value_kind(const PropertyValue &v) noexcept
{
	if (std::holds_alternative<bool>(v)) {
		return PropertyValueKind::Bool;
	}
	if (std::holds_alternative<double>(v)) {
		return PropertyValueKind::Number;
	}
	if (std::holds_alternative<std::string>(v)) {
		return PropertyValueKind::String;
	}
	return PropertyValueKind::Color;
}

struct MetaDef {
	std::string id;
	std::string name;
	std::string version;
	std::string author;
	std::string description;
	std::optional<std::string> requires_naxgc;
};

struct CanvasDef {
	uint32_t width{1920};
	uint32_t height{1080};
	double fps{60.0};
};

struct FontDef {
	std::string family;
	std::string file;
	std::string json_path;
};

enum class LayerType {
	Svg,
	Text,
	Image,
};

enum class LayerRole {
	Normal,
	Mask,
};

enum class MaskMode {
	Alpha,
	Inverse,
};

enum class TextAlign {
	Left,
	Center,
	Right,
};

enum class TextVAlign {
	Top,
	Middle,
	Bottom,
};

enum class TextOverflow {
	Shrink,
	Clip,
	Ellipsis,
	Wrap,
};

enum class ImageFit {
	Contain,
	Cover,
	Stretch,
};

struct LayerDef {
	std::string name;
	std::string json_path;
	LayerType type{LayerType::Svg};
	std::string id;
	LayerRole role{LayerRole::Normal};
	std::array<double, 2> origin{0.0, 0.0};
	double opacity{1.0};
	bool visible{true};
	std::optional<std::string> mask;
	MaskMode mask_mode{MaskMode::Alpha};
	std::optional<double> raster_scale;

	// SVG-specific
	std::optional<Color> fill;
	std::optional<Color> stroke;
	std::optional<double> stroke_width;

	// Text-specific
	std::string text;
	std::string font;
	int weight{400};
	bool italic{false};
	double size{32.0};
	double min_size{32.0};
	Color color{255, 255, 255, 255};
	TextAlign align{TextAlign::Left};
	TextVAlign valign{TextVAlign::Middle};
	TextOverflow overflow{TextOverflow::Clip};
	int max_lines{1};
	double line_height{1.2};
	double letter_spacing{0.0};
	bool uppercase{false};

	// Image-specific
	std::string src;
	ImageFit fit{ImageFit::Contain};
};

/**
 * Returns the expected PropertyValueKind for a given layer type and property name,
 * or std::nullopt if the property does not exist or is incompatible with the layer type.
 */
inline std::optional<PropertyValueKind> get_layer_property_kind(LayerType layer_type, std::string_view prop) noexcept
{
	if (prop == "x" || prop == "y" || prop == "scale" || prop == "scaleX" || prop == "scaleY" ||
	    prop == "rotation" || prop == "opacity") {
		return PropertyValueKind::Number;
	}
	if (prop == "visible") {
		return PropertyValueKind::Bool;
	}

	switch (layer_type) {
	case LayerType::Svg:
		if (prop == "fill" || prop == "stroke") {
			return PropertyValueKind::Color;
		}
		if (prop == "strokeWidth") {
			return PropertyValueKind::Number;
		}
		break;
	case LayerType::Text:
		if (prop == "text") {
			return PropertyValueKind::String;
		}
		if (prop == "color") {
			return PropertyValueKind::Color;
		}
		if (prop == "size") {
			return PropertyValueKind::Number;
		}
		break;
	case LayerType::Image:
		if (prop == "src") {
			return PropertyValueKind::String;
		}
		break;
	}

	return std::nullopt;
}

enum class EasingType {
	Linear,
	InQuad,
	OutQuad,
	InOutQuad,
	InCubic,
	OutCubic,
	InOutCubic,
	InSine,
	OutSine,
	InOutSine,
	InExpo,
	OutExpo,
	InOutExpo,
	InBack,
	OutBack,
	InOutBack,
	OutElastic,
	OutBounce,
	CubicBezier,
};

struct EasingSpec {
	EasingType type{EasingType::Linear};
	std::array<double, 4> bezier{0.0, 0.0, 1.0, 1.0};
};

enum class BlendMode {
	Override,
	Add,
};

struct Keyframe {
	double time{0.0};
	PropertyValue value;
};

struct TrackDef {
	std::string json_path;
	std::string layer;
	std::string prop;
	std::optional<PropertyValue> from;
	std::optional<PropertyValue> to;
	std::vector<Keyframe> keys;
	bool has_keys{false};
	double start{0.0};
	std::optional<double> dur;
	EasingSpec ease;
	BlendMode blend{BlendMode::Override};
};

enum class TriggerEventKind {
	AtTime,
	OnEnd,
	OnMarker,
};

enum class TriggerMode {
	Linked,
	Detached,
};

struct TriggerDef {
	std::string json_path;
	std::string play;
	TriggerEventKind event_kind{TriggerEventKind::AtTime};
	double at{0.0};
	std::string marker_name;
	double delay{0.0};
	TriggerMode mode{TriggerMode::Linked};
	bool inherit_speed{false};
	double speed{1.0};
};

enum class OnEndPolicy {
	Hold,
	Revert,
};

enum class RetriggerPolicy {
	Restart,
	Ignore,
	Stack,
};

struct ClipDef {
	std::string name;
	std::string json_path;
	std::vector<TrackDef> tracks;
	std::vector<TriggerDef> triggers;
	std::unordered_map<std::string, double> markers;
	std::optional<double> duration;
	bool loop{false};
	OnEndPolicy on_end{OnEndPolicy::Hold};
	RetriggerPolicy retrigger{RetriggerPolicy::Restart};
};

enum class ControlType {
	Text,
	Number,
	Toggle,
	Color,
	Dropdown,
	Button,
	Label,
	Separator,
};

enum class ButtonStyle {
	Default,
	Primary,
	Danger,
};

struct DropdownOptionDef {
	std::string json_path;
	std::string label;
	std::optional<PropertyValue> value;
	std::optional<std::string> apply;
};

struct ControlDef {
	std::string json_path;
	ControlType type{ControlType::Text};
	std::string key;
	std::string label;
	std::vector<std::string> binds;
	std::optional<PropertyValue> default_value;
	std::string tooltip;

	// Text control
	std::string placeholder;
	std::optional<int> max_length;
	bool multiline{false};
	std::optional<std::string> submit_action;

	// Number control
	std::optional<double> min;
	std::optional<double> max;
	std::optional<double> step;
	bool slider{false};
	std::string unit;

	// Dropdown control
	std::vector<DropdownOptionDef> options;

	// Button control
	std::string action;
	ButtonStyle style{ButtonStyle::Default};
	std::string hotkey;
};

struct PanelSectionDef {
	std::string json_path;
	std::string title;
	bool collapsed{false};
	int columns{1};
	std::vector<ControlDef> controls;
};

struct PanelDef {
	std::string title;
	std::vector<PanelSectionDef> sections;
};

enum class ActionStepType {
	Play,
	Stop,
	Set,
	Call,
};

struct ActionStepDef {
	std::string json_path;
	ActionStepType type{ActionStepType::Play};
	std::string target;
	double delay{0.0};
};

struct StateEntryDef {
	std::string json_path;
	std::string path;
	PropertyValue value;
};

struct StateDef {
	std::string name;
	std::string json_path;
	std::vector<StateEntryDef> entries;
};

struct Manifest {
	int schema{1};
	MetaDef meta;
	CanvasDef canvas;
	std::string svg;
	std::optional<std::string> initial_state;
	std::vector<FontDef> fonts;
	std::unordered_map<std::string, LayerDef> layers;
	std::vector<std::string> layer_declaration_order;
	std::unordered_map<std::string, StateDef> states;
	std::vector<std::string> state_declaration_order;
	std::optional<PanelDef> panel;
	std::unordered_map<std::string, ClipDef> animations;
	std::vector<std::string> clip_declaration_order;
	std::unordered_map<std::string, std::vector<ActionStepDef>> actions;
	std::vector<std::string> action_declaration_order;
};

} // namespace naxgc
