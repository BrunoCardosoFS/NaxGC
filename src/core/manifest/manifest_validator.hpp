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

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "core/manifest/manifest_types.hpp"
#include "core/manifest/svg_inspector.hpp"
#include "core/util/result.hpp"

namespace naxgc {

struct LoadedTemplate {
	Manifest manifest;
	std::string svg_content;
	SvgInspectionResult svg_info;
};

struct TemplateLoadResult {
	std::optional<LoadedTemplate> loaded;
	std::vector<Diagnostic> diagnostics;

	bool is_ok() const noexcept
	{
		if (!loaded.has_value()) {
			return false;
		}
		for (const auto &d : diagnostics) {
			if (d.severity == DiagnosticSeverity::Error) {
				return false;
			}
		}
		return true;
	}

	std::vector<Diagnostic> errors() const
	{
		std::vector<Diagnostic> out;
		for (const auto &d : diagnostics) {
			if (d.severity == DiagnosticSeverity::Error) {
				out.push_back(d);
			}
		}
		return out;
	}

	std::vector<Diagnostic> warnings() const
	{
		std::vector<Diagnostic> out;
		for (const auto &d : diagnostics) {
			if (d.severity == DiagnosticSeverity::Warning) {
				out.push_back(d);
			}
		}
		return out;
	}
};

/**
 * Validates an already-parsed Manifest against its package directory and SVG content.
 * Appends all errors (NXG-E0xx) and warnings (NXG-W0xx) to `diagnostics`.
 */
void validate_manifest_package(Manifest &manifest, const std::filesystem::path &package_root,
			       std::optional<SvgInspectionResult> &out_svg_info, std::string &out_svg_content,
			       std::vector<Diagnostic> &diagnostics);

/**
 * High-level entry point to load and validate a template directory on disk.
 * Never throws exceptions and never leaks absolute host paths in diagnostics.
 */
TemplateLoadResult load_template_package(const std::filesystem::path &package_root);

} // namespace naxgc
