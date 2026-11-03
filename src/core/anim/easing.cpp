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

#include "core/anim/easing.hpp"

#include <algorithm>
#include <cmath>

namespace naxgc {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Evaluates 1D Bezier coordinate given control points p0=0, p1, p2, p3=1
inline double sample_bezier(double p1, double p2, double t) noexcept
{
	// ((1 - 3*p2 + 3*p1)*t + (3*p2 - 6*p1))*t + 3*p1)*t
	return (((1.0 - 3.0 * p2 + 3.0 * p1) * t + (3.0 * p2 - 6.0 * p1)) * t + 3.0 * p1) * t;
}

// Derivative of sample_bezier with respect to t
inline double sample_bezier_derivative(double p1, double p2, double t) noexcept
{
	return ((3.0 * (1.0 - 3.0 * p2 + 3.0 * p1)) * t + 2.0 * (3.0 * p2 - 6.0 * p1)) * t + 3.0 * p1;
}

} // namespace

double evaluate_cubic_bezier(double x1, double y1, double x2, double y2, double u) noexcept
{
	if (u <= 0.0) {
		return 0.0;
	}
	if (u >= 1.0) {
		return 1.0;
	}

	// Solve x(t) = u using Newton-Raphson
	double t = u;
	constexpr int kNewtonIterations = 8;
	for (int i = 0; i < kNewtonIterations; ++i) {
		const double current_x = sample_bezier(x1, x2, t) - u;
		if (std::abs(current_x) < 1e-7) {
			return sample_bezier(y1, y2, t);
		}
		const double current_slope = sample_bezier_derivative(x1, x2, t);
		if (std::abs(current_slope) < 1e-6) {
			break;
		}
		t -= current_x / current_slope;
	}

	// Fallback to bisection if Newton did not converge or slope was near zero
	double t_low = 0.0;
	double t_high = 1.0;
	t = u;
	constexpr int kBisectionIterations = 16;
	for (int i = 0; i < kBisectionIterations; ++i) {
		const double current_x = sample_bezier(x1, x2, t);
		if (std::abs(current_x - u) < 1e-7) {
			return sample_bezier(y1, y2, t);
		}
		if (current_x < u) {
			t_low = t;
		} else {
			t_high = t;
		}
		t = 0.5 * (t_low + t_high);
	}

	return sample_bezier(y1, y2, t);
}

double evaluate_easing_type(EasingType type, double u) noexcept
{
	const double clamped_u = std::clamp(u, 0.0, 1.0);

	switch (type) {
	case EasingType::Linear:
		return clamped_u;

	case EasingType::InQuad:
		return clamped_u * clamped_u;

	case EasingType::OutQuad:
		return clamped_u * (2.0 - clamped_u);

	case EasingType::InOutQuad:
		return (clamped_u < 0.5) ? (2.0 * clamped_u * clamped_u) : (-1.0 + (4.0 - 2.0 * clamped_u) * clamped_u);

	case EasingType::InCubic:
		return clamped_u * clamped_u * clamped_u;

	case EasingType::OutCubic: {
		const double inv = clamped_u - 1.0;
		return inv * inv * inv + 1.0;
	}

	case EasingType::InOutCubic: {
		if (clamped_u < 0.5) {
			return 4.0 * clamped_u * clamped_u * clamped_u;
		}
		const double inv = 2.0 * clamped_u - 2.0;
		return 0.5 * inv * inv * inv + 1.0;
	}

	case EasingType::InSine:
		return 1.0 - std::cos((clamped_u * kPi) * 0.5);

	case EasingType::OutSine:
		return std::sin((clamped_u * kPi) * 0.5);

	case EasingType::InOutSine:
		return -0.5 * (std::cos(kPi * clamped_u) - 1.0);

	case EasingType::InExpo:
		return (clamped_u <= 0.0) ? 0.0 : std::pow(2.0, 10.0 * (clamped_u - 1.0));

	case EasingType::OutExpo:
		return (clamped_u >= 1.0) ? 1.0 : (1.0 - std::pow(2.0, -10.0 * clamped_u));

	case EasingType::InOutExpo:
		if (clamped_u <= 0.0) {
			return 0.0;
		}
		if (clamped_u >= 1.0) {
			return 1.0;
		}
		if (clamped_u < 0.5) {
			return 0.5 * std::pow(2.0, 20.0 * clamped_u - 10.0);
		}
		return 1.0 - 0.5 * std::pow(2.0, -20.0 * clamped_u + 10.0);

	case EasingType::InBack: {
		constexpr double c1 = 1.70158;
		constexpr double c3 = c1 + 1.0;
		return c3 * clamped_u * clamped_u * clamped_u - c1 * clamped_u * clamped_u;
	}

	case EasingType::OutBack: {
		constexpr double c1 = 1.70158;
		constexpr double c3 = c1 + 1.0;
		const double inv = clamped_u - 1.0;
		return 1.0 + c3 * inv * inv * inv + c1 * inv * inv;
	}

	case EasingType::InOutBack: {
		constexpr double c1 = 1.70158;
		constexpr double c2 = c1 * 1.525;
		if (clamped_u < 0.5) {
			const double t = 2.0 * clamped_u;
			return 0.5 * (t * t * ((c2 + 1.0) * t - c2));
		}
		const double t = 2.0 * clamped_u - 2.0;
		return 0.5 * (t * t * ((c2 + 1.0) * t + c2) + 2.0);
	}

	case EasingType::OutElastic: {
		if (clamped_u <= 0.0) {
			return 0.0;
		}
		if (clamped_u >= 1.0) {
			return 1.0;
		}
		constexpr double c4 = (2.0 * kPi) / 3.0;
		return std::pow(2.0, -10.0 * clamped_u) * std::sin((clamped_u * 10.0 - 0.75) * c4) + 1.0;
	}

	case EasingType::OutBounce: {
		constexpr double n1 = 7.5625;
		constexpr double d1 = 2.75;
		if (clamped_u < 1.0 / d1) {
			return n1 * clamped_u * clamped_u;
		}
		if (clamped_u < 2.0 / d1) {
			const double t = clamped_u - 1.5 / d1;
			return n1 * t * t + 0.75;
		}
		if (clamped_u < 2.5 / d1) {
			const double t = clamped_u - 2.25 / d1;
			return n1 * t * t + 0.9375;
		}
		const double t = clamped_u - 2.625 / d1;
		return n1 * t * t + 0.984375;
	}

	case EasingType::CubicBezier:
		return clamped_u;
	}

	return clamped_u;
}

double evaluate_easing(const EasingSpec &spec, double u) noexcept
{
	if (spec.type == EasingType::CubicBezier) {
		return evaluate_cubic_bezier(spec.bezier[0], spec.bezier[1], spec.bezier[2], spec.bezier[3], u);
	}
	return evaluate_easing_type(spec.type, u);
}

} // namespace naxgc
