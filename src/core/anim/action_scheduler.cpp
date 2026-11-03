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

#include "core/anim/action_scheduler.hpp"

#include <algorithm>

namespace naxgc {

void ActionScheduler::schedule(const ActionStepDef &step)
{
	pending_steps_.push_back(DelayedActionStep{step, step.delay});
}

std::vector<ReadyActionStep> ActionScheduler::advance(double dt)
{
	std::vector<ReadyActionStep> ready;
	std::vector<DelayedActionStep> remaining;
	remaining.reserve(pending_steps_.size());

	for (auto &ds : pending_steps_) {
		ds.remaining_time -= dt;
		if (ds.remaining_time <= 0.0) {
			ready.push_back(ReadyActionStep{ds.step, -ds.remaining_time});
		} else {
			remaining.push_back(ds);
		}
	}

	pending_steps_ = std::move(remaining);
	return ready;
}

} // namespace naxgc
