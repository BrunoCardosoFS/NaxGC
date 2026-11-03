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

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

#include "core/util/result.hpp"

namespace naxgc {

/**
 * Validates that a relative path inside a template package is safe (not empty,
 * not absolute, contains no '..' segments, no drive letters, and does not
 * escape the package root via symlinks).
 *
 * Returns the resolved filesystem path on success, or an Error with code
 * NXG-E003 (never leaking absolute host paths in the error message).
 */
Result<std::filesystem::path> resolve_package_path(const std::filesystem::path &package_root,
						   std::string_view relative_path);

/**
 * Checks whether a relative path syntax is safe without touching the filesystem.
 */
bool is_safe_relative_path_syntax(std::string_view relative_path) noexcept;

/**
 * Reads an existing file inside the package up to max_bytes.
 * Fails with NXG-E003 if the file is missing, is not a regular file, or exceeds max_bytes.
 */
Result<std::string> read_package_file_bounded(const std::filesystem::path &package_root, std::string_view relative_path,
					      std::size_t max_bytes, const char *oversize_error_code = "NXG-E003");

/**
 * Verifies that a package asset file (such as an image or font) exists, is within the package,
 * and does not exceed max_bytes.
 */
enum class AssetCheckStatus {
	Ok,
	Missing,
	UnsafeOrOversize,
};

struct AssetCheckResult {
	AssetCheckStatus status{AssetCheckStatus::Ok};
	std::string message;
};

AssetCheckResult check_package_asset(const std::filesystem::path &package_root, std::string_view relative_path,
				     std::size_t max_bytes);

/**
 * Checks whether a byte buffer is valid UTF-8.
 */
bool is_valid_utf8(std::string_view data) noexcept;

} // namespace naxgc
