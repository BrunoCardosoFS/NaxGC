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

#include "core/util/color.hpp"

#include <cstdio>

namespace naxgc {

namespace {

int hex_nibble(char c) noexcept
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return 10 + (c - 'a');
	}
	if (c >= 'A' && c <= 'F') {
		return 10 + (c - 'A');
	}
	return -1;
}

int hex_byte(char high, char low) noexcept
{
	const int hi = hex_nibble(high);
	const int lo = hex_nibble(low);
	if (hi < 0 || lo < 0) {
		return -1;
	}
	return (hi << 4) | lo;
}

} // namespace

std::string Color::to_hex_string() const
{
	char buf[10];
	if (a == 255) {
		std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
	} else {
		std::snprintf(buf, sizeof(buf), "#%02x%02x%02x%02x", r, g, b, a);
	}
	return std::string(buf);
}

Result<Color> parse_color(std::string_view hex)
{
	if (hex.empty() || hex[0] != '#') {
		return Result<Color>::err(Error{"NXG-E005", "color must start with '#'"});
	}

	const std::string_view digits = hex.substr(1);
	if (digits.size() == 3) {
		const int r = hex_nibble(digits[0]);
		const int g = hex_nibble(digits[1]);
		const int b = hex_nibble(digits[2]);
		if (r < 0 || g < 0 || b < 0) {
			return Result<Color>::err(Error{"NXG-E005", "invalid hex digits in #rgb color"});
		}
		return Result<Color>::ok(Color{
			static_cast<uint8_t>((r << 4) | r),
			static_cast<uint8_t>((g << 4) | g),
			static_cast<uint8_t>((b << 4) | b),
			255,
		});
	}

	if (digits.size() == 6 || digits.size() == 8) {
		const int r = hex_byte(digits[0], digits[1]);
		const int g = hex_byte(digits[2], digits[3]);
		const int b = hex_byte(digits[4], digits[5]);
		int a = 255;
		if (digits.size() == 8) {
			a = hex_byte(digits[6], digits[7]);
		}
		if (r < 0 || g < 0 || b < 0 || a < 0) {
			return Result<Color>::err(Error{"NXG-E005", "invalid hex digits in color"});
		}
		return Result<Color>::ok(Color{
			static_cast<uint8_t>(r),
			static_cast<uint8_t>(g),
			static_cast<uint8_t>(b),
			static_cast<uint8_t>(a),
		});
	}

	return Result<Color>::err(Error{"NXG-E005", "color must follow #rgb, #rrggbb, or #rrggbbaa format"});
}

} // namespace naxgc
