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

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "core/anim/animator.hpp"
#include "core/manifest/manifest_validator.hpp"
#include "core/scene/scene.hpp"

using namespace naxgc;

namespace {

std::filesystem::path get_template_dir()
{
#ifdef NAXGC_SOURCE_DIR
	return std::filesystem::path(NAXGC_SOURCE_DIR) / "template";
#else
	return std::filesystem::path("template");
#endif
}

double get_prop_double(const Scene &scene, std::string_view layer, std::string_view prop)
{
	auto res = scene.get_layer_property_effective(layer, prop);
	if (!res.is_ok()) {
		return 0.0;
	}
	if (const auto *val = std::get_if<double>(&res.value())) {
		return *val;
	}
	return 0.0;
}

} // namespace

TEST_SUITE("core::anim::golden")
{
	TEST_CASE("Template main.in and main.out 60fps golden evaluation")
	{
		const auto tpl_dir = get_template_dir();
		TemplateLoadResult res = load_template_package(tpl_dir);
		REQUIRE(res.is_ok());
		REQUIRE(res.loaded.has_value());

		const LoadedTemplate &tpl = *res.loaded;
		Scene scene = Scene::from_template(tpl, true);
		Animator animator;
		animator.set_target(&tpl.manifest, &scene, true);

		// 1. Validate initial state ('offair')
		CHECK(get_prop_double(scene, "linha1", "scaleX") == doctest::Approx(0.0));
		CHECK(get_prop_double(scene, "linha2", "scaleX") == doctest::Approx(0.0));
		CHECK(get_prop_double(scene, "linha1mask", "scaleX") == doctest::Approx(0.0));
		CHECK(get_prop_double(scene, "linha2mask", "scaleX") == doctest::Approx(0.0));
		CHECK(get_prop_double(scene, "logo", "scale") == doctest::Approx(0.0));
		CHECK(get_prop_double(scene, "tag_aovivo", "opacity") == doctest::Approx(0.0));

		// 2. Trigger action 'show'
		REQUIRE(animator.run_action("show").is_ok());
		CHECK(animator.is_clip_active("main.in"));

		constexpr double dt_60fps = 1.0 / 60.0;

		// Tick through 120 frames (2.0 seconds)
		struct FrameSnapshot {
			int frame{0};
			double linha1_scale_x{0.0};
			double linha1mask_scale_x{0.0};
			double linha1border_scale_x{0.0};
			double linha2_scale_x{0.0};
			double linha2mask_scale_x{0.0};
			double logo_scale{0.0};
			double tag_opacity{0.0};
		};

		std::vector<FrameSnapshot> snapshots;

		for (int frame = 0; frame <= 120; ++frame) {
			if (frame > 0) {
				animator.tick(dt_60fps);
			}

			if (frame == 0 || frame == 15 || frame == 30 || frame == 45 || frame == 60 || frame == 90 ||
			    frame == 120) {
				snapshots.push_back(FrameSnapshot{
					frame,
					get_prop_double(scene, "linha1", "scaleX"),
					get_prop_double(scene, "linha1mask", "scaleX"),
					get_prop_double(scene, "linha1border", "scaleX"),
					get_prop_double(scene, "linha2", "scaleX"),
					get_prop_double(scene, "linha2mask", "scaleX"),
					get_prop_double(scene, "logo", "scale"),
					get_prop_double(scene, "tag_aovivo", "opacity"),
				});
			}
		}

		// Verify key choreography milestones of main.in:
		// Frame 0: still at offair start
		CHECK(snapshots[0].linha1_scale_x == doctest::Approx(0.0));
		CHECK(snapshots[0].linha1mask_scale_x == doctest::Approx(0.0));
		CHECK(snapshots[0].linha1border_scale_x == doctest::Approx(0.0));
		CHECK(snapshots[0].logo_scale == doctest::Approx(0.0));

		// Frame 15 (~0.25s): logo is animating with outBack (reaches > 0.8), border leads as a trail, linha1 and its mask expand in lockstep
		CHECK(snapshots[1].logo_scale > 0.7);
		CHECK(snapshots[1].linha1border_scale_x > snapshots[1].linha1_scale_x);
		CHECK(snapshots[1].linha1_scale_x == doctest::Approx(snapshots[1].linha1mask_scale_x).epsilon(1e-2));

		// Frame 30 (~0.5s): logo has reached overshoot (~1.0), linha1 width is close to 1, linha2 and its mask expand in lockstep
		CHECK(snapshots[2].logo_scale > 0.95);
		CHECK(snapshots[2].linha1_scale_x > 0.7);
		CHECK(snapshots[2].linha1_scale_x == doctest::Approx(snapshots[2].linha1mask_scale_x).epsilon(1e-2));
		CHECK(snapshots[2].linha2_scale_x > 0.0);
		CHECK(snapshots[2].linha2_scale_x == doctest::Approx(snapshots[2].linha2mask_scale_x).epsilon(1e-2));

		// Frame 60 (1.0s): linha1 and logo have settled, linha2 is in final expansion stage
		CHECK(snapshots[4].linha1_scale_x > 0.95);
		CHECK(snapshots[4].linha1mask_scale_x > 0.95);
		CHECK(snapshots[4].linha2_scale_x > 0.95);
		CHECK(snapshots[4].logo_scale == doctest::Approx(1.0).epsilon(1e-2));

		// Frame 90 (1.5s): main.in has fully settled at rest position (scaleX = 1)
		CHECK(snapshots[5].linha1_scale_x == doctest::Approx(1.0).epsilon(1e-2));
		CHECK(snapshots[5].linha1mask_scale_x == doctest::Approx(1.0).epsilon(1e-2));
		CHECK(snapshots[5].linha1border_scale_x == doctest::Approx(1.0).epsilon(1e-2));
		CHECK(snapshots[5].linha2_scale_x == doctest::Approx(1.0).epsilon(1e-2));
		CHECK(snapshots[5].linha2mask_scale_x == doctest::Approx(1.0).epsilon(1e-2));
		CHECK(snapshots[5].logo_scale == doctest::Approx(1.0).epsilon(1e-2));
		CHECK(snapshots[5].tag_opacity == doctest::Approx(1.0).epsilon(1e-2));

		// Frame 90 and 120: loop_vivo is active (tag_aovivo_dot opacity is pulsing)
		CHECK(animator.is_clip_active("loop_vivo"));
		CHECK(!animator.is_clip_active("main.in"));

		// 3. Trigger action 'hide'
		REQUIRE(animator.run_action("hide").is_ok());
		CHECK(animator.is_clip_active("main.out"));

		// Tick through 70 frames (~1.17 seconds) of out animation
		for (int frame = 1; frame <= 70; ++frame) {
			animator.tick(dt_60fps);
		}

		// At the end of hide, all elements are back to offair positions (scaleX = 0)
		CHECK(get_prop_double(scene, "linha1", "scaleX") == doctest::Approx(0.0).epsilon(1e-2));
		CHECK(get_prop_double(scene, "linha2", "scaleX") == doctest::Approx(0.0).epsilon(1e-2));
		CHECK(get_prop_double(scene, "linha1mask", "scaleX") == doctest::Approx(0.0).epsilon(1e-2));
		CHECK(get_prop_double(scene, "linha2mask", "scaleX") == doctest::Approx(0.0).epsilon(1e-2));
		CHECK(get_prop_double(scene, "logo", "scale") == doctest::Approx(0.0).epsilon(1e-2));
		CHECK(get_prop_double(scene, "tag_aovivo", "opacity") == doctest::Approx(0.0).epsilon(1e-2));
		CHECK(!animator.is_clip_active("loop_vivo"));
	}
}
