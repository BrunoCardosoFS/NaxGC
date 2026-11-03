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

#include <vector>

#include "core/manifest/manifest_types.hpp"

namespace naxgc {

struct ReadyActionStep {
	ActionStepDef step;
	double residual_time{0.0};
};

struct DelayedActionStep {
	ActionStepDef step;
	double remaining_time{0.0};
};

class ActionScheduler {
public:
	ActionScheduler() = default;

	void schedule(const ActionStepDef &step);

	/**
	 * Advances pending delayed steps by dt and returns steps that have become due,
	 * along with their residual overshoot time.
	 */
	std::vector<ReadyActionStep> advance(double dt);

	void clear() noexcept { pending_steps_.clear(); }

	bool has_pending() const noexcept { return !pending_steps_.empty(); }

private:
	std::vector<DelayedActionStep> pending_steps_;
};

} // namespace naxgc
