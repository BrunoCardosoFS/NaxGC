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

#include "core/anim/interpolator.hpp"

#include <algorithm>
#include <cmath>

namespace naxgc {

double lerp_double(double a, double b, double u) noexcept
{
	return a + (b - a) * u;
}

Color lerp_color(const Color &a, const Color &b, double u) noexcept
{
	const auto lerp_ch = [u](uint8_t c1, uint8_t c2) -> uint8_t {
		const double val = static_cast<double>(c1) + (static_cast<double>(c2) - static_cast<double>(c1)) * u;
		return static_cast<uint8_t>(std::clamp(std::round(val), 0.0, 255.0));
	};

	return Color{lerp_ch(a.r, b.r), lerp_ch(a.g, b.g), lerp_ch(a.b, b.b), lerp_ch(a.a, b.a)};
}

bool lerp_bool(bool a, bool b, double u) noexcept
{
	return (u >= 1.0) ? b : a;
}

std::string lerp_string(const std::string &a, const std::string &b, double u)
{
	return (u > 0.0) ? b : a;
}

PropertyValue interpolate_value(const PropertyValue &a, const PropertyValue &b, double u)
{
	if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
		return lerp_double(std::get<double>(a), std::get<double>(b), u);
	}
	if (std::holds_alternative<Color>(a) && std::holds_alternative<Color>(b)) {
		return lerp_color(std::get<Color>(a), std::get<Color>(b), u);
	}
	if (std::holds_alternative<bool>(a) && std::holds_alternative<bool>(b)) {
		return lerp_bool(std::get<bool>(a), std::get<bool>(b), u);
	}
	if (std::holds_alternative<std::string>(a) && std::holds_alternative<std::string>(b)) {
		return lerp_string(std::get<std::string>(a), std::get<std::string>(b), u);
	}

	return (u >= 1.0) ? b : a;
}

PropertyValue combine_blended_value(const PropertyValue &base_val, const PropertyValue &new_val,
				    BlendMode mode) noexcept
{
	if (mode == BlendMode::Add && std::holds_alternative<double>(base_val) &&
	    std::holds_alternative<double>(new_val)) {
		return std::get<double>(base_val) + std::get<double>(new_val);
	}
	return new_val;
}

} // namespace naxgc
