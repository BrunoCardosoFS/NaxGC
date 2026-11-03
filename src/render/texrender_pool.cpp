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

#include "render/texrender_pool.hpp"

#include <utility>

#include <obs.h>

namespace naxgc {

TexrenderPool::~TexrenderPool()
{
	clear();
}

TexrenderPool::TexrenderPool(TexrenderPool &&other) noexcept
	: pool_(std::move(other.pool_)),
	  in_use_count_(other.in_use_count_)
{
	other.in_use_count_ = 0;
}

TexrenderPool &TexrenderPool::operator=(TexrenderPool &&other) noexcept
{
	if (this != &other) {
		clear();
		pool_ = std::move(other.pool_);
		in_use_count_ = other.in_use_count_;
		other.in_use_count_ = 0;
	}
	return *this;
}

gs_texrender_t *TexrenderPool::acquire(enum gs_color_format format)
{
	if (in_use_count_ < pool_.size()) {
		Entry &entry = pool_[in_use_count_];
		if (entry.format != format || entry.texrender == nullptr) {
			if (entry.texrender != nullptr) {
				gs_texrender_destroy(entry.texrender);
			}
			entry.texrender = gs_texrender_create(format, GS_ZS_NONE);
			entry.format = format;
		}
		++in_use_count_;
		return entry.texrender;
	}

	gs_texrender_t *new_tr = gs_texrender_create(format, GS_ZS_NONE);
	if (new_tr == nullptr) {
		blog(LOG_ERROR, "[NaxGC] Failed to allocate gs_texrender_t in TexrenderPool");
		return nullptr;
	}

	pool_.push_back(Entry{new_tr, format});
	++in_use_count_;
	return new_tr;
}

void TexrenderPool::release_all() noexcept
{
	for (auto &entry : pool_) {
		if (entry.texrender != nullptr) {
			gs_texrender_reset(entry.texrender);
		}
	}
	in_use_count_ = 0;
}

void TexrenderPool::clear() noexcept
{
	bool has_any = false;
	for (const auto &entry : pool_) {
		if (entry.texrender != nullptr) {
			has_any = true;
			break;
		}
	}

	if (has_any) {
		obs_enter_graphics();
		for (auto &entry : pool_) {
			if (entry.texrender != nullptr) {
				gs_texrender_destroy(entry.texrender);
				entry.texrender = nullptr;
			}
		}
		obs_leave_graphics();
	}

	pool_.clear();
	in_use_count_ = 0;
}

} // namespace naxgc
