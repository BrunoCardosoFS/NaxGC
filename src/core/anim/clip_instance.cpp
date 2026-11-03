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

#include "core/anim/clip_instance.hpp"

#include <algorithm>

namespace naxgc {

ClipInstance::ClipInstance(uint64_t id, const ClipDef &def, double start_time, double speed, TriggerMode mode,
			   std::optional<uint64_t> parent_id)
	: id_(id),
	  clip_def_(def),
	  local_time_(start_time),
	  speed_(speed),
	  mode_(mode),
	  parent_id_(parent_id)
{
	track_states_.reserve(clip_def_.tracks.size());
	for (std::size_t i = 0; i < clip_def_.tracks.size(); ++i) {
		track_states_.push_back(ActiveTrackState::init(i, clip_def_.tracks[i]));
	}

	fired_triggers_.assign(clip_def_.triggers.size(), false);

	if (clip_def_.duration.has_value()) {
		own_duration_ = *clip_def_.duration;
	} else {
		double max_dur = 0.0;
		for (const auto &ts : track_states_) {
			max_dur = std::max(max_dur, ts.start + ts.duration);
		}
		own_duration_ = max_dur;
	}

	if (!clip_def_.loop && local_time_ >= own_duration_) {
		own_tracks_completed_ = true;
	}
}

void ClipInstance::remove_child(uint64_t child_id)
{
	auto it = std::find(children_ids_.begin(), children_ids_.end(), child_id);
	if (it != children_ids_.end()) {
		children_ids_.erase(it);
	}
}

std::vector<TriggerSpawnRequest> ClipInstance::check_triggers(double t_old, double t_new)
{
	std::vector<TriggerSpawnRequest> spawns;

	for (std::size_t i = 0; i < clip_def_.triggers.size(); ++i) {
		if (fired_triggers_[i]) {
			continue;
		}

		const auto &trig = clip_def_.triggers[i];
		double t_fire = 0.0;

		switch (trig.event_kind) {
		case TriggerEventKind::AtTime:
			t_fire = trig.at + trig.delay;
			break;
		case TriggerEventKind::OnMarker: {
			auto m_it = clip_def_.markers.find(trig.marker_name);
			if (m_it != clip_def_.markers.end()) {
				t_fire = m_it->second + trig.delay;
			} else {
				t_fire = trig.delay;
			}
			break;
		}
		case TriggerEventKind::OnEnd:
			t_fire = own_duration_ + trig.delay;
			break;
		}

		const bool fires = (t_old < 0.0) ? (t_fire <= t_new) : (t_fire > t_old && t_fire <= t_new);

		if (fires) {
			fired_triggers_[i] = true;
			const double dt_residual = std::max(0.0, t_new - t_fire);
			const double child_speed = trig.inherit_speed ? speed_ : trig.speed;

			spawns.push_back(TriggerSpawnRequest{
				trig.play,
				dt_residual,
				child_speed,
				trig.mode,
				id_,
			});
		}
	}

	return spawns;
}

std::vector<TriggerSpawnRequest> ClipInstance::check_initial_triggers()
{
	return check_triggers(-1.0, local_time_);
}

std::vector<TriggerSpawnRequest> ClipInstance::advance(double dt)
{
	if (is_stopped_) {
		return {};
	}

	const double t_old = local_time_;
	local_time_ += dt * speed_;
	const double t_new = local_time_;

	std::vector<TriggerSpawnRequest> spawns = check_triggers(t_old, t_new);

	if (clip_def_.loop && clip_def_.duration.has_value() && *clip_def_.duration > 0.0) {
		const double dur = *clip_def_.duration;
		if (local_time_ >= dur) {
			local_time_ = std::fmod(local_time_, dur);
			// Reset triggers so they can fire in future loops
			fired_triggers_.assign(clip_def_.triggers.size(), false);
		}
	} else {
		if (local_time_ >= own_duration_) {
			own_tracks_completed_ = true;
		}
	}

	return spawns;
}

void ClipInstance::evaluate_tracks(const Scene &scene)
{
	for (std::size_t i = 0; i < track_states_.size(); ++i) {
		track_states_[i].evaluate(clip_def_.tracks[i], local_time_, scene);
	}
}

} // namespace naxgc
