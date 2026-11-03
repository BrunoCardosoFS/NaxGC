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

#include <doctest/doctest.h>

#include "core/anim/animator.hpp"
#include "core/manifest/manifest_types.hpp"
#include "core/scene/scene.hpp"

using namespace naxgc;

namespace {

LoadedTemplate create_test_template()
{
	LoadedTemplate tpl;
	Manifest &m = tpl.manifest;
	m.schema = 1;
	m.meta.id = "test.animator";
	m.canvas = {1920, 1080, 60.0};
	m.svg = "layout.svg";

	LayerDef l1;
	l1.name = "box";
	l1.type = LayerType::Svg;
	l1.id = "rect_box";
	m.layers.emplace("box", l1);
	m.layer_declaration_order.push_back("box");

	LayerDef l2;
	l2.name = "title";
	l2.type = LayerType::Text;
	l2.id = "txt_title";
	l2.text = "Initial Text";
	m.layers.emplace("title", l2);
	m.layer_declaration_order.push_back("title");

	tpl.svg_info.ordered_ids = {"rect_box", "txt_title"};
	tpl.svg_info.elements_by_id.emplace("rect_box", SvgElementInfo{"rect_box", "rect", {}, 0.0, 0.0, 100.0, 100.0});
	tpl.svg_info.elements_by_id.emplace("txt_title",
					    SvgElementInfo{"txt_title", "rect", {}, 0.0, 100.0, 200.0, 50.0});

	return tpl;
}

} // namespace

TEST_SUITE("core::anim::animator")
{
	TEST_CASE("Single track animation and onEnd hold")
	{
		LoadedTemplate tpl = create_test_template();
		Manifest &m = tpl.manifest;

		ClipDef clip;
		clip.name = "move_box";
		TrackDef t;
		t.layer = "box";
		t.prop = "x";
		t.from = 0.0;
		t.to = 100.0;
		t.start = 0.0;
		t.dur = 1.0;
		t.ease.type = EasingType::Linear;
		clip.tracks.push_back(t);
		clip.on_end = OnEndPolicy::Hold;
		m.animations.emplace(clip.name, clip);

		Scene scene = Scene::from_template(tpl);
		Animator animator;
		animator.set_max_dt(std::nullopt);
		animator.set_target(&m, &scene, false);

		REQUIRE(animator.play("move_box").is_ok());
		CHECK(animator.is_clip_active("move_box"));

		// At start
		animator.tick(0.0);
		auto val_res = scene.get_layer_property_effective("box", "x");
		REQUIRE(val_res.is_ok());
		CHECK(std::get<double>(val_res.value()) == doctest::Approx(0.0));

		// At half duration (0.5s)
		animator.tick(0.5);
		val_res = scene.get_layer_property_effective("box", "x");
		REQUIRE(val_res.is_ok());
		CHECK(std::get<double>(val_res.value()) == doctest::Approx(50.0));

		// At completion (another 0.5s -> 1.0s)
		animator.tick(0.5);
		val_res = scene.get_layer_property_effective("box", "x");
		REQUIRE(val_res.is_ok());
		CHECK(std::get<double>(val_res.value()) == doctest::Approx(100.0));

		// Hold policy should have stored 100 into base and cleared animated override
		const SceneLayer *box_layer = scene.find_layer("box");
		REQUIRE(box_layer != nullptr);
		CHECK(box_layer->props.x.base == doctest::Approx(100.0));
		CHECK(!box_layer->props.x.animated.has_value());
		CHECK(!animator.is_clip_active("move_box"));
	}

	TEST_CASE("onEnd revert restores initial base")
	{
		LoadedTemplate tpl = create_test_template();
		Manifest &m = tpl.manifest;

		ClipDef clip;
		clip.name = "flash_box";
		TrackDef t;
		t.layer = "box";
		t.prop = "opacity";
		t.from = 1.0;
		t.to = 0.0;
		t.dur = 0.4;
		clip.tracks.push_back(t);
		clip.on_end = OnEndPolicy::Revert;
		m.animations.emplace(clip.name, clip);

		Scene scene = Scene::from_template(tpl);
		Animator animator;
		animator.set_max_dt(std::nullopt);
		animator.set_target(&m, &scene, false);

		REQUIRE(animator.play("flash_box").is_ok());
		animator.tick(0.2);
		CHECK(std::get<double>(scene.get_layer_property_effective("box", "opacity").value()) ==
		      doctest::Approx(0.5));

		// Advance past finish
		animator.tick(0.3);
		const SceneLayer *box_layer = scene.find_layer("box");
		REQUIRE(box_layer != nullptr);
		CHECK(box_layer->props.opacity.base == doctest::Approx(1.0));
		CHECK(!box_layer->props.opacity.animated.has_value());
	}

	TEST_CASE("Interruption with from omitted captures current effective value")
	{
		LoadedTemplate tpl = create_test_template();
		Manifest &m = tpl.manifest;

		ClipDef clip_in;
		clip_in.name = "in";
		TrackDef t1;
		t1.layer = "box";
		t1.prop = "x";
		t1.from = -100.0;
		t1.to = 0.0;
		t1.dur = 1.0;
		clip_in.tracks.push_back(t1);
		m.animations.emplace(clip_in.name, clip_in);

		ClipDef clip_out;
		clip_out.name = "out";
		TrackDef t2;
		t2.layer = "box";
		t2.prop = "x";
		// from omitted!
		t2.to = -100.0;
		t2.dur = 0.5;
		clip_out.tracks.push_back(t2);
		m.animations.emplace(clip_out.name, clip_out);

		Scene scene = Scene::from_template(tpl);
		Animator animator;
		animator.set_max_dt(std::nullopt);
		animator.set_target(&m, &scene, false);

		// Start 'in' and advance halfway: -100 to 0 in 1s -> at 0.5s, x = -50
		REQUIRE(animator.play("in").is_ok());
		animator.tick(0.5);
		CHECK(std::get<double>(scene.get_layer_property_effective("box", "x").value()) ==
		      doctest::Approx(-50.0));

		// Interrupt with 'out'
		animator.stop("in");
		REQUIRE(animator.play("out").is_ok());

		// At start of 'out', it captures x = -50. At halfway through 'out' (0.25s), x = -75
		animator.tick(0.25);
		CHECK(std::get<double>(scene.get_layer_property_effective("box", "x").value()) ==
		      doctest::Approx(-75.0));

		// At end of 'out' (another 0.25s), x = -100
		animator.tick(0.25);
		CHECK(std::get<double>(scene.get_layer_property_effective("box", "x").value()) ==
		      doctest::Approx(-100.0));
	}

	TEST_CASE("Trigger with residual time and linked completion")
	{
		LoadedTemplate tpl = create_test_template();
		Manifest &m = tpl.manifest;

		// Parent clip triggers child at 0.03s
		ClipDef parent;
		parent.name = "parent";
		TrackDef pt;
		pt.layer = "box";
		pt.prop = "x";
		pt.from = 0.0;
		pt.to = 10.0;
		pt.dur = 0.05;
		parent.tracks.push_back(pt);

		TriggerDef trig;
		trig.play = "child";
		trig.at = 0.03;
		trig.mode = TriggerMode::Linked;
		parent.triggers.push_back(trig);
		m.animations.emplace(parent.name, parent);

		// Child clip moves title from 0 to 100 in 0.1s
		ClipDef child;
		child.name = "child";
		TrackDef ct;
		ct.layer = "title";
		ct.prop = "size";
		ct.from = 0.0;
		ct.to = 100.0;
		ct.dur = 0.1;
		child.tracks.push_back(ct);
		m.animations.emplace(child.name, child);

		Scene scene = Scene::from_template(tpl);
		Animator animator;
		animator.set_max_dt(std::nullopt);
		animator.set_target(&m, &scene, false);

		REQUIRE(animator.play("parent").is_ok());

		// Tick with dt = 0.05s.
		// Parent passes trigger at 0.03s, so residual time is 0.05 - 0.03 = 0.02s!
		// Child starts at t = 0.02s in the SAME tick.
		// For child with dur = 0.1s: size should be 20.0!
		animator.tick(0.05);

		CHECK(animator.is_clip_active("child"));
		auto size_val = scene.get_layer_property_effective("title", "size");
		REQUIRE(size_val.is_ok());
		CHECK(std::get<double>(size_val.value()) == doctest::Approx(20.0).epsilon(1e-4));

		// Parent tracks completed at 0.05s, but because child is linked, parent is still active!
		CHECK(animator.is_clip_active("parent"));

		// Advance child to completion (remaining 0.08s)
		animator.tick(0.08);
		CHECK(!animator.is_clip_active("child"));
		CHECK(!animator.is_clip_active("parent"));
	}

	TEST_CASE("Actions with delay and states")
	{
		LoadedTemplate tpl = create_test_template();
		Manifest &m = tpl.manifest;

		// Define a state
		StateDef st;
		st.name = "test_state";
		st.entries.push_back(StateEntryDef{"", "layers.box.x", 42.0});
		m.states.emplace("test_state", st);

		// Define an action with delay
		std::vector<ActionStepDef> steps;
		ActionStepDef step1;
		step1.type = ActionStepType::Set;
		step1.target = "test_state";
		step1.delay = 0.5;
		steps.push_back(step1);
		m.actions.emplace("apply_delayed", steps);

		Scene scene = Scene::from_template(tpl);
		Animator animator;
		animator.set_max_dt(std::nullopt);
		animator.set_target(&m, &scene, false);

		REQUIRE(animator.run_action("apply_delayed").is_ok());

		// Before delay
		animator.tick(0.2);
		CHECK(std::get<double>(scene.get_layer_property_effective("box", "x").value()) == doctest::Approx(0.0));

		// After delay expires (0.3s more -> total 0.5s)
		animator.tick(0.3);
		CHECK(std::get<double>(scene.get_layer_property_effective("box", "x").value()) ==
		      doctest::Approx(42.0));
	}
}
