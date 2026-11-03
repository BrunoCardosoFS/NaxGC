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

#include "core/anim/animator.hpp"

#include <algorithm>

#include "core/anim/interpolator.hpp"

namespace naxgc {

namespace {

std::string make_prop_key(std::string_view layer, std::string_view prop)
{
	std::string key;
	key.reserve(layer.size() + 1 + prop.size());
	key.append(layer);
	key.push_back(':');
	key.append(prop);
	return key;
}

std::pair<std::string_view, std::string_view> split_prop_key(std::string_view key)
{
	const auto pos = key.find(':');
	if (pos == std::string_view::npos) {
		return {key, {}};
	}
	return {key.substr(0, pos), key.substr(pos + 1)};
}

} // namespace

void Animator::set_target(const Manifest *manifest, Scene *scene, bool apply_initial_state)
{
	reset();
	manifest_ = manifest;
	scene_ = scene;

	if (apply_initial_state && manifest_ && scene_ && manifest_->initial_state.has_value()) {
		scene_->apply_state(*manifest_->initial_state);
		scene_->evaluate_transforms();
	}
}

void Animator::reset()
{
	action_scheduler_.clear();

	if (scene_) {
		for (const auto &key : previously_animated_props_) {
			const auto [layer, prop] = split_prop_key(key);
			scene_->clear_layer_property_animated(layer, prop);
		}
		scene_->evaluate_transforms();
	}

	previously_animated_props_.clear();
	active_instances_.clear();
}

Result<void> Animator::play(std::string_view clip_name, double start_offset, double speed, TriggerMode mode,
			    std::optional<uint64_t> parent_id)
{
	if (!manifest_ || !scene_) {
		return Result<void>::err(Error{"NXG-R004", "no active template loaded"});
	}

	const auto it = manifest_->animations.find(std::string(clip_name));
	if (it == manifest_->animations.end()) {
		return Result<void>::err(Error{"NXG-R003", "clip '" + std::string(clip_name) + "' does not exist"});
	}

	const ClipDef &clip_def = it->second;

	// Check retrigger policy
	if (clip_def.retrigger == RetriggerPolicy::Ignore) {
		if (is_clip_active(clip_name)) {
			return Result<void>::ok();
		}
	} else if (clip_def.retrigger == RetriggerPolicy::Restart) {
		// Stop any existing instances of this clip
		std::vector<uint64_t> to_stop;
		for (const auto &inst : active_instances_) {
			if (inst->clip_name() == clip_name) {
				to_stop.push_back(inst->id());
			}
		}
		for (uint64_t sid : to_stop) {
			stop_instance_recursive(sid);
		}
	}

	const uint64_t inst_id = ++next_instance_id_;
	auto inst = std::make_unique<ClipInstance>(inst_id, clip_def, start_offset, speed, mode, parent_id);

	if (parent_id.has_value()) {
		for (auto &existing : active_instances_) {
			if (existing->id() == *parent_id) {
				existing->add_child(inst_id);
				break;
			}
		}
	}

	auto initial_spawns = inst->check_initial_triggers();
	active_instances_.push_back(std::move(inst));

	if (!initial_spawns.empty()) {
		spawn_trigger_children(std::move(initial_spawns), 1);
	}

	return Result<void>::ok();
}

void Animator::stop_instance_recursive(uint64_t instance_id)
{
	for (auto &inst : active_instances_) {
		if (inst->id() == instance_id) {
			inst->mark_stopped();
			// Stop linked children recursively
			for (uint64_t child_id : inst->children_ids()) {
				for (const auto &cand : active_instances_) {
					if (cand->id() == child_id && cand->mode() == TriggerMode::Linked) {
						stop_instance_recursive(child_id);
						break;
					}
				}
			}
			break;
		}
	}
}

Result<void> Animator::stop(std::string_view clip_name)
{
	if (!manifest_) {
		return Result<void>::err(Error{"NXG-R004", "no active template loaded"});
	}

	const auto it = manifest_->animations.find(std::string(clip_name));
	if (it == manifest_->animations.end()) {
		return Result<void>::err(Error{"NXG-R003", "clip '" + std::string(clip_name) + "' does not exist"});
	}

	std::vector<uint64_t> to_stop;
	for (const auto &inst : active_instances_) {
		if (inst->clip_name() == clip_name) {
			to_stop.push_back(inst->id());
		}
	}

	for (uint64_t id : to_stop) {
		stop_instance_recursive(id);
	}

	return Result<void>::ok();
}

Result<void> Animator::run_action(std::string_view action_name)
{
	if (!manifest_ || !scene_) {
		return Result<void>::err(Error{"NXG-R004", "no active template loaded"});
	}

	const auto it = manifest_->actions.find(std::string(action_name));
	if (it == manifest_->actions.end()) {
		return Result<void>::err(Error{"NXG-R003", "action '" + std::string(action_name) + "' does not exist"});
	}

	for (const auto &step : it->second) {
		if (step.delay > 0.0) {
			action_scheduler_.schedule(step);
		} else {
			execute_action_step(step, 0.0);
		}
	}

	return Result<void>::ok();
}

void Animator::execute_action_step(const ActionStepDef &step, double residual_time)
{
	switch (step.type) {
	case ActionStepType::Play:
		play(step.target, residual_time);
		break;
	case ActionStepType::Stop:
		stop(step.target);
		break;
	case ActionStepType::Set:
		apply_state(step.target);
		break;
	case ActionStepType::Call:
		run_action(step.target);
		break;
	}
}

Result<void> Animator::apply_state(std::string_view state_name)
{
	if (!scene_) {
		return Result<void>::err(Error{"NXG-R004", "no active template loaded"});
	}
	const auto res = scene_->apply_state(state_name);
	if (res.is_ok()) {
		scene_->evaluate_transforms();
	}
	return res;
}

bool Animator::are_linked_children_finished(const ClipInstance &inst,
					    const std::unordered_set<uint64_t> &finishing_ids) const
{
	for (uint64_t child_id : inst.children_ids()) {
		if (finishing_ids.find(child_id) != finishing_ids.end()) {
			continue;
		}
		for (const auto &cand : active_instances_) {
			if (cand->id() == child_id && cand->mode() == TriggerMode::Linked) {
				return false;
			}
		}
	}
	return true;
}

void Animator::spawn_trigger_children(std::vector<TriggerSpawnRequest> requests, std::size_t depth)
{
	if (depth > kMaxTriggerDepth || requests.empty() || !manifest_) {
		return;
	}

	std::vector<TriggerSpawnRequest> next_level_requests;

	for (const auto &req : requests) {
		const auto it = manifest_->animations.find(req.clip_name);
		if (it == manifest_->animations.end()) {
			continue;
		}

		if (it->second.retrigger == RetriggerPolicy::Restart) {
			std::vector<uint64_t> to_stop;
			for (const auto &inst : active_instances_) {
				if (inst->clip_name() == req.clip_name) {
					to_stop.push_back(inst->id());
				}
			}
			for (uint64_t sid : to_stop) {
				stop_instance_recursive(sid);
			}
		} else if (it->second.retrigger == RetriggerPolicy::Ignore) {
			if (is_clip_active(req.clip_name)) {
				continue;
			}
		}

		const uint64_t inst_id = ++next_instance_id_;
		auto child_inst = std::make_unique<ClipInstance>(inst_id, it->second, req.residual_time * req.speed,
								 req.speed, req.mode, req.parent_id);

		if (req.parent_id != 0) {
			for (auto &parent_inst : active_instances_) {
				if (parent_inst->id() == req.parent_id) {
					parent_inst->add_child(inst_id);
					break;
				}
			}
		}

		auto child_triggers = child_inst->check_initial_triggers();
		active_instances_.push_back(std::move(child_inst));

		next_level_requests.insert(next_level_requests.end(), child_triggers.begin(), child_triggers.end());
	}

	if (!next_level_requests.empty()) {
		spawn_trigger_children(std::move(next_level_requests), depth + 1);
	}
}

void Animator::tick(double seconds)
{
	if (!manifest_ || !scene_) {
		return;
	}

	const double dt = max_dt_.has_value() ? std::clamp(seconds, 0.0, *max_dt_) : std::max(0.0, seconds);

	// 1. Advance delayed action steps
	auto ready_steps = action_scheduler_.advance(dt);
	for (const auto &ready : ready_steps) {
		execute_action_step(ready.step, ready.residual_time);
	}

	// 2. Advance active clip instances and collect trigger requests
	std::vector<TriggerSpawnRequest> all_spawns;
	for (auto &inst : active_instances_) {
		auto spawns = inst->advance(dt);
		all_spawns.insert(all_spawns.end(), spawns.begin(), spawns.end());
	}

	if (!all_spawns.empty()) {
		spawn_trigger_children(std::move(all_spawns), 1);
	}

	// 3. Evaluate tracks across all active instances
	for (auto &inst : active_instances_) {
		if (!inst->is_stopped()) {
			inst->evaluate_tracks(*scene_);
		}
	}

	// Conflict resolution & property writing
	struct MergedTrackValue {
		std::string layer;
		std::string prop;
		PropertyValue value;
	};

	std::unordered_map<std::string, MergedTrackValue> evaluated_props;
	std::unordered_set<std::string> currently_animated_props;

	for (const auto &inst : active_instances_) {
		if (inst->is_stopped()) {
			continue;
		}

		for (const auto &ts : inst->track_states()) {
			if (!ts.is_active || !ts.last_evaluated_value.has_value()) {
				continue;
			}

			const std::string key = make_prop_key(ts.layer, ts.prop);
			currently_animated_props.insert(key);

			auto it = evaluated_props.find(key);
			if (it == evaluated_props.end()) {
				evaluated_props.emplace(key,
							MergedTrackValue{ts.layer, ts.prop, *ts.last_evaluated_value});
			} else {
				it->second.value =
					combine_blended_value(it->second.value, *ts.last_evaluated_value, ts.blend);
			}
		}
	}

	// Apply updated animated values to scene
	for (const auto &[key, entry] : evaluated_props) {
		scene_->set_layer_property_animated(entry.layer, entry.prop, entry.value);
	}

	// Clear properties that are no longer animated
	for (const auto &prev_key : previously_animated_props_) {
		if (currently_animated_props.find(prev_key) == currently_animated_props.end()) {
			const auto [layer, prop] = split_prop_key(prev_key);
			scene_->clear_layer_property_animated(layer, prop);
		}
	}
	previously_animated_props_ = std::move(currently_animated_props);

	// 4. Handle finished or stopped instances
	std::unordered_set<uint64_t> finished_set;
	std::vector<uint64_t> finished_ids;

	bool progress = true;
	while (progress) {
		progress = false;
		for (const auto &inst : active_instances_) {
			if (finished_set.find(inst->id()) != finished_set.end()) {
				continue;
			}
			if (inst->is_stopped() || (!inst->clip_def().loop && inst->own_tracks_completed() &&
						   are_linked_children_finished(*inst, finished_set))) {
				finished_set.insert(inst->id());
				finished_ids.push_back(inst->id());
				progress = true;
			}
		}
	}

	for (uint64_t fid : finished_ids) {
		for (const auto &inst : active_instances_) {
			if (inst->id() != fid) {
				continue;
			}
			if (!inst->is_stopped() && !inst->clip_def().loop && inst->own_tracks_completed()) {
				if (inst->clip_def().on_end == OnEndPolicy::Hold) {
					for (const auto &ts : inst->track_states()) {
						if (ts.last_evaluated_value.has_value()) {
							scene_->set_layer_property_base(ts.layer, ts.prop,
											*ts.last_evaluated_value);
							scene_->clear_layer_property_animated(ts.layer, ts.prop);
						}
					}
				} else if (inst->clip_def().on_end == OnEndPolicy::Revert) {
					for (const auto &ts : inst->track_states()) {
						if (ts.initial_base_value.has_value()) {
							scene_->set_layer_property_base(ts.layer, ts.prop,
											*ts.initial_base_value);
							scene_->clear_layer_property_animated(ts.layer, ts.prop);
						}
					}
				}
			}
			break;
		}
	}

	// Remove finished instances and detach from parents
	if (!finished_ids.empty()) {
		for (uint64_t fid : finished_ids) {
			for (auto &parent_cand : active_instances_) {
				parent_cand->remove_child(fid);
			}
		}

		active_instances_.erase(std::remove_if(active_instances_.begin(), active_instances_.end(),
						       [&finished_ids](const std::unique_ptr<ClipInstance> &cand) {
							       return std::find(finished_ids.begin(),
										finished_ids.end(),
										cand->id()) != finished_ids.end();
						       }),
					active_instances_.end());
	}

	// 5. Evaluate world matrices and visibility transforms
	scene_->evaluate_transforms();
}

std::vector<std::string> Animator::active_clip_names() const
{
	std::vector<std::string> names;
	names.reserve(active_instances_.size());
	for (const auto &inst : active_instances_) {
		if (!inst->is_stopped()) {
			names.push_back(inst->clip_name());
		}
	}
	return names;
}

bool Animator::is_clip_active(std::string_view clip_name) const
{
	for (const auto &inst : active_instances_) {
		if (!inst->is_stopped() && inst->clip_name() == clip_name) {
			return true;
		}
	}
	return false;
}

} // namespace naxgc
