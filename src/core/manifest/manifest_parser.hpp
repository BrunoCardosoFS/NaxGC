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

#include <optional>
#include <string_view>
#include <vector>

#include "core/manifest/manifest_types.hpp"
#include "core/util/result.hpp"

namespace naxgc {

/**
 * Parses a JSON string into the typed Manifest structure.
 * Appends syntax, structural, and unknown-field diagnostics to `diagnostics`.
 * Returns std::nullopt only if the input cannot be parsed as a JSON object at all.
 */
std::optional<Manifest> parse_manifest_json(std::string_view json_text, std::vector<Diagnostic> &diagnostics);

/**
 * Validates whether an identifier follows [A-Za-z0-9_.-]+ (for layers, clips, states, actions, keys).
 */
bool is_valid_identifier(std::string_view name) noexcept;

/**
 * Validates whether a template meta.id follows [a-z0-9.-]+.
 */
bool is_valid_meta_id(std::string_view id) noexcept;

} // namespace naxgc
