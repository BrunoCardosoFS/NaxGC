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

#include <filesystem>
#include <string>
#include <vector>

#include "core/manifest/manifest_parser.hpp"
#include "core/manifest/manifest_validator.hpp"
#include "core/scene/scene.hpp"
#include "core/util/color.hpp"
#include "core/util/mat2d.hpp"

namespace {

std::filesystem::path source_root()
{
	return std::filesystem::u8path(NAXGC_SOURCE_DIR);
}

} // namespace

TEST_CASE("scene_orders_layers_by_svg_z_order_and_resolves_hierarchy")
{
	const std::filesystem::path tpl_dir = source_root() / "template";
	const naxgc::TemplateLoadResult res = naxgc::load_template_package(tpl_dir);
	REQUIRE(res.is_ok());
	REQUIRE(res.loaded.has_value());

	const naxgc::Scene scene = naxgc::Scene::from_template(*res.loaded, false);
	REQUIRE(scene.layers().size() == 12);

	const std::vector<std::string> expected_z_order = {
		"linha1", "linha1texto", "linha1border",   "linha2",         "linha2texto", "linha2border",
		"logo",   "tag_aovivo",  "tag_aovivo_dot", "tag_aovivo_txt", "linha1mask",  "linha2mask",
	};

	for (std::size_t i = 0; i < expected_z_order.size(); ++i) {
		CHECK(scene.layers()[i].name == expected_z_order[i]);
		CHECK(scene.layers()[i].index == i);
	}

	const naxgc::SceneLayer *tag_aovivo = scene.find_layer("tag_aovivo");
	const naxgc::SceneLayer *tag_dot = scene.find_layer("tag_aovivo_dot");
	const naxgc::SceneLayer *tag_txt = scene.find_layer("tag_aovivo_txt");
	const naxgc::SceneLayer *linha1mask = scene.find_layer("linha1mask");
	const naxgc::SceneLayer *linha1texto = scene.find_layer("linha1texto");

	REQUIRE(tag_aovivo != nullptr);
	REQUIRE(tag_dot != nullptr);
	REQUIRE(tag_txt != nullptr);
	REQUIRE(linha1mask != nullptr);
	REQUIRE(linha1texto != nullptr);

	CHECK(!tag_aovivo->parent_index.has_value());
	REQUIRE(tag_aovivo->children_indices.size() == 2);
	CHECK(tag_aovivo->children_indices[0] == tag_dot->index);
	CHECK(tag_aovivo->children_indices[1] == tag_txt->index);

	REQUIRE(tag_dot->parent_index.has_value());
	CHECK(*tag_dot->parent_index == tag_aovivo->index);
	REQUIRE(tag_txt->parent_index.has_value());
	CHECK(*tag_txt->parent_index == tag_aovivo->index);

	CHECK(linha1mask->role == naxgc::LayerRole::Mask);
	REQUIRE(linha1texto->mask_layer_index.has_value());
	CHECK(*linha1texto->mask_layer_index == linha1mask->index);
}

TEST_CASE("scene_evaluates_parent_child_matrices_and_opacity_inheritance")
{
	const std::filesystem::path tpl_dir = source_root() / "template";
	const naxgc::TemplateLoadResult res = naxgc::load_template_package(tpl_dir);
	REQUIRE(res.is_ok());

	naxgc::Scene scene = naxgc::Scene::from_template(*res.loaded, false);

	// Move parent `tag_aovivo` by (+150, -40) and set opacity to 0.5
	REQUIRE(scene.set_property_base("layers.tag_aovivo.x", 150.0).is_ok());
	REQUIRE(scene.set_property_base("layers.tag_aovivo.y", -40.0).is_ok());
	REQUIRE(scene.set_property_base("layers.tag_aovivo.opacity", 0.5).is_ok());

	// Set child `tag_aovivo_dot` opacity to 0.4 and x offset to +10
	REQUIRE(scene.set_property_base("layers.tag_aovivo_dot.x", 10.0).is_ok());
	REQUIRE(scene.set_property_base("layers.tag_aovivo_dot.opacity", 0.4).is_ok());

	scene.evaluate_transforms();

	const naxgc::SceneLayer *parent = scene.find_layer("tag_aovivo");
	const naxgc::SceneLayer *dot = scene.find_layer("tag_aovivo_dot");
	REQUIRE(parent != nullptr);
	REQUIRE(dot != nullptr);

	CHECK(parent->effective_opacity == doctest::Approx(0.5));
	CHECK(dot->effective_opacity == doctest::Approx(0.2));

	// Verify world matrix translates child by parent + child displacement (150 + 10 = 160, -40 + 0 = -40)
	const auto [wx, wy] = dot->world_matrix.apply(120.0, 790.0);
	CHECK(wx == doctest::Approx(280.0));
	CHECK(wy == doctest::Approx(750.0));

	// Hiding parent hides children
	REQUIRE(scene.set_property_base("layers.tag_aovivo.visible", false).is_ok());
	scene.evaluate_transforms();
	CHECK(!parent->effective_visible);
	CHECK(!dot->effective_visible);
}

TEST_CASE("scene_pivot_origin_scales_around_layer_center")
{
	const naxgc::Rect2D bbox{100.0, 200.0, 80.0, 40.0};
	const std::array<double, 2> center_origin{0.5, 0.5};

	// Center of bbox is (140, 220). Scaling by 2x around center should keep (140, 220) fixed
	// and move top-left (100, 200) to (60, 180).
	const naxgc::Mat2D mat = naxgc::compute_layer_local_matrix(bbox, center_origin, 0.0, 0.0, 2.0, 2.0, 0.0);
	const auto [cx, cy] = mat.apply(140.0, 220.0);
	CHECK(cx == doctest::Approx(140.0));
	CHECK(cy == doctest::Approx(220.0));

	const auto [tlx, tly] = mat.apply(100.0, 200.0);
	CHECK(tlx == doctest::Approx(60.0));
	CHECK(tly == doctest::Approx(180.0));
}

TEST_CASE("scene_tracks_content_dirty_only_on_visual_content_changes")
{
	const std::filesystem::path tpl_dir = source_root() / "template";
	const naxgc::TemplateLoadResult res = naxgc::load_template_package(tpl_dir);
	REQUIRE(res.is_ok());

	naxgc::Scene scene = naxgc::Scene::from_template(*res.loaded, false);
	for (auto &l : scene.layers()) {
		l.content_dirty = false;
	}

	// Transform/visibility changes must NOT mark content_dirty
	REQUIRE(scene.set_property_base("layers.linha1.x", -400.0).is_ok());
	REQUIRE(scene.set_property_base("layers.linha1.y", 25.0).is_ok());
	REQUIRE(scene.set_property_base("layers.linha1.scale", 1.5).is_ok());
	REQUIRE(scene.set_property_base("layers.linha1.rotation", 45.0).is_ok());
	REQUIRE(scene.set_property_base("layers.linha1.opacity", 0.75).is_ok());
	REQUIRE(scene.set_property_base("layers.linha1.visible", false).is_ok());
	CHECK(!scene.find_layer("linha1")->content_dirty);

	// Changing `fill` on an SVG layer MUST mark content_dirty
	REQUIRE(scene.set_property_base("layers.linha1.fill", std::string("#b00020")).is_ok());
	CHECK(scene.find_layer("linha1")->content_dirty);

	// Reset dirty flag and set the exact same color again -> must NOT mark dirty
	scene.find_layer("linha1")->content_dirty = false;
	REQUIRE(scene.set_property_base("layers.linha1.fill", std::string("#b00020")).is_ok());
	CHECK(!scene.find_layer("linha1")->content_dirty);

	// Changing `text` or `color` on a text layer MUST mark content_dirty
	CHECK(!scene.find_layer("linha1texto")->content_dirty);
	REQUIRE(scene.set_property_base("layers.linha1texto.text", std::string("Maria Silva")).is_ok());
	CHECK(scene.find_layer("linha1texto")->content_dirty);
}

TEST_CASE("scene_hybrid_raster_scale_auto_detection_and_manual_override")
{
	const std::filesystem::path valid_dir = source_root() / "tests" / "fixtures" / "manifests" / "valid";
	const std::string json_str = R"({
		"schema": 1,
		"meta": { "id": "test.scale", "name": "Scale Test", "version": "0.1.0" },
		"canvas": { "width": 1920, "height": 1080, "fps": 60 },
		"svg": "layout.svg",
		"layers": {
			"auto_scaled": { "type": "svg", "id": "bar" },
			"manual_scaled": { "type": "text", "id": "txt_title", "text": "Hi", "rasterScale": 1.75 }
		},
		"animations": {
			"zoom": {
				"tracks": [
					{ "layer": "auto_scaled", "prop": "scale", "from": 1.0, "to": 2.5, "dur": 0.5 }
				]
			}
		}
	})";

	std::vector<naxgc::Diagnostic> diags;
	auto parsed = naxgc::parse_manifest_json(json_str, diags);
	REQUIRE(parsed.has_value());

	std::optional<naxgc::SvgInspectionResult> svg_info;
	std::string svg_content;
	naxgc::validate_manifest_package(*parsed, valid_dir, svg_info, svg_content, diags);
	REQUIRE(svg_info.has_value());

	naxgc::LoadedTemplate loaded{std::move(*parsed), std::move(svg_content), std::move(*svg_info), {}};
	const naxgc::Scene scene = naxgc::Scene::from_template(loaded, false);

	const naxgc::SceneLayer *auto_layer = scene.find_layer("auto_scaled");
	const naxgc::SceneLayer *manual_layer = scene.find_layer("manual_scaled");
	REQUIRE(auto_layer != nullptr);
	REQUIRE(manual_layer != nullptr);

	CHECK(auto_layer->effective_raster_scale == doctest::Approx(2.5));
	CHECK(manual_layer->effective_raster_scale == doctest::Approx(1.75));
}

TEST_CASE("scene_applies_named_states_and_animated_overrides")
{
	const std::filesystem::path tpl_dir = source_root() / "template";
	const naxgc::TemplateLoadResult res = naxgc::load_template_package(tpl_dir);
	REQUIRE(res.is_ok());

	naxgc::Scene scene = naxgc::Scene::from_template(*res.loaded, true);
	// initialState is "offair", so linha1.x base is -1200
	CHECK(scene.find_layer("linha1")->props.x.effective() == doctest::Approx(-1200.0));

	// Animated override takes precedence over base
	REQUIRE(scene.set_layer_property_animated("linha1", "x", -300.0).is_ok());
	CHECK(scene.find_layer("linha1")->props.x.effective() == doctest::Approx(-300.0));

	// Clearing animated override reverts to base (-1200)
	REQUIRE(scene.clear_layer_property_animated("linha1", "x").is_ok());
	CHECK(scene.find_layer("linha1")->props.x.effective() == doctest::Approx(-1200.0));

	// Applying "tema_vermelho" changes linha1.fill to #b00020
	REQUIRE(scene.apply_state("tema_vermelho").is_ok());
	const auto &fill_opt = scene.find_layer("linha1")->props.fill.effective();
	REQUIRE(fill_opt.has_value());
	CHECK(fill_opt->to_hex_string() == "#b00020");
}
