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
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/anim/action_scheduler.hpp"
#include "core/anim/clip_instance.hpp"
#include "core/manifest/manifest_types.hpp"
#include "core/scene/scene.hpp"
#include "core/util/result.hpp"

namespace naxgc {

constexpr double kDefaultMaxDt = 0.1; // 100 ms

class Animator {
public:
	Animator() = default;

	/**
	 * Binds the Animator to a manifest and Scene.
	 * If apply_initial_state is true and manifest has initial_state, applies it to scene base values.
	 */
	void set_target(const Manifest *manifest, Scene *scene, bool apply_initial_state = true);

	/**
	 * Clears all active animations and scheduled actions.
	 */
	void reset();

	bool has_target() const noexcept { return manifest_ != nullptr && scene_ != nullptr; }

	/**
	 * Starts playing a named clip.
	 * Returns NXG-R003 if the clip does not exist.
	 */
	Result<void> play(std::string_view clip_name, double start_offset = 0.0, double speed = 1.0,
			  TriggerMode mode = TriggerMode::Detached, std::optional<uint64_t> parent_id = std::nullopt);

	/**
	 * Stops playing a named clip and cancels its linked descendants.
	 * Returns NXG-R003 if the clip does not exist in the manifest.
	 */
	Result<void> stop(std::string_view clip_name);

	/**
	 * Executes a named action sequence.
	 * Returns NXG-R003 if the action does not exist.
	 */
	Result<void> run_action(std::string_view action_name);

	/**
	 * Applies a named state from the manifest to the Scene.
	 */
	Result<void> apply_state(std::string_view state_name);

	/**
	 * Advances animation state by seconds, evaluates tracks, and updates Scene properties and transforms.
	 */
	void tick(double seconds);

	/**
	 * Returns list of currently active clip names.
	 */
	std::vector<std::string> active_clip_names() const;

	/**
	 * Checks if any instance of a named clip is currently running.
	 */
	bool is_clip_active(std::string_view clip_name) const;

	std::size_t active_instance_count() const noexcept { return active_instances_.size(); }

	void set_max_dt(std::optional<double> max_dt) noexcept { max_dt_ = max_dt; }
	std::optional<double> max_dt() const noexcept { return max_dt_; }

private:
	const Manifest *manifest_{nullptr};
	Scene *scene_{nullptr};
	ActionScheduler action_scheduler_;
	std::optional<double> max_dt_{kDefaultMaxDt};

	uint64_t next_instance_id_{0};
	std::vector<std::unique_ptr<ClipInstance>> active_instances_;

	// Set of property paths ("layer:prop") that had animated overrides in the previous tick
	std::unordered_set<std::string> previously_animated_props_;

	void execute_action_step(const ActionStepDef &step, double residual_time);
	void stop_instance_recursive(uint64_t instance_id);
	bool are_linked_children_finished(const ClipInstance &inst,
					  const std::unordered_set<uint64_t> &finishing_ids) const;
	void spawn_trigger_children(std::vector<TriggerSpawnRequest> requests, std::size_t depth);
};

} // namespace naxgc
