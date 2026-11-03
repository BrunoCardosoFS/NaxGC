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

#include <cstdint>
#include <filesystem>
#include <string>

#include <QGuiApplication>

#include "core/manifest/manifest_validator.hpp"
#include "core/scene/scene.hpp"
#include "render/rasterizer_svg.hpp"
#include "render/rasterizer_text.hpp"

namespace {

std::filesystem::path source_root()
{
	return std::filesystem::u8path(NAXGC_SOURCE_DIR);
}

struct BgraPixel {
	uint8_t b{0};
	uint8_t g{0};
	uint8_t r{0};
	uint8_t a{0};
};

BgraPixel sample_pixel(const naxgc::RasterizedBitmap &bmp, uint32_t x, uint32_t y)
{
	REQUIRE(x < bmp.width);
	REQUIRE(y < bmp.height);
	const std::size_t offset = (static_cast<std::size_t>(y) * bmp.width + x) * 4u;
	return BgraPixel{
		bmp.bgra_premultiplied[offset + 0],
		bmp.bgra_premultiplied[offset + 1],
		bmp.bgra_premultiplied[offset + 2],
		bmp.bgra_premultiplied[offset + 3],
	};
}

std::size_t count_non_transparent_pixels(const naxgc::RasterizedBitmap &bmp)
{
	std::size_t count = 0;
	for (std::size_t i = 3; i < bmp.bgra_premultiplied.size(); i += 4) {
		if (bmp.bgra_premultiplied[i] > 0) {
			++count;
		}
	}
	return count;
}

} // namespace

TEST_CASE("svg_rasterizer_computes_bboxes_and_rasterizes_template_layers_in_1080p")
{
	const std::filesystem::path tpl_dir = source_root() / "template";
	const naxgc::TemplateLoadResult res = naxgc::load_template_package(tpl_dir);
	REQUIRE(res.is_ok());
	REQUIRE(res.loaded.has_value());

	naxgc::Scene scene = naxgc::Scene::from_template(*res.loaded, false);
	const naxgc::SvgRasterizer svg_rast(*res.loaded);
	svg_rast.populate_scene_rest_bboxes(scene);

	CHECK(!svg_rast.has_implicit_background());

	// Verify rest bboxes of SVG and text frame layers in 1080p canvas coordinates
	const naxgc::SceneLayer *linha1 = scene.find_layer("linha1");
	const naxgc::SceneLayer *linha1texto = scene.find_layer("linha1texto");
	const naxgc::SceneLayer *logo = scene.find_layer("logo");
	const naxgc::SceneLayer *tag_aovivo = scene.find_layer("tag_aovivo");
	const naxgc::SceneLayer *tag_dot = scene.find_layer("tag_aovivo_dot");
	const naxgc::SceneLayer *tag_txt = scene.find_layer("tag_aovivo_txt");

	REQUIRE(linha1 != nullptr);
	REQUIRE(linha1texto != nullptr);
	REQUIRE(logo != nullptr);
	REQUIRE(tag_aovivo != nullptr);
	REQUIRE(tag_dot != nullptr);
	REQUIRE(tag_txt != nullptr);

	CHECK(linha1->rest_bbox.x == doctest::Approx(260.0));
	CHECK(linha1->rest_bbox.y == doctest::Approx(820.0));
	CHECK(linha1->rest_bbox.width == doctest::Approx(1100.0));
	CHECK(linha1->rest_bbox.height == doctest::Approx(84.0));

	CHECK(linha1texto->rest_bbox.x == doctest::Approx(292.0));
	CHECK(linha1texto->rest_bbox.y == doctest::Approx(820.0));
	CHECK(linha1texto->rest_bbox.width == doctest::Approx(1036.0));
	CHECK(linha1texto->rest_bbox.height == doctest::Approx(84.0));

	CHECK(logo->rest_bbox.x == doctest::Approx(92.0));
	CHECK(logo->rest_bbox.y == doctest::Approx(812.0));
	CHECK(logo->rest_bbox.width == doctest::Approx(156.0));
	CHECK(logo->rest_bbox.height == doctest::Approx(156.0));

	CHECK(tag_aovivo->rest_bbox.x == doctest::Approx(96.0));
	CHECK(tag_aovivo->rest_bbox.y == doctest::Approx(772.0));
	CHECK(tag_aovivo->rest_bbox.width == doctest::Approx(148.0));
	CHECK(tag_aovivo->rest_bbox.height == doctest::Approx(36.0));

	CHECK(tag_dot->rest_bbox.x == doctest::Approx(113.0));
	CHECK(tag_dot->rest_bbox.y == doctest::Approx(783.0));
	CHECK(tag_dot->rest_bbox.width == doctest::Approx(14.0));
	CHECK(tag_dot->rest_bbox.height == doctest::Approx(14.0));

	CHECK(tag_txt->rest_bbox.x == doctest::Approx(138.0));
	CHECK(tag_txt->rest_bbox.y == doctest::Approx(772.0));
	CHECK(tag_txt->rest_bbox.width == doctest::Approx(98.0));
	CHECK(tag_txt->rest_bbox.height == doctest::Approx(36.0));

	// Rasterize `linha1` (#0b2a5b) with 2px AA padding -> 1104x88 at (258, 818)
	const naxgc::RasterizedBitmap bmp_linha1 = svg_rast.rasterize_layer(*linha1);
	REQUIRE(!bmp_linha1.empty());
	CHECK(bmp_linha1.width == 1104);
	CHECK(bmp_linha1.height == 88);
	CHECK(bmp_linha1.canvas_rect.x == doctest::Approx(258.0));
	CHECK(bmp_linha1.canvas_rect.y == doctest::Approx(818.0));

	const BgraPixel center_blue = sample_pixel(bmp_linha1, bmp_linha1.width / 2, bmp_linha1.height / 2);
	CHECK(center_blue.r == 0x0b);
	CHECK(center_blue.g == 0x2a);
	CHECK(center_blue.b == 0x5b);
	CHECK(center_blue.a == 0xff);

	// Change `linha1.fill` to `#b00020` (tema_vermelho) and verify re-rasterized color
	REQUIRE(scene.apply_state("tema_vermelho").is_ok());
	const naxgc::RasterizedBitmap bmp_linha1_red = svg_rast.rasterize_layer(*scene.find_layer("linha1"));
	REQUIRE(!bmp_linha1_red.empty());
	const BgraPixel center_red = sample_pixel(bmp_linha1_red, bmp_linha1_red.width / 2, bmp_linha1_red.height / 2);
	CHECK(center_red.r == 0xb0);
	CHECK(center_red.g == 0x00);
	CHECK(center_red.b == 0x20);
	CHECK(center_red.a == 0xff);

	// Verify child layer isolation: `tag_aovivo` must NOT bake in the white circle `tag_aovivo_dot` at (120, 790)
	const naxgc::RasterizedBitmap bmp_tag = svg_rast.rasterize_layer(*tag_aovivo);
	REQUIRE(!bmp_tag.empty());
	const uint32_t dot_x_in_tag = static_cast<uint32_t>(120.0 - bmp_tag.canvas_rect.x);
	const uint32_t dot_y_in_tag = static_cast<uint32_t>(790.0 - bmp_tag.canvas_rect.y);
	const BgraPixel tag_at_dot = sample_pixel(bmp_tag, dot_x_in_tag, dot_y_in_tag);
	// Should be the red pill background (#d00000), not the white dot (#ffffff)
	CHECK(tag_at_dot.r == 0xd0);
	CHECK(tag_at_dot.g == 0x00);
	CHECK(tag_at_dot.b == 0x00);
	CHECK(tag_at_dot.a == 0xff);

	// And `tag_aovivo_dot` itself rasterizes the white circle
	const naxgc::RasterizedBitmap bmp_dot = svg_rast.rasterize_layer(*tag_dot);
	REQUIRE(!bmp_dot.empty());
	const BgraPixel dot_center = sample_pixel(bmp_dot, bmp_dot.width / 2, bmp_dot.height / 2);
	CHECK(dot_center.r == 0xff);
	CHECK(dot_center.g == 0xff);
	CHECK(dot_center.b == 0xff);
	CHECK(dot_center.a == 0xff);
}

TEST_CASE("text_rasterizer_renders_package_font_and_overflow_modes")
{
	const std::filesystem::path tpl_dir = source_root() / "template";
	const naxgc::TemplateLoadResult res = naxgc::load_template_package(tpl_dir);
	REQUIRE(res.is_ok());
	REQUIRE(res.loaded.has_value());

	naxgc::Scene scene = naxgc::Scene::from_template(*res.loaded, false);
	const naxgc::SvgRasterizer svg_rast(*res.loaded);
	svg_rast.populate_scene_rest_bboxes(scene);
	const naxgc::TextRasterizer txt_rast(*res.loaded);

	// 1. `tag_aovivo_txt` has static text "AO VIVO" -> non-empty 98x36 bitmap
	const naxgc::SceneLayer *tag_txt = scene.find_layer("tag_aovivo_txt");
	REQUIRE(tag_txt != nullptr);
	const naxgc::RasterizedBitmap bmp_aovivo = txt_rast.rasterize_layer(*tag_txt);
	REQUIRE(!bmp_aovivo.empty());
	CHECK(bmp_aovivo.width == 98);
	CHECK(bmp_aovivo.height == 36);
	CHECK(count_non_transparent_pixels(bmp_aovivo) > 20);

	// 2. `linha1texto` starts with empty text "" -> empty bitmap
	const naxgc::SceneLayer *linha1texto = scene.find_layer("linha1texto");
	REQUIRE(linha1texto != nullptr);
	const naxgc::RasterizedBitmap bmp_empty = txt_rast.rasterize_layer(*linha1texto);
	CHECK(bmp_empty.empty());

	// 3. Setting `linha1texto.text` renders into 1036x84 frame
	REQUIRE(scene.set_property_base("layers.linha1texto.text", std::string("Maria Fernanda Oliveira")).is_ok());
	const naxgc::RasterizedBitmap bmp_name = txt_rast.rasterize_layer(*scene.find_layer("linha1texto"));
	REQUIRE(!bmp_name.empty());
	CHECK(bmp_name.width == 1036);
	CHECK(bmp_name.height == 84);
	CHECK(count_non_transparent_pixels(bmp_name) > 200);

	// 4. Test overflow modes: shrink, ellipsis, wrap, clip
	naxgc::SceneLayer test_layer = *scene.find_layer("linha1texto");
	test_layer.rest_bbox = naxgc::Rect2D{0.0, 0.0, 220.0, 80.0};
	test_layer.props.text.set_base(
		"Texto muito longo para testar o comportamento de overflow em quadros estreitos");

	test_layer.overflow = naxgc::TextOverflow::Shrink;
	test_layer.min_size = 16.0;
	const naxgc::RasterizedBitmap bmp_shrink = txt_rast.rasterize_layer(test_layer);
	CHECK(!bmp_shrink.empty());
	CHECK(count_non_transparent_pixels(bmp_shrink) > 50);

	test_layer.overflow = naxgc::TextOverflow::Ellipsis;
	const naxgc::RasterizedBitmap bmp_ellipsis = txt_rast.rasterize_layer(test_layer);
	CHECK(!bmp_ellipsis.empty());
	CHECK(count_non_transparent_pixels(bmp_ellipsis) > 50);

	test_layer.overflow = naxgc::TextOverflow::Wrap;
	test_layer.max_lines = 2;
	const naxgc::RasterizedBitmap bmp_wrap = txt_rast.rasterize_layer(test_layer);
	CHECK(!bmp_wrap.empty());
	CHECK(count_non_transparent_pixels(bmp_wrap) > count_non_transparent_pixels(bmp_ellipsis));

	test_layer.overflow = naxgc::TextOverflow::Clip;
	const naxgc::RasterizedBitmap bmp_clip = txt_rast.rasterize_layer(test_layer);
	CHECK(!bmp_clip.empty());
	CHECK(count_non_transparent_pixels(bmp_clip) > 50);
}
