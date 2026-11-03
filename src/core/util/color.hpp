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
#include <string>
#include <string_view>

#include "core/util/result.hpp"

namespace naxgc {

struct Color {
	uint8_t r{255};
	uint8_t g{255};
	uint8_t b{255};
	uint8_t a{255};

	bool operator==(const Color &other) const noexcept
	{
		return r == other.r && g == other.g && b == other.b && a == other.a;
	}

	bool operator!=(const Color &other) const noexcept { return !(*this == other); }

	std::string to_hex_string() const;
};

Result<Color> parse_color(std::string_view hex);

} // namespace naxgc
