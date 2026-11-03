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
#include "core/util/color.hpp"

namespace naxgc {

double lerp_double(double a, double b, double u) noexcept;

Color lerp_color(const Color &a, const Color &b, double u) noexcept;

bool lerp_bool(bool a, bool b, double u) noexcept;

std::string lerp_string(const std::string &a, const std::string &b, double u);

/**
 * Interpolates between two PropertyValues of matching kinds.
 * If types do not match, returns b if u >= 1.0, otherwise a.
 */
PropertyValue interpolate_value(const PropertyValue &a, const PropertyValue &b, double u);

/**
 * Combines two PropertyValues according to BlendMode (Add vs Override).
 * For double properties, Add adds delta to base. For other types, falls back to Override (b).
 */
PropertyValue combine_blended_value(const PropertyValue &base_val, const PropertyValue &new_val,
				    BlendMode mode) noexcept;

} // namespace naxgc
