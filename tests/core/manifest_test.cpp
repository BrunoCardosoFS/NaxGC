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

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "core/manifest/manifest_parser.hpp"
#include "core/manifest/manifest_validator.hpp"
#include "core/util/color.hpp"
#include "core/util/safe_path.hpp"

namespace {

std::filesystem::path source_root()
{
	return std::filesystem::u8path(NAXGC_SOURCE_DIR);
}

struct FixtureRunResult {
	std::optional<naxgc::Manifest> manifest;
	std::vector<naxgc::Diagnostic> diagnostics;

	bool has_code(std::string_view code, std::string_view expected_path = "") const
	{
		for (const auto &d : diagnostics) {
			if (d.code == code) {
				if (expected_path.empty() || d.json_path == expected_path) {
					return true;
				}
			}
		}
		return false;
	}

	bool has_errors() const
	{
		for (const auto &d : diagnostics) {
			if (d.severity == naxgc::DiagnosticSeverity::Error) {
				return true;
			}
		}
		return false;
	}
};

FixtureRunResult run_fixture_file(const std::filesystem::path &fixture_dir, std::string_view filename)
{
	FixtureRunResult out;
	auto content = naxgc::read_package_file_bounded(fixture_dir, filename, naxgc::kMaxManifestBytes, "NXG-E001");
	if (content.is_err()) {
		out.diagnostics.push_back(naxgc::Diagnostic{naxgc::DiagnosticSeverity::Error, content.error().code, "",
							    content.error().message});
		return out;
	}

	out.manifest = naxgc::parse_manifest_json(content.value(), out.diagnostics);
	if (!out.manifest.has_value()) {
		return out;
	}

	std::optional<naxgc::SvgInspectionResult> svg_info;
	std::string svg_content;
	naxgc::validate_manifest_package(*out.manifest, fixture_dir, svg_info, svg_content, out.diagnostics);
	return out;
}

} // namespace

TEST_CASE("reference_template_validates_without_errors_or_warnings")
{
	const std::filesystem::path tpl_dir = source_root() / "template";
	const naxgc::TemplateLoadResult res = naxgc::load_template_package(tpl_dir);

	if (!res.diagnostics.empty()) {
		for (const auto &d : res.diagnostics) {
			MESSAGE(d.format());
		}
	}

	CHECK(res.is_ok());
	CHECK(res.errors().empty());
	CHECK(res.warnings().empty());
	REQUIRE(res.loaded.has_value());

	const naxgc::Manifest &m = res.loaded->manifest;
	CHECK(m.schema == 1);
	CHECK(m.meta.id == "naxgc.default.lower-third");
	CHECK(m.canvas.width == 1920);
	CHECK(m.canvas.height == 1080);
	CHECK(m.layers.size() == 12);
	CHECK(m.states.size() == 3);
	CHECK(m.animations.size() == 17);
	CHECK(m.actions.size() == 2);
	REQUIRE(m.panel.has_value());
	CHECK(m.panel->sections.size() == 3);
}

TEST_CASE("valid_manifest_fixtures_load_without_errors")
{
	const std::filesystem::path valid_dir = source_root() / "tests" / "fixtures" / "manifests" / "valid";

	SUBCASE("minimal.json")
	{
		const FixtureRunResult res = run_fixture_file(valid_dir, "minimal.json");
		CHECK_FALSE(res.has_errors());
		REQUIRE(res.manifest.has_value());
		CHECK(res.manifest->meta.id == "exemplo.minimo");
		CHECK(res.manifest->layers.size() == 1);
	}

	SUBCASE("complete.json")
	{
		const FixtureRunResult res = run_fixture_file(valid_dir, "complete.json");
		CHECK_FALSE(res.has_errors());
		CHECK(res.diagnostics.empty());
		REQUIRE(res.manifest.has_value());
		CHECK(res.manifest->layers.size() == 5);
		CHECK(res.manifest->animations.size() == 4);
		CHECK(res.manifest->actions.size() == 3);
	}
}

TEST_CASE("invalid_manifest_fixtures_report_expected_error_codes_and_paths")
{
	const std::filesystem::path invalid_dir = source_root() / "tests" / "fixtures" / "manifests" / "invalid";

	struct ExpectedCase {
		const char *file;
		const char *code;
		const char *json_path;
	};

	const ExpectedCase cases[] = {
		{"e001_invalid_json.json", "NXG-E001", ""},
		{"e002_unsupported_schema.json", "NXG-E002", "schema"},
		{"e003_unsafe_path.json", "NXG-E003", "svg"},
		{"e004_invalid_svg.json", "NXG-E004", "svg"},
		{"e005_missing_required.json", "NXG-E005", "meta.name"},
		{"e010_missing_svg_id.json", "NXG-E010", "layers.barra.id"},
		{"e011_duplicate_layer_id.json", "NXG-E011", "layers.barra_b.id"},
		{"e012_invalid_mask.json", "NXG-E012", "layers.titulo.mask"},
		{"e013_unknown_layer_type.json", "NXG-E013", "layers.barra.type"},
		{"e020_missing_clip.json", "NXG-E020", "animations.main.in.triggers[0].play"},
		{"e021_trigger_cycle.json", "NXG-E021", "animations.clip_b.triggers[0]"},
		{"e022_track_missing_layer.json", "NXG-E022", "animations.in.tracks[0].layer"},
		{"e023_incompatible_prop.json", "NXG-E023", "animations.in.tracks[0].prop"},
		{"e024_invalid_keys.json", "NXG-E024", "animations.in.tracks[0].keys"},
		{"e025_missing_marker.json", "NXG-E025", "animations.pai.triggers[0].on"},
		{"e030_invalid_bind.json", "NXG-E030", "panel.sections[0].controls[0].bind"},
		{"e031_duplicate_control_key.json", "NXG-E031", "panel.sections[0].controls[1].key"},
		{"e032_missing_action.json", "NXG-E032", "panel.sections[0].controls[0].action"},
		{"e033_incompatible_default.json", "NXG-E033", "panel.sections[0].controls[0].default"},
		{"e040_action_missing_clip.json", "NXG-E040", "actions.show[0]"},
		{"e041_action_call_cycle.json", "NXG-E041", "actions.acao_b[0]"},
		{"e050_missing_initial_state.json", "NXG-E050", "initialState"},
		{"e060_trigger_depth_exceeded.json", "NXG-E060", "animations.c00.triggers"},
	};

	for (const auto &tc : cases) {
		CAPTURE(tc.file);
		CAPTURE(tc.code);
		CAPTURE(tc.json_path);
		const FixtureRunResult res = run_fixture_file(invalid_dir, tc.file);
		CHECK(res.has_errors());
		CHECK(res.has_code(tc.code, tc.json_path));
	}
}

TEST_CASE("manifest_warnings_are_reported_without_blocking_load")
{
	const std::filesystem::path valid_dir = source_root() / "tests" / "fixtures" / "manifests" / "valid";

	const char *json_with_warnings = R"({
		"schema": 1,
		"campoDesconhecido": 123,
		"meta": { "id": "test.warnings", "name": "Warnings" },
		"canvas": { "width": 1280, "height": 720 },
		"svg": "layout.svg",
		"fonts": [
			{ "family": "MissingFont", "file": "fonts/NaoExiste.ttf" }
		],
		"layers": {
			"barra": { "type": "svg", "id": "barra" },
			"titulo": {
				"type": "text",
				"id": "txt_titulo",
				"size": 60,
				"overflow": "clip"
			},
			"dot_unused": { "type": "svg", "id": "dot" }
		},
		"panel": {
			"sections": [
				{
					"title": "Texto",
					"controls": [
						{
							"key": "t1",
							"type": "text",
							"bind": "layers.titulo.text",
							"maxLength": 200
						}
					]
				}
			]
		},
		"animations": {
			"in": {
				"tracks": [
					{ "layer": "barra", "prop": "x", "from": -500, "to": 0, "dur": 0.4 }
				]
			}
		}
	})";

	FixtureRunResult res;
	res.manifest = naxgc::parse_manifest_json(json_with_warnings, res.diagnostics);
	REQUIRE(res.manifest.has_value());

	std::optional<naxgc::SvgInspectionResult> svg_info;
	std::string svg_content;
	naxgc::validate_manifest_package(*res.manifest, valid_dir, svg_info, svg_content, res.diagnostics);

	CHECK_FALSE(res.has_errors());
	CHECK(res.has_code("NXG-W001", "svg"));
	CHECK(res.has_code("NXG-W002", "fonts[0].file"));
	CHECK(res.has_code("NXG-W003", "panel.sections[0].controls[0].maxLength"));
	CHECK(res.has_code("NXG-W004", "campoDesconhecido"));
	CHECK(res.has_code("NXG-W005", "canvas"));
	CHECK(res.has_code("NXG-W006", "layers.dot_unused"));
}

TEST_CASE("safe_path_rejects_traversal_and_absolute_paths")
{
	const std::filesystem::path tpl_dir = source_root() / "template";

	CHECK(naxgc::is_safe_relative_path_syntax("layout.svg"));
	CHECK(naxgc::is_safe_relative_path_syntax("fonts/Inter.ttf"));

	CHECK_FALSE(naxgc::is_safe_relative_path_syntax(""));
	CHECK_FALSE(naxgc::is_safe_relative_path_syntax("../manifest.json"));
	CHECK_FALSE(naxgc::is_safe_relative_path_syntax("fonts/../../secret.txt"));
	CHECK_FALSE(naxgc::is_safe_relative_path_syntax("/etc/passwd"));
	CHECK_FALSE(naxgc::is_safe_relative_path_syntax("\\Windows\\System32"));
	CHECK_FALSE(naxgc::is_safe_relative_path_syntax("C:/Windows/win.ini"));
	CHECK_FALSE(naxgc::is_safe_relative_path_syntax("fonts\\Inter.ttf"));

	auto resolved_ok = naxgc::resolve_package_path(tpl_dir, "layout.svg");
	CHECK(resolved_ok.is_ok());

	auto resolved_bad = naxgc::resolve_package_path(tpl_dir, "../README.md");
	REQUIRE(resolved_bad.is_err());
	CHECK(resolved_bad.error().code == "NXG-E003");
	// Ensure host absolute path is not leaked in the error message
	CHECK(resolved_bad.error().message.find(tpl_dir.string()) == std::string::npos);
}

TEST_CASE("color_parser_supports_rgb_rrggbb_and_rrggbbaa")
{
	auto c3 = naxgc::parse_color("#f0a");
	REQUIRE(c3.is_ok());
	CHECK(c3.value() == naxgc::Color{0xFF, 0x00, 0xAA, 0xFF});

	auto c6 = naxgc::parse_color("#0b2a5b");
	REQUIRE(c6.is_ok());
	CHECK(c6.value() == naxgc::Color{0x0B, 0x2A, 0x5B, 0xFF});
	CHECK(c6.value().to_hex_string() == "#0b2a5b");

	auto c8 = naxgc::parse_color("#ffd23f80");
	REQUIRE(c8.is_ok());
	CHECK(c8.value() == naxgc::Color{0xFF, 0xD2, 0x3F, 0x80});
	CHECK(c8.value().to_hex_string() == "#ffd23f80");

	CHECK(naxgc::parse_color("0b2a5b").is_err());
	CHECK(naxgc::parse_color("#12345").is_err());
	CHECK(naxgc::parse_color("#zzzzzz").is_err());
}

TEST_CASE("json_schema_file_is_present_and_valid_json")
{
	const std::filesystem::path schema_dir = source_root() / "schemas";
	auto content = naxgc::read_package_file_bounded(schema_dir, "manifest.schema.json", naxgc::kMaxManifestBytes,
							"NXG-E001");
	REQUIRE(content.is_ok());
	CHECK(naxgc::is_valid_utf8(content.value()));
	CHECK(content.value().find("https://json-schema.org/draft/2020-12/schema") != std::string::npos);
}
