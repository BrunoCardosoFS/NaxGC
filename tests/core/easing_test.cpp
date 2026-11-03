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

#include <cmath>
#include <doctest/doctest.h>

#include "core/anim/easing.hpp"

using namespace naxgc;

TEST_SUITE("core::anim::easing")
{
	TEST_CASE("Standard easings endpoint guarantees")
	{
		const EasingType all_types[] = {
			EasingType::Linear,     EasingType::InQuad,    EasingType::OutQuad,    EasingType::InOutQuad,
			EasingType::InCubic,    EasingType::OutCubic,  EasingType::InOutCubic, EasingType::InSine,
			EasingType::OutSine,    EasingType::InOutSine, EasingType::InExpo,     EasingType::OutExpo,
			EasingType::InOutExpo,  EasingType::InBack,    EasingType::OutBack,    EasingType::InOutBack,
			EasingType::OutElastic, EasingType::OutBounce,
		};

		for (const auto type : all_types) {
			CHECK(evaluate_easing_type(type, 0.0) == doctest::Approx(0.0).epsilon(1e-6));
			CHECK(evaluate_easing_type(type, 1.0) == doctest::Approx(1.0).epsilon(1e-6));
		}
	}

	TEST_CASE("Linear easing")
	{
		CHECK(evaluate_easing_type(EasingType::Linear, 0.0) == doctest::Approx(0.0));
		CHECK(evaluate_easing_type(EasingType::Linear, 0.25) == doctest::Approx(0.25));
		CHECK(evaluate_easing_type(EasingType::Linear, 0.5) == doctest::Approx(0.5));
		CHECK(evaluate_easing_type(EasingType::Linear, 0.75) == doctest::Approx(0.75));
		CHECK(evaluate_easing_type(EasingType::Linear, 1.0) == doctest::Approx(1.0));
	}

	TEST_CASE("Quad and Cubic easings at midpoint")
	{
		// inQuad: u^2 -> 0.25
		CHECK(evaluate_easing_type(EasingType::InQuad, 0.5) == doctest::Approx(0.25));
		// outQuad: 2u - u^2 -> 1.0 - 0.25 = 0.75
		CHECK(evaluate_easing_type(EasingType::OutQuad, 0.5) == doctest::Approx(0.75));
		// inOutQuad: 0.5 at 0.5
		CHECK(evaluate_easing_type(EasingType::InOutQuad, 0.5) == doctest::Approx(0.5));

		// inCubic: u^3 -> 0.125
		CHECK(evaluate_easing_type(EasingType::InCubic, 0.5) == doctest::Approx(0.125));
		// outCubic: (u-1)^3 + 1 -> -0.125 + 1 = 0.875
		CHECK(evaluate_easing_type(EasingType::OutCubic, 0.5) == doctest::Approx(0.875));
		// inOutCubic: 0.5 at 0.5
		CHECK(evaluate_easing_type(EasingType::InOutCubic, 0.5) == doctest::Approx(0.5));
	}

	TEST_CASE("Sine and Expo easings")
	{
		CHECK(evaluate_easing_type(EasingType::InOutSine, 0.5) == doctest::Approx(0.5));
		CHECK(evaluate_easing_type(EasingType::InOutExpo, 0.5) == doctest::Approx(0.5));
	}

	TEST_CASE("Back easings overshoot")
	{
		// InBack dips below 0
		CHECK(evaluate_easing_type(EasingType::InBack, 0.3) < 0.0);
		// OutBack overshoots above 1
		CHECK(evaluate_easing_type(EasingType::OutBack, 0.7) > 1.0);
	}

	TEST_CASE("Cubic-bezier solver")
	{
		// Linear via cubic bezier: [0, 0, 1, 1]
		EasingSpec lin_spec{EasingType::CubicBezier, {0.0, 0.0, 1.0, 1.0}};
		CHECK(evaluate_easing(lin_spec, 0.0) == doctest::Approx(0.0).epsilon(1e-4));
		CHECK(evaluate_easing(lin_spec, 0.5) == doctest::Approx(0.5).epsilon(1e-4));
		CHECK(evaluate_easing(lin_spec, 1.0) == doctest::Approx(1.0).epsilon(1e-4));

		// CSS ease: [0.25, 0.1, 0.25, 1.0]
		EasingSpec css_ease{EasingType::CubicBezier, {0.25, 0.1, 0.25, 1.0}};
		CHECK(evaluate_easing(css_ease, 0.0) == doctest::Approx(0.0).epsilon(1e-4));
		CHECK(evaluate_easing(css_ease, 1.0) == doctest::Approx(1.0).epsilon(1e-4));
		const double mid = evaluate_easing(css_ease, 0.5);
		// For CSS ease, y(0.5) is around ~0.802
		CHECK(mid > 0.75);
		CHECK(mid < 0.85);
	}
}
