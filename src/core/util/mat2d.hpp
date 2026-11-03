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

#include <array>
#include <cmath>
#include <utility>

namespace naxgc {

struct Rect2D {
	double x{0.0};
	double y{0.0};
	double width{0.0};
	double height{0.0};

	constexpr bool is_empty() const noexcept { return width <= 0.0 || height <= 0.0; }
};

/**
 * 2D affine transformation matrix:
 * | a  c  tx |
 * | b  d  ty |
 * | 0  0  1  |
 *
 * Point transformation:
 *   x' = a * x + c * y + tx
 *   y' = b * x + d * y + ty
 */
struct Mat2D {
	double a{1.0};
	double b{0.0};
	double c{0.0};
	double d{1.0};
	double tx{0.0};
	double ty{0.0};

	static constexpr Mat2D identity() noexcept { return Mat2D{}; }

	static constexpr Mat2D translation(double dx, double dy) noexcept { return Mat2D{1.0, 0.0, 0.0, 1.0, dx, dy}; }

	static constexpr Mat2D scale(double sx, double sy) noexcept { return Mat2D{sx, 0.0, 0.0, sy, 0.0, 0.0}; }

	/**
	 * Clockwise rotation in degrees in canvas coordinates (+X right, +Y down).
	 */
	static Mat2D rotation_deg(double deg) noexcept
	{
		if (deg == 0.0) {
			return identity();
		}
		constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
		const double rad = deg * kDegToRad;
		const double cos_a = std::cos(rad);
		const double sin_a = std::sin(rad);
		return Mat2D{cos_a, sin_a, -sin_a, cos_a, 0.0, 0.0};
	}

	constexpr Mat2D operator*(const Mat2D &rhs) const noexcept
	{
		return Mat2D{
			a * rhs.a + c * rhs.b, b * rhs.a + d * rhs.b,        a * rhs.c + c * rhs.d,
			b * rhs.c + d * rhs.d, a * rhs.tx + c * rhs.ty + tx, b * rhs.tx + d * rhs.ty + ty,
		};
	}

	constexpr std::pair<double, double> apply(double x, double y) const noexcept
	{
		return {a * x + c * y + tx, b * x + d * y + ty};
	}

	bool approx_equals(const Mat2D &other, double eps = 1e-6) const noexcept
	{
		return std::abs(a - other.a) <= eps && std::abs(b - other.b) <= eps && std::abs(c - other.c) <= eps &&
		       std::abs(d - other.d) <= eps && std::abs(tx - other.tx) <= eps && std::abs(ty - other.ty) <= eps;
	}
};

/**
 * Computes the local canvas-space transformation matrix for a layer given its rest bounding box,
 * normalized pivot `origin` ([0..1, 0..1]), canvas displacement (`dx`, `dy`), effective scale
 * (`scale_x`, `scale_y`), and clockwise rotation in degrees (`rotation_deg`).
 */
inline Mat2D compute_layer_local_matrix(const Rect2D &rest_bbox, const std::array<double, 2> &origin, double dx,
					double dy, double scale_x, double scale_y, double rotation_deg) noexcept
{
	const double px = rest_bbox.x + origin[0] * rest_bbox.width;
	const double py = rest_bbox.y + origin[1] * rest_bbox.height;

	return Mat2D::translation(dx + px, dy + py) * Mat2D::rotation_deg(rotation_deg) *
	       Mat2D::scale(scale_x, scale_y) * Mat2D::translation(-px, -py);
}

} // namespace naxgc
