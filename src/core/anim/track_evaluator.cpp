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

#include "core/anim/track_evaluator.hpp"

#include <algorithm>
#include <cmath>

#include "core/anim/easing.hpp"
#include "core/anim/interpolator.hpp"

namespace naxgc {

ActiveTrackState ActiveTrackState::init(std::size_t index, const TrackDef &def) noexcept
{
	ActiveTrackState state;
	state.track_index = index;
	state.layer = def.layer;
	state.prop = def.prop;
	state.blend = def.blend;
	state.start = def.start;

	if (def.has_keys) {
		state.duration = def.keys.empty() ? 0.0 : def.keys.back().time;
	} else {
		state.duration = def.dur.value_or(0.0);
	}

	return state;
}

std::optional<PropertyValue> ActiveTrackState::evaluate(const TrackDef &def, double local_time, const Scene &scene)
{
	if (local_time < def.start) {
		is_active = false;
		return std::nullopt;
	}

	is_active = true;
	const double t_rel = local_time - def.start;
	is_completed = (t_rel >= duration);

	// On first activation at or after start time, capture initial values
	if (!captured_from.has_value()) {
		if (def.from.has_value()) {
			captured_from = *def.from;
		} else {
			const auto eff_res = scene.get_layer_property_effective(def.layer, def.prop);
			if (eff_res.is_ok()) {
				captured_from = eff_res.value();
			} else {
				captured_from = 0.0;
			}
		}
	}

	if (!initial_base_value.has_value()) {
		const auto eff_res = scene.get_layer_property_effective(def.layer, def.prop);
		if (eff_res.is_ok()) {
			initial_base_value = eff_res.value();
		}
	}

	PropertyValue result_val;

	if (def.has_keys) {
		if (def.keys.empty()) {
			return std::nullopt;
		}

		if (t_rel <= def.keys.front().time) {
			result_val = def.keys.front().value;
		} else if (t_rel >= def.keys.back().time) {
			result_val = def.keys.back().value;
		} else {
			// Find neighboring keyframes
			for (std::size_t i = 0; i + 1 < def.keys.size(); ++i) {
				const auto &k0 = def.keys[i];
				const auto &k1 = def.keys[i + 1];
				if (t_rel >= k0.time && t_rel <= k1.time) {
					const double dt_seg = k1.time - k0.time;
					if (dt_seg <= 1e-7) {
						result_val = k1.value;
					} else {
						const double u_seg = (t_rel - k0.time) / dt_seg;
						const double u_eased = evaluate_easing(def.ease, u_seg);
						result_val = interpolate_value(k0.value, k1.value, u_eased);
					}
					break;
				}
			}
		}
	} else {
		// from / to
		const double dur = (duration > 0.0) ? duration : 0.0;
		const double u = (dur <= 0.0) ? 1.0 : std::clamp(t_rel / dur, 0.0, 1.0);
		const double u_eased = evaluate_easing(def.ease, u);
		const PropertyValue &from_val = *captured_from;
		const PropertyValue &to_val = def.to.value_or(from_val);
		result_val = interpolate_value(from_val, to_val, u_eased);
	}

	last_evaluated_value = result_val;
	return result_val;
}

} // namespace naxgc
