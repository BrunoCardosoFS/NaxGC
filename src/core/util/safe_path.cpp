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

#include "core/util/safe_path.hpp"

#include <fstream>
#include <system_error>

namespace naxgc {

namespace {

bool is_subpath_of(const std::filesystem::path &child, const std::filesystem::path &parent)
{
	auto p_it = parent.begin();
	auto c_it = child.begin();
	for (; p_it != parent.end(); ++p_it, ++c_it) {
		if (c_it == child.end() || *c_it != *p_it) {
			return false;
		}
	}
	return true;
}

} // namespace

bool is_safe_relative_path_syntax(std::string_view relative_path) noexcept
{
	if (relative_path.empty()) {
		return false;
	}

	if (relative_path.find('\0') != std::string_view::npos) {
		return false;
	}

	if (relative_path[0] == '/' || relative_path[0] == '\\') {
		return false;
	}

	if (relative_path.find(':') != std::string_view::npos || relative_path.find('\\') != std::string_view::npos) {
		return false;
	}

	// Reject backslash separators or '..' components
	std::size_t start = 0;
	while (start <= relative_path.size()) {
		std::size_t sep = relative_path.find_first_of("/\\", start);
		std::string_view segment = (sep == std::string_view::npos) ? relative_path.substr(start)
									   : relative_path.substr(start, sep - start);
		if (segment == ".." || segment == "." || segment.empty()) {
			return false;
		}
		if (sep == std::string_view::npos) {
			break;
		}
		start = sep + 1;
	}

	const std::filesystem::path rel = std::filesystem::u8path(relative_path.begin(), relative_path.end());
	if (rel.is_absolute() || rel.has_root_name() || rel.has_root_directory()) {
		return false;
	}

	return true;
}

Result<std::filesystem::path> resolve_package_path(const std::filesystem::path &package_root,
						   std::string_view relative_path)
{
	if (!is_safe_relative_path_syntax(relative_path)) {
		return Result<std::filesystem::path>::err(
			Error{"NXG-E003", "unsafe relative path '" + std::string(relative_path) + "'"});
	}

	std::error_code ec;
	const std::filesystem::path canonical_root = std::filesystem::canonical(package_root, ec);
	if (ec) {
		return Result<std::filesystem::path>::err(Error{"NXG-E003", "template directory does not exist"});
	}

	const std::filesystem::path rel = std::filesystem::u8path(relative_path.begin(), relative_path.end());
	const std::filesystem::path combined = canonical_root / rel;

	// Check every prefix component for symlinks escaping the root
	std::filesystem::path current = canonical_root;
	for (const auto &part : rel) {
		current /= part;
		std::error_code sym_ec;
		if (std::filesystem::is_symlink(current, sym_ec)) {
			const std::filesystem::path resolved_sym = std::filesystem::canonical(current, sym_ec);
			if (sym_ec || !is_subpath_of(resolved_sym, canonical_root)) {
				return Result<std::filesystem::path>::err(
					Error{"NXG-E003", "symlink escapes package directory: '" +
								  std::string(relative_path) + "'"});
			}
		}
	}

	const std::filesystem::path weak_target = std::filesystem::weakly_canonical(combined, ec);
	if (ec || !is_subpath_of(weak_target, canonical_root)) {
		return Result<std::filesystem::path>::err(
			Error{"NXG-E003", "path escapes package directory: '" + std::string(relative_path) + "'"});
	}

	return Result<std::filesystem::path>::ok(weak_target);
}

Result<std::string> read_package_file_bounded(const std::filesystem::path &package_root, std::string_view relative_path,
					      std::size_t max_bytes, const char *oversize_error_code)
{
	auto resolved = resolve_package_path(package_root, relative_path);
	if (resolved.is_err()) {
		return Result<std::string>::err(resolved.error());
	}

	const std::filesystem::path &full_path = resolved.value();
	std::error_code ec;
	if (!std::filesystem::exists(full_path, ec) || ec) {
		return Result<std::string>::err(
			Error{"NXG-E003", "file not found in package: '" + std::string(relative_path) + "'"});
	}

	if (!std::filesystem::is_regular_file(full_path, ec) || ec) {
		return Result<std::string>::err(
			Error{"NXG-E003", "not a regular file: '" + std::string(relative_path) + "'"});
	}

	const auto file_size = std::filesystem::file_size(full_path, ec);
	if (ec) {
		return Result<std::string>::err(
			Error{"NXG-E003", "failed to query file size: '" + std::string(relative_path) + "'"});
	}

	if (file_size > max_bytes) {
		return Result<std::string>::err(Error{oversize_error_code, "file exceeds maximum allowed size: '" +
										   std::string(relative_path) + "'"});
	}

	std::ifstream file(full_path, std::ios::binary);
	if (!file) {
		return Result<std::string>::err(
			Error{"NXG-E003", "failed to open file: '" + std::string(relative_path) + "'"});
	}

	std::string content(static_cast<std::size_t>(file_size), '\0');
	if (file_size > 0) {
		file.read(&content[0], static_cast<std::streamsize>(file_size));
		if (!file || static_cast<std::uintmax_t>(file.gcount()) != file_size) {
			return Result<std::string>::err(
				Error{"NXG-E003", "failed to read file: '" + std::string(relative_path) + "'"});
		}
	}

	return Result<std::string>::ok(std::move(content));
}

AssetCheckResult check_package_asset(const std::filesystem::path &package_root, std::string_view relative_path,
				     std::size_t max_bytes)
{
	auto resolved = resolve_package_path(package_root, relative_path);
	if (resolved.is_err()) {
		return AssetCheckResult{AssetCheckStatus::UnsafeOrOversize, resolved.error().message};
	}

	const std::filesystem::path &full_path = resolved.value();
	std::error_code ec;
	if (!std::filesystem::exists(full_path, ec) || ec) {
		return AssetCheckResult{AssetCheckStatus::Missing,
					"asset file not found in package: '" + std::string(relative_path) + "'"};
	}

	if (!std::filesystem::is_regular_file(full_path, ec) || ec) {
		return AssetCheckResult{AssetCheckStatus::UnsafeOrOversize,
					"asset path is not a regular file: '" + std::string(relative_path) + "'"};
	}

	const auto file_size = std::filesystem::file_size(full_path, ec);
	if (ec || file_size > max_bytes) {
		return AssetCheckResult{AssetCheckStatus::UnsafeOrOversize,
					"asset file exceeds maximum size limit: '" + std::string(relative_path) + "'"};
	}

	return AssetCheckResult{AssetCheckStatus::Ok, {}};
}

bool is_valid_utf8(std::string_view data) noexcept
{
	std::size_t i = 0;
	const std::size_t len = data.size();
	while (i < len) {
		const auto b0 = static_cast<unsigned char>(data[i]);
		if (b0 <= 0x7F) {
			++i;
		} else if ((b0 & 0xE0) == 0xC0) {
			if (b0 < 0xC2 || i + 1 >= len) {
				return false;
			}
			const auto b1 = static_cast<unsigned char>(data[i + 1]);
			if ((b1 & 0xC0) != 0x80) {
				return false;
			}
			i += 2;
		} else if ((b0 & 0xF0) == 0xE0) {
			if (i + 2 >= len) {
				return false;
			}
			const auto b1 = static_cast<unsigned char>(data[i + 1]);
			const auto b2 = static_cast<unsigned char>(data[i + 2]);
			if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) {
				return false;
			}
			if (b0 == 0xE0 && b1 < 0xA0) {
				return false; // Overlong
			}
			if (b0 == 0xED && b1 >= 0xA0) {
				return false; // UTF-16 surrogate halves
			}
			i += 3;
		} else if ((b0 & 0xF8) == 0xF0) {
			if (b0 > 0xF4 || i + 3 >= len) {
				return false;
			}
			const auto b1 = static_cast<unsigned char>(data[i + 1]);
			const auto b2 = static_cast<unsigned char>(data[i + 2]);
			const auto b3 = static_cast<unsigned char>(data[i + 3]);
			if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) {
				return false;
			}
			if (b0 == 0xF0 && b1 < 0x90) {
				return false; // Overlong
			}
			if (b0 == 0xF4 && b1 >= 0x90) {
				return false; // > U+10FFFF
			}
			i += 4;
		} else {
			return false;
		}
	}
	return true;
}

} // namespace naxgc
