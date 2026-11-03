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

#include <exception>
#include <filesystem>
#include <memory>
#include <string>

#include <obs-module.h>

#include "core/anim/animator.hpp"
#include "core/manifest/manifest_validator.hpp"
#include "render/renderer.hpp"

namespace naxgc {

namespace {

constexpr uint32_t kDefaultWidth = 1920;
constexpr uint32_t kDefaultHeight = 1080;

struct StatusSummary {
	std::string text;
	enum obs_text_info_type info_type { OBS_TEXT_INFO_NORMAL };
};

StatusSummary build_status_summary(const TemplateLoadResult &res, const LoadedTemplate *fallback_template)
{
	StatusSummary summary;
	const auto errs = res.errors();
	const auto warns = res.warnings();

	if (res.is_ok() && res.loaded.has_value()) {
		const Manifest &m = res.loaded->manifest;
		summary.text = "Loaded: " + m.meta.name + " (" + m.meta.id + ") — " + std::to_string(m.canvas.width) +
			       "x" + std::to_string(m.canvas.height) + ", " + std::to_string(m.layers.size()) +
			       " layer(s), " + std::to_string(m.animations.size()) + " clip(s)";
		if (warns.empty()) {
			summary.info_type = OBS_TEXT_INFO_NORMAL;
		} else {
			summary.info_type = OBS_TEXT_INFO_WARNING;
			summary.text += "\nWarnings (" + std::to_string(warns.size()) + "):";
			for (const auto &w : warns) {
				summary.text += "\n• " + w.format();
			}
		}
		return summary;
	}

	summary.info_type = OBS_TEXT_INFO_ERROR;
	summary.text = "Template validation failed (" + std::to_string(errs.size()) + " error(s)):";
	for (const auto &e : errs) {
		summary.text += "\n• " + e.format();
	}
	if (fallback_template != nullptr) {
		summary.text +=
			"\n(Keeping previously loaded template '" + fallback_template->manifest.meta.name + "')";
	}
	return summary;
}

} // namespace

struct NaxgcSource {
	obs_source_t *source{nullptr};

	uint32_t width{kDefaultWidth};
	uint32_t height{kDefaultHeight};

	std::string template_path;
	std::string last_evaluated_path;
	std::shared_ptr<const LoadedTemplate> active_template;
	Renderer renderer;
	Animator animator;
	std::string status_text{"No template folder selected."};
	enum obs_text_info_type status_info_type { OBS_TEXT_INFO_NORMAL };
	bool reset_on_show{false};

	// TEMPORARY (M4 OBS visual verification):
	// Automated loop animation and demo state allow testing the Animator directly
	// in OBS Studio before M6 (Qt dock control panel) and M7 (obs-websocket) are implemented.
	bool demo_loop{true};
	double demo_timer{0.0};
	bool demo_is_shown{false};

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
	obs_data_set_default_string(settings, "template_status", "No template folder selected.");
	obs_data_set_default_int(settings, "width", static_cast<long long>(kDefaultWidth));
	obs_data_set_default_int(settings, "height", static_cast<long long>(kDefaultHeight));
	obs_data_set_default_bool(settings, "reset_on_show", false);
	obs_data_set_default_bool(settings, "demo_loop", true);
}

static bool on_template_path_modified(void *priv, obs_properties_t *props, obs_property_t *property,
				      obs_data_t *settings)
{
	UNUSED_PARAMETER(property);
	obs_property_t *status_prop = obs_properties_get(props, "template_status");
	if (!status_prop || !settings) {
		return false;
	}

	auto *context = static_cast<NaxgcSource *>(priv);
	const char *raw_path = obs_data_get_string(settings, "template_path");
	const std::string path_str = raw_path ? raw_path : "";

	if (path_str.empty()) {
		obs_data_set_string(settings, "template_status", "No template folder selected.");
		obs_property_text_set_info_type(status_prop, OBS_TEXT_INFO_NORMAL);
		return true;
	}

	if (context && path_str == context->last_evaluated_path) {
		obs_data_set_string(settings, "template_status", context->status_text.c_str());
		obs_property_text_set_info_type(status_prop, context->status_info_type);
		return true;
	}

	try {
		const std::filesystem::path fs_path = std::filesystem::u8path(path_str.begin(), path_str.end());
		const TemplateLoadResult res = load_template_package(fs_path);
		const LoadedTemplate *fallback = context ? context->active_template.get() : nullptr;
		const StatusSummary summary = build_status_summary(res, fallback);
		obs_data_set_string(settings, "template_status", summary.text.c_str());
		obs_property_text_set_info_type(status_prop, summary.info_type);
	} catch (const std::exception &ex) {
		const std::string msg = std::string("NXG-E003: unexpected error reading template: ") + ex.what();
		obs_data_set_string(settings, "template_status", msg.c_str());
		obs_property_text_set_info_type(status_prop, OBS_TEXT_INFO_ERROR);
	} catch (...) {
		obs_data_set_string(settings, "template_status", "NXG-E003: unexpected error reading template");
		obs_property_text_set_info_type(status_prop, OBS_TEXT_INFO_ERROR);
	}

	return true;
}

static bool on_show_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	auto *context = static_cast<NaxgcSource *>(data);
	if (!context || !context->active_template) {
		return false;
	}
	if (context->animator.run_action("show").is_err()) {
		context->animator.play("main.in");
		context->animator.play("loop_vivo");
	}
	return true;
}

static bool on_hide_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	auto *context = static_cast<NaxgcSource *>(data);
	if (!context || !context->active_template) {
		return false;
	}
	if (context->animator.run_action("hide").is_err()) {
		context->animator.play("main.out");
		context->animator.stop("loop_vivo");
	}
	return true;
}

static obs_properties_t *naxgc_source_get_properties(void *data)
{
	auto *context = static_cast<NaxgcSource *>(data);
	obs_properties_t *props = obs_properties_create();

	obs_property_t *path_prop = obs_properties_add_path(props, "template_path", "Template Folder",
							    OBS_PATH_DIRECTORY, nullptr, nullptr);
	obs_property_set_modified_callback2(path_prop, on_template_path_modified, context);

	obs_property_t *status_prop =
		obs_properties_add_text(props, "template_status", "Template Status", OBS_TEXT_INFO);
	obs_property_text_set_info_word_wrap(status_prop, true);
	if (context) {
		obs_property_text_set_info_type(status_prop, context->status_info_type);
	}

	obs_properties_add_int(props, "width", "Width", 100, 7680, 1);
	obs_properties_add_int(props, "height", "Height", 100, 4320, 1);
	obs_properties_add_bool(props, "reset_on_show", "Reset animation on show");

	// TEMPORARY (M4 OBS visual verification): controls to trigger animations manually or in loop
	obs_properties_add_bool(props, "demo_loop", "Demo loop animation (show / hide continuous)");
	obs_properties_add_button2(props, "btn_show", "▶ Entrar (show)", on_show_clicked, context);
	obs_properties_add_button2(props, "btn_hide", "■ Sair (hide)", on_hide_clicked, context);

	return props;
}

static void naxgc_source_update(void *data, obs_data_t *settings)
{
	auto *context = static_cast<NaxgcSource *>(data);
	if (!context || !settings) {
		return;
	}

	const char *path = obs_data_get_string(settings, "template_path");
	const std::string new_path = path ? path : "";
	context->template_path = new_path;

	const auto w = static_cast<uint32_t>(obs_data_get_int(settings, "width"));
	const auto h = static_cast<uint32_t>(obs_data_get_int(settings, "height"));
	context->width = (w > 0) ? w : kDefaultWidth;
	context->height = (h > 0) ? h : kDefaultHeight;

	context->reset_on_show = obs_data_get_bool(settings, "reset_on_show");
	context->demo_loop = obs_data_get_bool(settings, "demo_loop");

	if (new_path.empty()) {
		context->last_evaluated_path.clear();
		context->active_template.reset();
		context->animator.reset();
		context->renderer.clear();
		context->status_text = "No template folder selected.";
		context->status_info_type = OBS_TEXT_INFO_NORMAL;
		obs_data_set_string(settings, "template_status", context->status_text.c_str());
	} else if (new_path != context->last_evaluated_path) {
		context->last_evaluated_path = new_path;
		try {
			const std::filesystem::path fs_path = std::filesystem::u8path(new_path.begin(), new_path.end());
			TemplateLoadResult res = load_template_package(fs_path);

			for (const auto &diag : res.diagnostics) {
				if (diag.severity == DiagnosticSeverity::Warning) {
					blog(LOG_WARNING, "[NaxGC] %s", diag.format().c_str());
				} else {
					blog(LOG_ERROR, "[NaxGC] %s", diag.format().c_str());
				}
			}

			const StatusSummary summary = build_status_summary(res, context->active_template.get());
			context->status_text = summary.text;
			context->status_info_type = summary.info_type;
			obs_data_set_string(settings, "template_status", context->status_text.c_str());

			if (res.is_ok() && res.loaded.has_value()) {
				context->active_template = std::make_shared<LoadedTemplate>(std::move(*res.loaded));
				context->renderer.set_template(context->active_template, false);
				context->animator.set_target(&context->active_template->manifest,
							     &context->renderer.scene(), true);

				// TEMPORARY (M4 OBS visual verification):
				// Populate default placeholder text for the lower third if none was provided,
				// so that typography and mask clipping are clearly visible during the animation check.
				if (context->demo_loop) {
					auto txt1 = context->renderer.scene().get_property_effective(
						"layers.linha1texto.text");
					if (txt1.is_ok() && std::holds_alternative<std::string>(txt1.value()) &&
					    std::get<std::string>(txt1.value()).empty()) {
						context->renderer.scene().set_property_base(
							"layers.linha1texto.text",
							std::string("Maria Fernanda Oliveira"));
					}
					auto txt2 = context->renderer.scene().get_property_effective(
						"layers.linha2texto.text");
					if (txt2.is_ok() && std::holds_alternative<std::string>(txt2.value()) &&
					    std::get<std::string>(txt2.value()).empty()) {
						context->renderer.scene().set_property_base(
							"layers.linha2texto.text", std::string("Apresentadora"));
					}

					if (context->animator.run_action("show").is_err()) {
						context->animator.play("main.in");
						context->animator.play("loop_vivo");
					}
					context->demo_is_shown = true;
					context->demo_timer = 0.0;
				}

				const Manifest &m = context->active_template->manifest;
				blog(LOG_INFO,
				     "[NaxGC] Template loaded: '%s' (id=%s, canvas=%ux%u, layers=%zu, clips=%zu)",
				     m.meta.name.c_str(), m.meta.id.c_str(), m.canvas.width, m.canvas.height,
				     m.layers.size(), m.animations.size());
			} else if (context->active_template) {
				blog(LOG_WARNING, "[NaxGC] Failed to load new template; keeping active template '%s'",
				     context->active_template->manifest.meta.id.c_str());
			}
		} catch (const std::exception &ex) {
			blog(LOG_ERROR, "[NaxGC] NXG-E003: unexpected error loading template: %s", ex.what());
			context->status_text = std::string("NXG-E003: unexpected error loading template: ") + ex.what();
			context->status_info_type = OBS_TEXT_INFO_ERROR;
			obs_data_set_string(settings, "template_status", context->status_text.c_str());
		} catch (...) {
			blog(LOG_ERROR, "[NaxGC] NXG-E003: unexpected unknown error loading template");
			context->status_text = "NXG-E003: unexpected unknown error loading template";
			context->status_info_type = OBS_TEXT_INFO_ERROR;
			obs_data_set_string(settings, "template_status", context->status_text.c_str());
		}
	}

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
	return context ? context->width : kDefaultWidth;
}

static uint32_t naxgc_source_get_height(void *data)
{
	auto *context = static_cast<NaxgcSource *>(data);
	return context ? context->height : kDefaultHeight;
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
		if (context->active_template) {
			context->animator.set_target(&context->active_template->manifest, &context->renderer.scene(),
						     true);
		}
		blog(LOG_INFO, "[NaxGC] Source shown (reset_on_show: anim_time and animator reset)");
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
	context->animator.tick(static_cast<double>(seconds));

	// TEMPORARY (M4 OBS visual verification):
	// Automatically cycles show/hide continuous loop so playback can be inspected in OBS Studio
	// without keeping the animation idle/static for too long (2.0s on-air, 0.6s off-air).
	if (context->demo_loop && context->active_template && context->is_showing) {
		context->demo_timer += static_cast<double>(seconds);
		if (context->demo_is_shown) {
			if (context->demo_timer >= 2.0) {
				if (context->animator.run_action("hide").is_err()) {
					context->animator.play("main.out");
					context->animator.stop("loop_vivo");
				}
				context->demo_is_shown = false;
				context->demo_timer = 0.0;
			}
		} else {
			if (context->demo_timer >= 0.6) {
				if (context->animator.run_action("show").is_err()) {
					context->animator.play("main.in");
					context->animator.play("loop_vivo");
				}
				context->demo_is_shown = true;
				context->demo_timer = 0.0;
			}
		}
	}
}

static void naxgc_source_video_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	auto *context = static_cast<NaxgcSource *>(data);
	if (!context) {
		return;
	}

	context->renderer.render(context->width, context->height);
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
