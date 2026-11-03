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

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "core/manifest/manifest_parser.hpp"
#include "core/manifest/manifest_validator.hpp"
#include "core/scene/scene.hpp"
#include "render/rasterizer_svg.hpp"
#include "render/rasterizer_text.hpp"

namespace {

std::filesystem::path source_root()
{
	return std::filesystem::u8path(NAXGC_SOURCE_DIR);
}

bool has_diagnostic(const std::vector<naxgc::Diagnostic> &diags, std::string_view code)
{
	for (const auto &d : diags) {
		if (d.code == code) {
			return true;
		}
	}
	return false;
}

struct PremultPixel {
	uint8_t b{0};
	uint8_t g{0};
	uint8_t r{0};
	uint8_t a{0};
};

PremultPixel apply_mask_alpha(PremultPixel layer, uint8_t mask_a)
{
	const float m = static_cast<float>(mask_a) / 255.0f;
	return PremultPixel{
		static_cast<uint8_t>(std::round(layer.b * m)),
		static_cast<uint8_t>(std::round(layer.g * m)),
		static_cast<uint8_t>(std::round(layer.r * m)),
		static_cast<uint8_t>(std::round(layer.a * m)),
	};
}

PremultPixel apply_mask_inverse(PremultPixel layer, uint8_t mask_a)
{
	const float m = 1.0f - (static_cast<float>(mask_a) / 255.0f);
	return PremultPixel{
		static_cast<uint8_t>(std::round(layer.b * m)),
		static_cast<uint8_t>(std::round(layer.g * m)),
		static_cast<uint8_t>(std::round(layer.r * m)),
		static_cast<uint8_t>(std::round(layer.a * m)),
	};
}

} // namespace

TEST_CASE("mask_manifest_validation_and_e012_diagnostics")
{
	const std::filesystem::path tpl_dir = source_root() / "template";
	const naxgc::TemplateLoadResult res = naxgc::load_template_package(tpl_dir);
	REQUIRE(res.is_ok());
	REQUIRE(res.loaded.has_value());

	const naxgc::Manifest &m = res.loaded->manifest;

	// Template reference masks must exist and be valid
	auto it1 = m.layers.find("linha1texto");
	REQUIRE(it1 != m.layers.end());
	REQUIRE(it1->second.mask.has_value());
	CHECK(*it1->second.mask == "linha1mask");
	CHECK(it1->second.mask_mode == naxgc::MaskMode::Alpha);

	auto it1m = m.layers.find("linha1mask");
	REQUIRE(it1m != m.layers.end());
	CHECK(it1m->second.role == naxgc::LayerRole::Mask);

	auto it2 = m.layers.find("linha2texto");
	REQUIRE(it2 != m.layers.end());
	REQUIRE(it2->second.mask.has_value());
	CHECK(*it2->second.mask == "linha2mask");
	CHECK(it2->second.mask_mode == naxgc::MaskMode::Alpha);

	auto it2m = m.layers.find("linha2mask");
	REQUIRE(it2m != m.layers.end());
	CHECK(it2m->second.role == naxgc::LayerRole::Mask);

	// Test semantic errors: NXG-E012
	std::optional<naxgc::SvgInspectionResult> svg_info = res.loaded->svg_info;
	std::string svg_content = res.loaded->svg_content;

	// Case 1: Mask points to non-existent layer
	{
		naxgc::Manifest test_m = res.loaded->manifest;
		test_m.layers["linha1texto"].mask = "non_existent_mask";
		std::vector<naxgc::Diagnostic> diags;
		naxgc::validate_manifest_package(test_m, tpl_dir, svg_info, svg_content, diags);
		CHECK(has_diagnostic(diags, "NXG-E012"));
	}

	// Case 2: Mask points to a layer that does not have role: "mask"
	{
		naxgc::Manifest test_m = res.loaded->manifest;
		test_m.layers["linha1texto"].mask = "linha1"; // linha1 has role: normal
		std::vector<naxgc::Diagnostic> diags;
		naxgc::validate_manifest_package(test_m, tpl_dir, svg_info, svg_content, diags);
		CHECK(has_diagnostic(diags, "NXG-E012"));
	}

	// Case 3: Mask layer has a mask itself (mask of a mask)
	{
		naxgc::Manifest test_m = res.loaded->manifest;
		test_m.layers["linha1mask"].mask = "linha2mask"; // linha1mask is a mask layer
		std::vector<naxgc::Diagnostic> diags;
		naxgc::validate_manifest_package(test_m, tpl_dir, svg_info, svg_content, diags);
		CHECK(has_diagnostic(diags, "NXG-E012"));
	}
}

TEST_CASE("mask_pixel_math_alpha_and_inverse")
{
	// Red pixel (premultiplied 255, 0, 0, 255)
	const PremultPixel full_red{0, 0, 255, 255};

	// Alpha mode:
	// Mask 255 -> full color preserved
	const PremultPixel a_full = apply_mask_alpha(full_red, 255);
	CHECK(a_full.r == 255);
	CHECK(a_full.a == 255);

	// Mask 0 -> completely transparent
	const PremultPixel a_zero = apply_mask_alpha(full_red, 0);
	CHECK(a_zero.r == 0);
	CHECK(a_zero.a == 0);

	// Mask 128 (50%) -> half intensity
	const PremultPixel a_half = apply_mask_alpha(full_red, 128);
	CHECK(a_half.r == doctest::Approx(128).epsilon(1));
	CHECK(a_half.a == doctest::Approx(128).epsilon(1));

	// Inverse mode:
	// Mask 255 -> completely transparent
	const PremultPixel i_full = apply_mask_inverse(full_red, 255);
	CHECK(i_full.r == 0);
	CHECK(i_full.a == 0);

	// Mask 0 -> full color preserved
	const PremultPixel i_zero = apply_mask_inverse(full_red, 0);
	CHECK(i_zero.r == 255);
	CHECK(i_zero.a == 255);

	// Mask 128 (50%) -> half intensity
	const PremultPixel i_half = apply_mask_inverse(full_red, 128);
	CHECK(i_half.r == doctest::Approx(127).epsilon(1));
	CHECK(i_half.a == doctest::Approx(127).epsilon(1));
}

TEST_CASE("template_mask_reveals_text_when_moving_from_offset_to_rest")
{
	const std::filesystem::path tpl_dir = source_root() / "template";
	const naxgc::TemplateLoadResult res = naxgc::load_template_package(tpl_dir);
	REQUIRE(res.is_ok());
	REQUIRE(res.loaded.has_value());

	naxgc::Scene scene = naxgc::Scene::from_template(*res.loaded, false);
	naxgc::SvgRasterizer svg_rast(*res.loaded);
	svg_rast.populate_scene_rest_bboxes(scene);
	naxgc::TextRasterizer text_rast(*res.loaded);

	naxgc::SceneLayer *txt_layer = scene.find_layer("linha1texto");
	const naxgc::SceneLayer *mask_layer = scene.find_layer("linha1mask");
	REQUIRE(txt_layer != nullptr);
	REQUIRE(mask_layer != nullptr);

	// Set text content so we have non-transparent pixels to reveal
	REQUIRE(scene.set_property_base("layers.linha1texto.text", std::string("PROFESSOR NAX")).is_ok());

	// Rasterize text layer
	const naxgc::RasterizedBitmap txt_bmp = text_rast.rasterize_layer(*txt_layer, 1.0, 1.0);
	REQUIRE(!txt_bmp.empty());

	std::size_t initial_visible_pixels = 0;
	for (std::size_t i = 3; i < txt_bmp.bgra_premultiplied.size(); i += 4) {
		if (txt_bmp.bgra_premultiplied[i] > 0) {
			++initial_visible_pixels;
		}
	}
	REQUIRE(initial_visible_pixels > 0);

	// In 1080p canvas space:
	// txt_layer frame: x = 292, width = 1036 (range 292 .. 1328)
	// mask_layer rest frame: x = 260, width = 1100 (range 260 .. 1360)
	CHECK(mask_layer->rest_bbox.x <= txt_layer->rest_bbox.x);
	CHECK((mask_layer->rest_bbox.x + mask_layer->rest_bbox.width) >=
	      (txt_layer->rest_bbox.x + txt_layer->rest_bbox.width));

	// At rest pose (scaleX = 1): mask fully encloses the text
	const double rest_mask_left = mask_layer->rest_bbox.x;
	const double rest_mask_right = rest_mask_left + mask_layer->rest_bbox.width * 1.0;
	const double txt_left = txt_layer->rest_bbox.x;
	const double txt_right = txt_left + txt_layer->rest_bbox.width;

	CHECK(rest_mask_left <= txt_left);
	CHECK(rest_mask_right >= txt_right);

	// At animated initial state 'offair' (scaleX = 0):
	// mask width = 1100 * 0 = 0, so right edge = 260.
	// Since 260 < 292 (txt_left), overlap with text is strictly zero!
	const double offair_mask_right = mask_layer->rest_bbox.x + mask_layer->rest_bbox.width * 0.0;
	CHECK(offair_mask_right < txt_left);
}
