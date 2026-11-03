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

#include "core/manifest/manifest_types.hpp"
#include "core/scene/scene.hpp"

namespace naxgc {

struct ActiveTrackState {
	std::size_t track_index{0};
	std::string layer;
	std::string prop;
	BlendMode blend{BlendMode::Override};
	double start{0.0};
	double duration{0.0};

	std::optional<PropertyValue> captured_from;
	std::optional<PropertyValue> initial_base_value;
	std::optional<PropertyValue> last_evaluated_value;

	bool is_active{false};
	bool is_completed{false};

	static ActiveTrackState init(std::size_t index, const TrackDef &def) noexcept;

	/**
	 * Evaluates the track at local_time within the clip.
	 * If local_time < start, returns std::nullopt.
	 * Captures current scene effective value as `from` on first activation if `from` was omitted in manifest.
	 */
	std::optional<PropertyValue> evaluate(const TrackDef &def, double local_time, const Scene &scene);
};

} // namespace naxgc
