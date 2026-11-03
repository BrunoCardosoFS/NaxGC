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

#include "core/manifest/manifest_types.hpp"

namespace naxgc {

/**
 * Evaluates an easing curve specified by EasingSpec for progress u in [0, 1].
 * Values of u outside [0, 1] are clamped to [0, 1].
 */
double evaluate_easing(const EasingSpec &spec, double u) noexcept;

/**
 * Evaluates a named EasingType for progress u in [0, 1].
 */
double evaluate_easing_type(EasingType type, double u) noexcept;

/**
 * Solves and evaluates a 1D cubic-bezier curve (x1, y1, x2, y2) for input progress u.
 */
double evaluate_cubic_bezier(double x1, double y1, double x2, double y2, double u) noexcept;

} // namespace naxgc
