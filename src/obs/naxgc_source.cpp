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

#include "naxgc_source.hpp"

#include <cmath>
#include <memory>
#include <string>

#include <graphics/graphics.h>
#include <graphics/matrix4.h>
#include <graphics/vec4.h>
#include <obs-module.h>

namespace naxgc {

namespace {

constexpr float kDefaultWidth = 1920.0f;
constexpr float kDefaultHeight = 1080.0f;
constexpr float kRectWidth = 320.0f;
constexpr float kRectHeight = 180.0f;
constexpr float kMarginX = 100.0f;

constexpr double kMoveDuration = 1.2;  // Seconds to cross from one side to the other
constexpr double kPauseDuration = 0.3; // Seconds to stop at each side
constexpr double kCycleDuration = (kMoveDuration + kPauseDuration) * 2.0;

/**
 * Standard cubic ease-in-out function.
 * Smooth acceleration at start and deceleration at end.
 */
inline double ease_in_out(double t) noexcept
{
	if (t <= 0.0) {
		return 0.0;
	}
	if (t >= 1.0) {
		return 1.0;
	}
	return (t < 0.5) ? (4.0 * t * t * t) : (1.0 - std::pow(-2.0 * t + 2.0, 3.0) / 2.0);
}

} // namespace

struct NaxgcSource {
	obs_source_t *source{nullptr};

	uint32_t width{static_cast<uint32_t>(kDefaultWidth)};
	uint32_t height{static_cast<uint32_t>(kDefaultHeight)};

	std::string template_path;
	bool reset_on_show{false};

	double anim_time{0.0};
	bool is_showing{true};

	explicit NaxgcSource(obs_source_t *src) : source(src) {}
};

static const char *naxgc_source_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return "NaxGC";
}

static void naxgc_source_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "template_path", "");
	obs_data_set_default_int(settings, "width", static_cast<long long>(kDefaultWidth));
	obs_data_set_default_int(settings, "height", static_cast<long long>(kDefaultHeight));
	obs_data_set_default_bool(settings, "reset_on_show", false);
}

static obs_properties_t *naxgc_source_get_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_path(props, "template_path", "Template Folder", OBS_PATH_DIRECTORY, nullptr, nullptr);
	obs_properties_add_int(props, "width", "Width", 100, 7680, 1);
	obs_properties_add_int(props, "height", "Height", 100, 4320, 1);
	obs_properties_add_bool(props, "reset_on_show", "Reset animation on show");

	return props;
}

static void naxgc_source_update(void *data, obs_data_t *settings)
{
	auto *context = static_cast<NaxgcSource *>(data);
	if (!context || !settings) {
		return;
	}

	const char *path = obs_data_get_string(settings, "template_path");
	context->template_path = path ? path : "";

	const auto w = static_cast<uint32_t>(obs_data_get_int(settings, "width"));
	const auto h = static_cast<uint32_t>(obs_data_get_int(settings, "height"));
	context->width = (w > 0) ? w : static_cast<uint32_t>(kDefaultWidth);
	context->height = (h > 0) ? h : static_cast<uint32_t>(kDefaultHeight);

	context->reset_on_show = obs_data_get_bool(settings, "reset_on_show");

	blog(LOG_INFO, "[NaxGC] Source updated: %ux%u, template='%s', reset_on_show=%s", context->width,
	     context->height, context->template_path.c_str(), context->reset_on_show ? "true" : "false");
}

static void *naxgc_source_create(obs_data_t *settings, obs_source_t *source)
{
	auto context = std::make_unique<NaxgcSource>(source);
	naxgc_source_update(context.get(), settings);

	blog(LOG_INFO, "[NaxGC] Source created successfully");
	return context.release();
}

static void naxgc_source_destroy(void *data)
{
	auto *context = static_cast<NaxgcSource *>(data);
	if (context) {
		blog(LOG_INFO, "[NaxGC] Source destroyed");
		delete context;
	}
}

static uint32_t naxgc_source_get_width(void *data)
{
	auto *context = static_cast<NaxgcSource *>(data);
	return context ? context->width : static_cast<uint32_t>(kDefaultWidth);
}

static uint32_t naxgc_source_get_height(void *data)
{
	auto *context = static_cast<NaxgcSource *>(data);
	return context ? context->height : static_cast<uint32_t>(kDefaultHeight);
}

static void naxgc_source_show(void *data)
{
	auto *context = static_cast<NaxgcSource *>(data);
	if (!context) {
		return;
	}

	context->is_showing = true;
	if (context->reset_on_show) {
		context->anim_time = 0.0;
		blog(LOG_INFO, "[NaxGC] Source shown (reset_on_show: anim_time reset to 0)");
	}
}

static void naxgc_source_hide(void *data)
{
	auto *context = static_cast<NaxgcSource *>(data);
	if (!context) {
		return;
	}

	context->is_showing = false;
}

static void naxgc_source_video_tick(void *data, float seconds)
{
	auto *context = static_cast<NaxgcSource *>(data);
	if (!context) {
		return;
	}

	// Advance animation deterministically based exclusively on OBS video_tick elapsed seconds
	context->anim_time += static_cast<double>(seconds);
}

static void naxgc_source_video_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	auto *context = static_cast<NaxgcSource *>(data);
	if (!context) {
		return;
	}

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	if (!solid) {
		return;
	}

	const float canvas_w = static_cast<float>(context->width);
	const float canvas_h = static_cast<float>(context->height);

	const float start_x = kMarginX;
	const float end_x = (canvas_w > (kRectWidth + 2.0f * kMarginX)) ? (canvas_w - kRectWidth - kMarginX) : start_x;
	const float pos_y = (canvas_h - kRectHeight) * 0.5f;

	// Calculate position within the loop
	double t = std::fmod(context->anim_time, kCycleDuration);
	if (t < 0.0) {
		t += kCycleDuration;
	}

	float current_x = start_x;

	if (t < kMoveDuration) {
		// Phase 1: Left to right with ease-in-out
		const double progress = ease_in_out(t / kMoveDuration);
		current_x = start_x + static_cast<float>((end_x - start_x) * progress);
	} else if (t < (kMoveDuration + kPauseDuration)) {
		// Phase 2: Stopped at the right side
		current_x = end_x;
	} else if (t < (2.0 * kMoveDuration + kPauseDuration)) {
		// Phase 3: Right to left with ease-in-out
		const double rel_t = t - (kMoveDuration + kPauseDuration);
		const double progress = ease_in_out(rel_t / kMoveDuration);
		current_x = end_x - static_cast<float>((end_x - start_x) * progress);
	} else {
		// Phase 4: Stopped at the left side
		current_x = start_x;
	}

	// Linear / sRGB setup for color accuracy
	const bool linear_srgb = gs_get_linear_srgb();
	const bool prev_srgb = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(linear_srgb);

	gs_eparam_t *color_param = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	struct vec4 rect_color;
	// Vibrant modern broadcast blue (#1E88E5)
	vec4_set(&rect_color, 0.12f, 0.53f, 0.90f, 1.0f);
	gs_effect_set_vec4(color_param, &rect_color);

	gs_matrix_push();
	gs_matrix_translate3f(current_x, pos_y, 0.0f);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_draw_sprite(nullptr, 0, static_cast<uint32_t>(kRectWidth), static_cast<uint32_t>(kRectHeight));

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_matrix_pop();

	gs_enable_framebuffer_srgb(prev_srgb);
}

static struct obs_source_info init_source_info()
{
	struct obs_source_info info = {};
	info.id = "naxgc_source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_DO_NOT_DUPLICATE | OBS_SOURCE_SRGB;
	info.get_name = naxgc_source_get_name;
	info.create = naxgc_source_create;
	info.destroy = naxgc_source_destroy;
	info.get_width = naxgc_source_get_width;
	info.get_height = naxgc_source_get_height;
	info.get_defaults = naxgc_source_get_defaults;
	info.get_properties = naxgc_source_get_properties;
	info.update = naxgc_source_update;
	info.show = naxgc_source_show;
	info.hide = naxgc_source_hide;
	info.video_tick = naxgc_source_video_tick;
	info.video_render = naxgc_source_video_render;
	info.icon_type = OBS_ICON_TYPE_CUSTOM;
	return info;
}

struct obs_source_info naxgc_source_info = init_source_info();

} // namespace naxgc
