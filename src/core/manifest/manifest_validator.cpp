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

#include "core/manifest/manifest_validator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include "core/manifest/manifest_parser.hpp"
#include "core/util/safe_path.hpp"

namespace naxgc {

namespace {

constexpr int kEngineMajor = 0;
constexpr int kEngineMinor = 1;
constexpr int kEnginePatch = 0;

void add_error(std::vector<Diagnostic> &diags, std::string code, std::string path, std::string msg)
{
	diags.push_back(Diagnostic{DiagnosticSeverity::Error, std::move(code), std::move(path), std::move(msg)});
}

void add_warning(std::vector<Diagnostic> &diags, std::string code, std::string path, std::string msg)
{
	diags.push_back(Diagnostic{DiagnosticSeverity::Warning, std::move(code), std::move(path), std::move(msg)});
}

bool parse_semver_triplet(std::string_view ver, int &major, int &minor, int &patch)
{
	std::string s(ver);
	int consumed = 0;
	if (std::sscanf(s.c_str(), "%d.%d.%d%n", &major, &minor, &patch, &consumed) != 3) {
		return false;
	}
	if (major < 0 || minor < 0 || patch < 0) {
		return false;
	}
	return static_cast<std::size_t>(consumed) == s.size();
}

bool check_requires_naxgc(std::string_view constraint)
{
	while (!constraint.empty() && constraint.front() == ' ') {
		constraint.remove_prefix(1);
	}
	while (!constraint.empty() && constraint.back() == ' ') {
		constraint.remove_suffix(1);
	}

	std::string_view op = "==";
	if (constraint.rfind(">=", 0) == 0) {
		op = ">=";
		constraint.remove_prefix(2);
	} else if (constraint.rfind("<=", 0) == 0) {
		op = "<=";
		constraint.remove_prefix(2);
	} else if (constraint.rfind("==", 0) == 0) {
		op = "==";
		constraint.remove_prefix(2);
	} else if (constraint.rfind("^", 0) == 0) {
		op = ">=";
		constraint.remove_prefix(1);
	} else if (constraint.rfind(">", 0) == 0) {
		op = ">";
		constraint.remove_prefix(1);
	} else if (constraint.rfind("<", 0) == 0) {
		op = "<";
		constraint.remove_prefix(1);
	}

	while (!constraint.empty() && constraint.front() == ' ') {
		constraint.remove_prefix(1);
	}

	int req_maj = 0;
	int req_min = 0;
	int req_pat = 0;
	if (!parse_semver_triplet(constraint, req_maj, req_min, req_pat)) {
		return false;
	}

	const auto cur = std::tie(kEngineMajor, kEngineMinor, kEnginePatch);
	const auto req = std::tie(req_maj, req_min, req_pat);

	if (op == ">=") {
		return cur >= req;
	}
	if (op == "<=") {
		return cur <= req;
	}
	if (op == ">") {
		return cur > req;
	}
	if (op == "<") {
		return cur < req;
	}
	return cur == req;
}

bool coerce_property_value(PropertyValue &val, PropertyValueKind expected)
{
	if (expected == PropertyValueKind::Color) {
		if (std::holds_alternative<Color>(val)) {
			return true;
		}
		if (const auto *str = std::get_if<std::string>(&val)) {
			auto parsed = parse_color(*str);
			if (parsed.is_ok()) {
				val = parsed.value();
				return true;
			}
		}
		return false;
	}
	return get_property_value_kind(val) == expected;
}

bool split_layer_property_path(std::string_view path, std::string_view &out_layer, std::string_view &out_prop) noexcept
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

bool is_control_type_compatible_with_property(ControlType ctrl_type, PropertyValueKind prop_kind) noexcept
{
	switch (ctrl_type) {
	case ControlType::Text:
		return prop_kind == PropertyValueKind::String;
	case ControlType::Number:
		return prop_kind == PropertyValueKind::Number;
	case ControlType::Toggle:
		return prop_kind == PropertyValueKind::Bool;
	case ControlType::Color:
		return prop_kind == PropertyValueKind::Color;
	case ControlType::Dropdown:
		return true;
	case ControlType::Button:
	case ControlType::Label:
	case ControlType::Separator:
		return false;
	}
	return false;
}

void validate_triggers_graph(const Manifest &manifest, std::vector<Diagnostic> &diags)
{
	// 0 = unvisited, 1 = visiting (on stack), 2 = visited
	std::unordered_map<std::string, int> state;
	std::unordered_map<std::string, std::size_t> max_depth_from;
	std::vector<std::string> stack;
	bool reported_depth_exceeded = false;

	auto dfs = [&](auto &self, const std::string &clip_name) -> std::size_t {
		state[clip_name] = 1;
		stack.push_back(clip_name);

		std::size_t best_child_depth = 0;
		auto c_it = manifest.animations.find(clip_name);
		if (c_it != manifest.animations.end()) {
			for (const auto &trig : c_it->second.triggers) {
				if (trig.play.empty() ||
				    manifest.animations.find(trig.play) == manifest.animations.end()) {
					continue;
				}
				const int target_state = state[trig.play];
				if (target_state == 1) {
					// Reconstruct cycle path
					std::string cycle_str;
					auto start_it = std::find(stack.begin(), stack.end(), trig.play);
					for (auto it = start_it; it != stack.end(); ++it) {
						cycle_str += *it + " -> ";
					}
					cycle_str += trig.play;
					add_error(diags, "NXG-E021", trig.json_path, "trigger cycle: " + cycle_str);
				} else if (target_state == 0) {
					const std::size_t d = 1 + self(self, trig.play);
					if (d > best_child_depth) {
						best_child_depth = d;
					}
				} else {
					const std::size_t d = 1 + max_depth_from[trig.play];
					if (d > best_child_depth) {
						best_child_depth = d;
					}
				}
			}
		}

		stack.pop_back();
		state[clip_name] = 2;
		max_depth_from[clip_name] = best_child_depth;
		return best_child_depth;
	};

	for (const std::string &clip_name : manifest.clip_declaration_order) {
		const ClipDef &clip_def = manifest.animations.at(clip_name);
		if (state[clip_name] == 0) {
			dfs(dfs, clip_name);
		}
		if (!reported_depth_exceeded && max_depth_from[clip_name] > kMaxTriggerDepth) {
			reported_depth_exceeded = true;
			add_error(diags, "NXG-E060", clip_def.json_path + ".triggers",
				  "trigger nesting depth (" + std::to_string(max_depth_from[clip_name]) +
					  ") exceeds maximum limit (" + std::to_string(kMaxTriggerDepth) + ")");
		}
	}
}

void validate_actions_graph(const Manifest &manifest, std::vector<Diagnostic> &diags)
{
	std::unordered_map<std::string, int> state;
	std::vector<std::string> stack;

	auto dfs = [&](auto &self, const std::string &act_name) -> void {
		state[act_name] = 1;
		stack.push_back(act_name);

		auto a_it = manifest.actions.find(act_name);
		if (a_it != manifest.actions.end()) {
			for (const auto &step : a_it->second) {
				if (step.type != ActionStepType::Call || step.target.empty()) {
					continue;
				}
				if (manifest.actions.find(step.target) == manifest.actions.end()) {
					continue;
				}
				const int target_state = state[step.target];
				if (target_state == 1) {
					std::string cycle_str;
					auto start_it = std::find(stack.begin(), stack.end(), step.target);
					for (auto it = start_it; it != stack.end(); ++it) {
						cycle_str += *it + " -> ";
					}
					cycle_str += step.target;
					add_error(diags, "NXG-E041", step.json_path, "action call cycle: " + cycle_str);
				} else if (target_state == 0) {
					self(self, step.target);
				}
			}
		}

		stack.pop_back();
		state[act_name] = 2;
	};

	for (const std::string &act_name : manifest.action_declaration_order) {
		if (state[act_name] == 0) {
			dfs(dfs, act_name);
		}
	}
}

} // namespace

void validate_manifest_package(Manifest &manifest, const std::filesystem::path &package_root,
			       std::optional<SvgInspectionResult> &out_svg_info, std::string &out_svg_content,
			       std::vector<Diagnostic> &diagnostics)
{
	// 1. Validate meta.requires.naxgc
	if (manifest.meta.requires_naxgc.has_value()) {
		if (!check_requires_naxgc(*manifest.meta.requires_naxgc)) {
			add_error(diagnostics, "NXG-E002", "meta.requires.naxgc",
				  "template requires NaxGC '" + *manifest.meta.requires_naxgc +
					  "', which is incompatible with version 0.1.0");
		}
	}

	// 2. Resource count limits (NXG-E060)
	if (manifest.layers.size() > kMaxLayers) {
		add_error(diagnostics, "NXG-E060", "layers",
			  "layer count (" + std::to_string(manifest.layers.size()) + ") exceeds maximum (" +
				  std::to_string(kMaxLayers) + ")");
	}
	if (manifest.animations.size() > kMaxClips) {
		add_error(diagnostics, "NXG-E060", "animations",
			  "clip count (" + std::to_string(manifest.animations.size()) + ") exceeds maximum (" +
				  std::to_string(kMaxClips) + ")");
	}

	// 3. Validate fonts (NXG-W002 if missing, NXG-E003 if unsafe/oversize)
	for (const auto &font : manifest.fonts) {
		if (font.file.empty()) {
			continue;
		}
		const AssetCheckResult res = check_package_asset(package_root, font.file, kMaxFontBytes);
		if (res.status == AssetCheckStatus::Missing) {
			add_warning(diagnostics, "NXG-W002", font.json_path + ".file",
				    "package font file '" + font.file + "' is missing; using system fallback");
		} else if (res.status == AssetCheckStatus::UnsafeOrOversize) {
			add_error(diagnostics, "NXG-E003", font.json_path + ".file", res.message);
		}
	}

	// 4. Read and inspect SVG (NXG-E003, NXG-E004, NXG-W005)
	if (!manifest.svg.empty()) {
		auto svg_file = read_package_file_bounded(package_root, manifest.svg, kMaxSvgBytes, "NXG-E003");
		if (svg_file.is_err()) {
			add_error(diagnostics, svg_file.error().code, "svg", svg_file.error().message);
		} else {
			out_svg_content = std::move(svg_file.value());
			auto inspected = inspect_svg(out_svg_content);
			if (inspected.is_err()) {
				add_error(diagnostics, inspected.error().code, "svg", inspected.error().message);
			} else {
				out_svg_info = std::move(inspected.value());
			}
		}
	}

	// 5. Validate layers vs SVG and image assets (NXG-E003, NXG-E010, NXG-E011, NXG-E012, NXG-W001, NXG-W005)
	std::unordered_map<std::string, std::string> svg_id_to_layer_name;
	if (out_svg_info.has_value()) {
		const SvgInspectionResult &svg = *out_svg_info;

		for (const std::string &dup_svg_id : svg.duplicate_ids) {
			add_error(diagnostics, "NXG-E011", "svg", "duplicate id '" + dup_svg_id + "' inside SVG file");
		}

		// Compare canvas size with SVG size / viewBox (NXG-W005)
		const double cw = static_cast<double>(manifest.canvas.width);
		const double ch = static_cast<double>(manifest.canvas.height);
		bool size_mismatch = false;
		if (svg.viewbox_width.has_value() && svg.viewbox_height.has_value()) {
			if (std::abs(*svg.viewbox_width - cw) > 1e-3 || std::abs(*svg.viewbox_height - ch) > 1e-3) {
				size_mismatch = true;
			}
		}
		if (svg.width.has_value() && svg.height.has_value()) {
			if (std::abs(*svg.width - cw) > 1e-3 || std::abs(*svg.height - ch) > 1e-3) {
				size_mismatch = true;
			}
		}
		if (size_mismatch) {
			add_warning(diagnostics, "NXG-W005", "canvas",
				    "canvas dimensions differ from SVG dimensions/viewBox; proportional scale applied");
		}
	}

	for (const std::string &layer_name : manifest.layer_declaration_order) {
		const LayerDef &layer = manifest.layers.at(layer_name);

		if (layer.type == LayerType::Image && !layer.src.empty()) {
			const AssetCheckResult res = check_package_asset(package_root, layer.src, kMaxImageBytes);
			if (res.status != AssetCheckStatus::Ok) {
				add_error(diagnostics, "NXG-E003", layer.json_path + ".src", res.message);
			}
		}

		if (!layer.id.empty()) {
			auto [it, inserted] = svg_id_to_layer_name.emplace(layer.id, layer.name);
			if (!inserted) {
				add_error(diagnostics, "NXG-E011", layer.json_path + ".id",
					  "SVG id '" + layer.id + "' is already used by layer '" + it->second + "'");
			} else if (out_svg_info.has_value()) {
				if (out_svg_info->elements_by_id.find(layer.id) == out_svg_info->elements_by_id.end()) {
					add_error(diagnostics, "NXG-E010", layer.json_path + ".id",
						  "layer id '" + layer.id + "' does not exist in SVG '" + manifest.svg +
							  "'");
				}
			}
		}

		if (layer.mask.has_value()) {
			if (layer.role == LayerRole::Mask) {
				add_error(diagnostics, "NXG-E012", layer.json_path + ".mask",
					  "mask layer '" + layer.name + "' cannot have a mask itself");
			} else {
				auto m_it = manifest.layers.find(*layer.mask);
				if (m_it == manifest.layers.end()) {
					add_error(diagnostics, "NXG-E012", layer.json_path + ".mask",
						  "referenced mask layer '" + *layer.mask + "' does not exist");
				} else if (m_it->second.role != LayerRole::Mask) {
					add_error(diagnostics, "NXG-E012", layer.json_path + ".mask",
						  "layer '" + *layer.mask + "' must have role: \"mask\"");
				} else if (m_it->second.mask.has_value()) {
					add_error(diagnostics, "NXG-E012", layer.json_path + ".mask",
						  "mask of a mask is not supported");
				}
			}
		}
	}

	// Check uncovered visual SVG elements (NXG-W001)
	if (out_svg_info.has_value()) {
		for (const auto &prim : out_svg_info->visual_primitives) {
			bool covered =
				(!prim.id.empty() && svg_id_to_layer_name.find(prim.id) != svg_id_to_layer_name.end());
			if (!covered) {
				for (const std::string &anc_id : prim.ancestor_ids) {
					if (svg_id_to_layer_name.find(anc_id) != svg_id_to_layer_name.end()) {
						covered = true;
						break;
					}
				}
			}
			if (!covered) {
				const std::string desc = prim.id.empty()
								 ? ("<" + prim.tag + ">")
								 : ("<" + prim.tag + " id=\"" + prim.id + "\">");
				add_warning(
					diagnostics, "NXG-W001", "svg",
					"SVG visual element " + desc +
						" is not associated with any declared layer (implicit static background)");
				break;
			}
		}
	}

	// Track which layers are referenced (for NXG-W006)
	std::unordered_set<std::string> directly_used_layers;
	for (const auto &[lname, ldef] : manifest.layers) {
		if (ldef.mask.has_value()) {
			directly_used_layers.insert(*ldef.mask);
		}
	}

	// 6. Validate animations (NXG-E020..E025, NXG-E060)
	for (const std::string &clip_name : manifest.clip_declaration_order) {
		ClipDef &clip = manifest.animations.at(clip_name);
		if (clip.loop && !clip.duration.has_value()) {
			add_error(diagnostics, "NXG-E024", clip.json_path + ".loop",
				  "clip '" + clip_name + "' has loop: true without 'duration'");
		}

		if (clip.triggers.size() > kMaxTriggersPerClip) {
			add_error(diagnostics, "NXG-E060", clip.json_path + ".triggers",
				  "clip '" + clip_name + "' has " + std::to_string(clip.triggers.size()) +
					  " triggers, exceeding limit (" + std::to_string(kMaxTriggersPerClip) + ")");
		}

		for (auto &track : clip.tracks) {
			std::optional<PropertyValueKind> expected_kind;
			if (!track.layer.empty()) {
				auto l_it = manifest.layers.find(track.layer);
				if (l_it == manifest.layers.end()) {
					add_error(diagnostics, "NXG-E022", track.json_path + ".layer",
						  "track references non-existent layer '" + track.layer + "'");
				} else {
					directly_used_layers.insert(track.layer);
					if (!track.prop.empty()) {
						expected_kind = get_layer_property_kind(l_it->second.type, track.prop);
						if (!expected_kind.has_value()) {
							add_error(
								diagnostics, "NXG-E023", track.json_path + ".prop",
								"property '" + track.prop +
									"' does not exist or is incompatible with layer '" +
									track.layer + "'");
						}
					}
				}
			}

			if (track.has_keys && (track.from.has_value() || track.to.has_value())) {
				add_error(diagnostics, "NXG-E024", track.json_path,
					  "track cannot mix 'keys' with 'from'/'to'");
			} else if (!track.has_keys) {
				if (!track.to.has_value()) {
					add_error(diagnostics, "NXG-E024", track.json_path,
						  "track without 'keys' must specify 'to'");
				}
				if (!track.dur.has_value()) {
					add_error(diagnostics, "NXG-E024", track.json_path,
						  "track with 'from'/'to' requires 'dur'");
				}
				if (expected_kind.has_value()) {
					if (track.from.has_value() &&
					    !coerce_property_value(*track.from, *expected_kind)) {
						add_error(diagnostics, "NXG-E024", track.json_path + ".from",
							  "track 'from' value is incompatible with property '" +
								  track.prop + "'");
					}
					if (track.to.has_value() && !coerce_property_value(*track.to, *expected_kind)) {
						add_error(diagnostics, "NXG-E024", track.json_path + ".to",
							  "track 'to' value is incompatible with property '" +
								  track.prop + "'");
					}
				}
			} else {
				for (std::size_t k = 0; k < track.keys.size(); ++k) {
					if (track.keys[k].time < 0.0) {
						add_error(diagnostics, "NXG-E024", track.json_path + ".keys",
							  "keyframe times must be non-negative");
						break;
					}
					if (k > 0 && track.keys[k].time <= track.keys[k - 1].time) {
						add_error(diagnostics, "NXG-E024", track.json_path + ".keys",
							  "keyframe times in 'keys' must be strictly increasing");
						break;
					}
				}
				if (expected_kind.has_value()) {
					for (std::size_t k = 0; k < track.keys.size(); ++k) {
						if (!coerce_property_value(track.keys[k].value, *expected_kind)) {
							add_error(diagnostics, "NXG-E024",
								  track.json_path + ".keys[" + std::to_string(k) + "]",
								  "keyframe value is incompatible with property '" +
									  track.prop + "'");
						}
					}
				}
			}
		}

		for (const auto &trig : clip.triggers) {
			if (!trig.play.empty() && manifest.animations.find(trig.play) == manifest.animations.end()) {
				add_error(diagnostics, "NXG-E020", trig.json_path + ".play",
					  "trigger references non-existent clip '" + trig.play + "'");
			}
			if (trig.event_kind == TriggerEventKind::OnMarker) {
				if (clip.markers.find(trig.marker_name) == clip.markers.end()) {
					add_error(diagnostics, "NXG-E025", trig.json_path + ".on",
						  "marker '" + trig.marker_name + "' is not defined in clip '" +
							  clip_name + "'");
				}
			}
		}
	}

	validate_triggers_graph(manifest, diagnostics);

	// 7. Validate initialState and states (NXG-E022, NXG-E023, NXG-E033, NXG-E050)
	if (manifest.initial_state.has_value()) {
		if (manifest.states.find(*manifest.initial_state) == manifest.states.end()) {
			add_error(diagnostics, "NXG-E050", "initialState",
				  "initialState '" + *manifest.initial_state + "' does not exist in 'states'");
		}
	}

	for (const std::string &state_name : manifest.state_declaration_order) {
		StateDef &state_def = manifest.states.at(state_name);
		for (auto &entry : state_def.entries) {
			std::string_view layer_name;
			std::string_view prop_name;
			if (!split_layer_property_path(entry.path, layer_name, prop_name)) {
				add_error(diagnostics, "NXG-E030", entry.json_path,
					  "invalid state property path '" + entry.path + "'");
				continue;
			}
			const std::string lname(layer_name);
			auto l_it = manifest.layers.find(lname);
			if (l_it == manifest.layers.end()) {
				add_error(diagnostics, "NXG-E022", entry.json_path,
					  "state references non-existent layer '" + lname + "'");
				continue;
			}
			directly_used_layers.insert(lname);

			auto kind = get_layer_property_kind(l_it->second.type, prop_name);
			if (!kind.has_value()) {
				add_error(diagnostics, "NXG-E023", entry.json_path,
					  "property '" + std::string(prop_name) + "' is incompatible with layer '" +
						  lname + "'");
				continue;
			}
			if (!coerce_property_value(entry.value, *kind)) {
				add_error(diagnostics, "NXG-E033", entry.json_path,
					  "state value for '" + entry.path + "' is incompatible with property type");
			}
		}
	}

	// 8. Validate panel & controls (NXG-E030..E033, NXG-W003)
	std::unordered_set<std::string> seen_control_keys;
	if (manifest.panel.has_value()) {
		for (auto &sec : manifest.panel->sections) {
			for (auto &ctrl : sec.controls) {
				if (!ctrl.key.empty()) {
					if (!seen_control_keys.insert(ctrl.key).second) {
						add_error(diagnostics, "NXG-E031", ctrl.json_path + ".key",
							  "duplicate control key '" + ctrl.key + "'");
					}
				}

				const bool requires_bind =
					(ctrl.type == ControlType::Text || ctrl.type == ControlType::Number ||
					 ctrl.type == ControlType::Toggle || ctrl.type == ControlType::Color);
				if (requires_bind && ctrl.binds.empty()) {
					add_error(diagnostics, "NXG-E030", ctrl.json_path + ".bind",
						  "control requires 'bind'");
				}

				std::optional<PropertyValueKind> bound_kind;
				std::vector<std::string> bound_text_layers;

				for (const std::string &bind_str : ctrl.binds) {
					std::string_view layer_name;
					std::string_view prop_name;
					if (!split_layer_property_path(bind_str, layer_name, prop_name)) {
						add_error(diagnostics, "NXG-E030", ctrl.json_path + ".bind",
							  "invalid bind path '" + bind_str +
								  "' (expected 'layers.<layer>.<prop>')");
						continue;
					}
					const std::string lname(layer_name);
					auto l_it = manifest.layers.find(lname);
					if (l_it == manifest.layers.end()) {
						add_error(diagnostics, "NXG-E030", ctrl.json_path + ".bind",
							  "bind references non-existent layer '" + lname + "'");
						continue;
					}
					directly_used_layers.insert(lname);

					auto kind = get_layer_property_kind(l_it->second.type, prop_name);
					if (!kind.has_value()) {
						add_error(diagnostics, "NXG-E030", ctrl.json_path + ".bind",
							  "bind property '" + std::string(prop_name) +
								  "' is incompatible with layer '" + lname + "'");
						continue;
					}

					if (!is_control_type_compatible_with_property(ctrl.type, *kind)) {
						add_error(diagnostics, "NXG-E030", ctrl.json_path + ".bind",
							  "control type is incompatible with bound property '" +
								  bind_str + "'");
						continue;
					}

					if (bound_kind.has_value() && *bound_kind != *kind) {
						add_error(
							diagnostics, "NXG-E030", ctrl.json_path + ".bind",
							"all binds on a control must target properties of the same type");
					}
					bound_kind = *kind;

					if (l_it->second.type == LayerType::Text && prop_name == "text") {
						bound_text_layers.push_back(lname);
					}
				}

				// Validate default value for bound controls
				if (ctrl.type != ControlType::Dropdown && ctrl.default_value.has_value() &&
				    bound_kind.has_value()) {
					if (!coerce_property_value(*ctrl.default_value, *bound_kind)) {
						add_error(
							diagnostics, "NXG-E033", ctrl.json_path + ".default",
							"control 'default' value is incompatible with bound property");
					} else if (ctrl.type == ControlType::Number &&
						   std::holds_alternative<double>(*ctrl.default_value)) {
						const double d = std::get<double>(*ctrl.default_value);
						if ((ctrl.min.has_value() && d < *ctrl.min) ||
						    (ctrl.max.has_value() && d > *ctrl.max)) {
							add_error(
								diagnostics, "NXG-E033", ctrl.json_path + ".default",
								"number control 'default' is outside [min, max] range");
						}
					}
				}

				if (ctrl.type == ControlType::Dropdown) {
					bool default_matched = !ctrl.default_value.has_value();
					for (auto &opt : ctrl.options) {
						if (opt.apply.has_value()) {
							auto s_it = manifest.states.find(*opt.apply);
							if (s_it == manifest.states.end()) {
								add_error(
									diagnostics, "NXG-E032",
									opt.json_path + ".apply",
									"dropdown option references non-existent state '" +
										*opt.apply + "'");
							}
							if (ctrl.default_value.has_value()) {
								if (const auto *def_str = std::get_if<std::string>(
									    &*ctrl.default_value)) {
									if (*def_str == *opt.apply) {
										default_matched = true;
									}
								}
							}
						} else if (opt.value.has_value()) {
							if (ctrl.binds.empty()) {
								add_error(
									diagnostics, "NXG-E030", ctrl.json_path,
									"dropdown with 'value' options requires 'bind'");
							} else if (bound_kind.has_value()) {
								if (!coerce_property_value(*opt.value, *bound_kind)) {
									add_error(
										diagnostics, "NXG-E033",
										opt.json_path + ".value",
										"dropdown option 'value' is incompatible with bound property");
								} else if (ctrl.default_value.has_value()) {
									PropertyValue def_copy = *ctrl.default_value;
									if (coerce_property_value(def_copy,
												  *bound_kind) &&
									    def_copy == *opt.value) {
										default_matched = true;
									}
								}
							}
						}
					}
					if (!default_matched && !ctrl.options.empty()) {
						add_error(diagnostics, "NXG-E033", ctrl.json_path + ".default",
							  "dropdown 'default' does not match any option");
					}
				} else if (ctrl.type == ControlType::Button) {
					if (!ctrl.action.empty() &&
					    manifest.actions.find(ctrl.action) == manifest.actions.end()) {
						add_error(diagnostics, "NXG-E032", ctrl.json_path + ".action",
							  "button references non-existent action '" + ctrl.action +
								  "'");
					}
				} else if (ctrl.type == ControlType::Text) {
					if (ctrl.submit_action.has_value() &&
					    manifest.actions.find(*ctrl.submit_action) == manifest.actions.end()) {
						add_error(
							diagnostics, "NXG-E032", ctrl.json_path + ".submitAction",
							"text control 'submitAction' references non-existent action '" +
								*ctrl.submit_action + "'");
					}

					// Check NXG-W003 for maxLength or default text against SVG frame width
					if (out_svg_info.has_value()) {
						for (const std::string &tl_name : bound_text_layers) {
							const LayerDef &tl = manifest.layers.at(tl_name);
							auto elem_it = out_svg_info->elements_by_id.find(tl.id);
							if (elem_it == out_svg_info->elements_by_id.end() ||
							    !elem_it->second.width.has_value()) {
								continue;
							}
							const double frame_w = *elem_it->second.width;
							const double eff_size = (tl.overflow == TextOverflow::Shrink)
											? tl.min_size
											: tl.size;
							const int lines = (tl.overflow == TextOverflow::Wrap)
										  ? std::max(1, tl.max_lines)
										  : 1;
							const double capacity_px = frame_w * static_cast<double>(lines);
							const double char_w = eff_size * 0.4;

							if (ctrl.max_length.has_value() &&
							    static_cast<double>(*ctrl.max_length) * char_w >
								    capacity_px) {
								add_warning(
									diagnostics, "NXG-W003",
									ctrl.json_path + ".maxLength",
									"control 'maxLength' (" +
										std::to_string(*ctrl.max_length) +
										") likely exceeds text frame width of layer '" +
										tl_name + "'");
							}
							if (ctrl.default_value.has_value()) {
								if (const auto *s = std::get_if<std::string>(
									    &*ctrl.default_value)) {
									if (!s->empty() &&
									    static_cast<double>(s->size()) * char_w >
										    capacity_px) {
										add_warning(
											diagnostics, "NXG-W003",
											ctrl.json_path + ".default",
											"default text likely exceeds text frame width of layer '" +
												tl_name + "'");
									}
								}
							}
						}
					}
				}
			}
		}
	}

	// 9. Validate actions (NXG-E040, NXG-E041)
	for (const std::string &act_name : manifest.action_declaration_order) {
		const auto &steps = manifest.actions.at(act_name);
		for (const auto &step : steps) {
			if (step.target.empty()) {
				continue;
			}
			switch (step.type) {
			case ActionStepType::Play:
			case ActionStepType::Stop:
				if (manifest.animations.find(step.target) == manifest.animations.end()) {
					add_error(diagnostics, "NXG-E040", step.json_path,
						  "action step references non-existent clip '" + step.target + "'");
				}
				break;
			case ActionStepType::Set:
				if (manifest.states.find(step.target) == manifest.states.end()) {
					add_error(diagnostics, "NXG-E040", step.json_path,
						  "action step references non-existent state '" + step.target + "'");
				}
				break;
			case ActionStepType::Call:
				if (manifest.actions.find(step.target) == manifest.actions.end()) {
					add_error(diagnostics, "NXG-E040", step.json_path,
						  "action step references non-existent action '" + step.target + "'");
				}
				break;
			}
		}
	}

	validate_actions_graph(manifest, diagnostics);

	// 10. Post-process warnings on layers: NXG-W003 (layer default text) and NXG-W006 (unused layer)
	for (const std::string &layer_name : manifest.layer_declaration_order) {
		const LayerDef &layer = manifest.layers.at(layer_name);

		if (layer.type == LayerType::Text && !layer.text.empty() && out_svg_info.has_value()) {
			auto elem_it = out_svg_info->elements_by_id.find(layer.id);
			if (elem_it != out_svg_info->elements_by_id.end() && elem_it->second.width.has_value()) {
				const double frame_w = *elem_it->second.width;
				const double eff_size = (layer.overflow == TextOverflow::Shrink) ? layer.min_size
												 : layer.size;
				const int lines = (layer.overflow == TextOverflow::Wrap) ? std::max(1, layer.max_lines)
											 : 1;
				const double capacity_px = frame_w * static_cast<double>(lines);
				if (static_cast<double>(layer.text.size()) * eff_size * 0.4 > capacity_px) {
					add_warning(diagnostics, "NXG-W003", layer.json_path + ".text",
						    "layer default 'text' likely exceeds SVG frame width");
				}
			}
		}

		bool used = directly_used_layers.count(layer_name) > 0;
		if (!used && out_svg_info.has_value() && !layer.id.empty()) {
			auto elem_it = out_svg_info->elements_by_id.find(layer.id);
			if (elem_it != out_svg_info->elements_by_id.end()) {
				for (const std::string &anc_id : elem_it->second.ancestor_ids) {
					auto anc_layer_it = svg_id_to_layer_name.find(anc_id);
					if (anc_layer_it != svg_id_to_layer_name.end() &&
					    directly_used_layers.count(anc_layer_it->second) > 0) {
						used = true;
						break;
					}
				}
			}
		}

		if (!used) {
			add_warning(diagnostics, "NXG-W006", layer.json_path,
				    "declared layer '" + layer_name +
					    "' is never used in animations, controls, states, or masks");
		}
	}
}

TemplateLoadResult load_template_package(const std::filesystem::path &package_root)
{
	TemplateLoadResult result;

	std::error_code ec;
	if (package_root.empty() || !std::filesystem::exists(package_root, ec) ||
	    !std::filesystem::is_directory(package_root, ec) || ec) {
		result.diagnostics.push_back(
			Diagnostic{DiagnosticSeverity::Error, "NXG-E003", "", "template directory does not exist"});
		return result;
	}

	auto manifest_file = read_package_file_bounded(package_root, "manifest.json", kMaxManifestBytes, "NXG-E001");
	if (manifest_file.is_err()) {
		result.diagnostics.push_back(Diagnostic{DiagnosticSeverity::Error, manifest_file.error().code, "",
							manifest_file.error().message});
		return result;
	}

	auto parsed_manifest = parse_manifest_json(manifest_file.value(), result.diagnostics);
	if (!parsed_manifest.has_value()) {
		return result;
	}

	std::optional<SvgInspectionResult> svg_info;
	std::string svg_content;
	validate_manifest_package(*parsed_manifest, package_root, svg_info, svg_content, result.diagnostics);

	bool has_errors = false;
	for (const auto &d : result.diagnostics) {
		if (d.severity == DiagnosticSeverity::Error) {
			has_errors = true;
			break;
		}
	}

	if (!has_errors && svg_info.has_value()) {
		std::vector<LoadedFontData> loaded_fonts;
		for (const auto &font : parsed_manifest->fonts) {
			if (font.file.empty()) {
				continue;
			}
			auto font_bytes = read_package_file_bounded(package_root, font.file, kMaxFontBytes, "NXG-E003");
			if (font_bytes.is_ok()) {
				loaded_fonts.push_back(LoadedFontData{
					font.family,
					font.file,
					std::move(font_bytes.value()),
				});
			}
		}

		result.loaded = LoadedTemplate{
			std::move(*parsed_manifest),
			std::move(svg_content),
			std::move(*svg_info),
			std::move(loaded_fonts),
		};
	}

	return result;
}

} // namespace naxgc
