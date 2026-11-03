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

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/anim/track_evaluator.hpp"
#include "core/manifest/manifest_types.hpp"
#include "core/scene/scene.hpp"

namespace naxgc {

struct TriggerSpawnRequest {
	std::string clip_name;
	double residual_time{0.0};
	double speed{1.0};
	TriggerMode mode{TriggerMode::Linked};
	uint64_t parent_id{0};
};

class ClipInstance {
public:
	ClipInstance(uint64_t id, const ClipDef &def, double start_time = 0.0, double speed = 1.0,
		     TriggerMode mode = TriggerMode::Detached, std::optional<uint64_t> parent_id = std::nullopt);

	uint64_t id() const noexcept { return id_; }
	const std::string &clip_name() const noexcept { return clip_def_.name; }
	const ClipDef &clip_def() const noexcept { return clip_def_; }

	double local_time() const noexcept { return local_time_; }
	double speed() const noexcept { return speed_; }
	TriggerMode mode() const noexcept { return mode_; }

	std::optional<uint64_t> parent_id() const noexcept { return parent_id_; }
	const std::vector<uint64_t> &children_ids() const noexcept { return children_ids_; }
	void add_child(uint64_t child_id) { children_ids_.push_back(child_id); }
	void remove_child(uint64_t child_id);

	double own_duration() const noexcept { return own_duration_; }
	bool own_tracks_completed() const noexcept { return own_tracks_completed_; }
	bool is_stopped() const noexcept { return is_stopped_; }
	void mark_stopped() noexcept { is_stopped_ = true; }

	const std::vector<ActiveTrackState> &track_states() const noexcept { return track_states_; }
	std::vector<ActiveTrackState> &track_states() noexcept { return track_states_; }

	/**
	 * Advances local time by dt * speed, detects any triggers fired within [t_old, t_new],
	 * and returns the list of child clips to spawn with their residual time.
	 */
	std::vector<TriggerSpawnRequest> advance(double dt);

	/**
	 * Evaluates all tracks at the current local time against the Scene.
	 */
	void evaluate_tracks(const Scene &scene);

	/**
	 * Checks triggers at initial spawn time (e.g. at = 0 triggers or residual starting time).
	 */
	std::vector<TriggerSpawnRequest> check_initial_triggers();

private:
	uint64_t id_{0};
	ClipDef clip_def_;
	double local_time_{0.0};
	double speed_{1.0};
	TriggerMode mode_{TriggerMode::Detached};
	std::optional<uint64_t> parent_id_;
	std::vector<uint64_t> children_ids_;

	std::vector<ActiveTrackState> track_states_;
	std::vector<bool> fired_triggers_;
	double own_duration_{0.0};
	bool own_tracks_completed_{false};
	bool is_stopped_{false};

	std::vector<TriggerSpawnRequest> check_triggers(double t_old, double t_new);
};

} // namespace naxgc
