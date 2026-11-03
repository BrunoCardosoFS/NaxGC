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
#include <vector>

#include <graphics/graphics.h>

namespace naxgc {

/**
 * A pool of pre-allocated gs_texrender_t objects to avoid GPU surface allocations
 * and reallocations on each frame.
 */
class TexrenderPool {
public:
	TexrenderPool() = default;
	~TexrenderPool();

	TexrenderPool(const TexrenderPool &) = delete;
	TexrenderPool &operator=(const TexrenderPool &) = delete;

	TexrenderPool(TexrenderPool &&other) noexcept;
	TexrenderPool &operator=(TexrenderPool &&other) noexcept;

	/**
	 * Acquires a gs_texrender_t with the specified format (default GS_RGBA).
	 * If an available pooled instance matches, it is reused; if its format differs,
	 * it is recreated. If none is available, a new one is allocated.
	 * Must be called in the OBS graphics context.
	 */
	gs_texrender_t *acquire(enum gs_color_format format = GS_RGBA);

	/**
	 * Resets the rendered state (gs_texrender_reset) of all acquired texrenders and
	 * marks them available for reuse in subsequent passes or frames.
	 */
	void release_all() noexcept;

	/**
	 * Destroys all allocated texrenders and frees GPU resources.
	 * Can be called outside or inside graphics context (enters graphics context safely).
	 */
	void clear() noexcept;

	std::size_t total_count() const noexcept { return pool_.size(); }
	std::size_t in_use_count() const noexcept { return in_use_count_; }

private:
	struct Entry {
		gs_texrender_t *texrender{nullptr};
		enum gs_color_format format { GS_UNKNOWN };
	};

	std::vector<Entry> pool_;
	std::size_t in_use_count_{0};
};

} // namespace naxgc
