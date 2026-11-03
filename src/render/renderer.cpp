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

#include "render/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include <graphics/matrix4.h>
#include <graphics/vec4.h>
#include <obs-module.h>

namespace naxgc {

namespace {

constexpr const char *kLayerEffectSource = R"(
uniform float4x4 ViewProj;
uniform texture2d image;
uniform float opacity;

sampler_state def_sampler {
	Filter   = Linear;
	AddressU = Clamp;
	AddressV = Clamp;
};

struct VertInOut {
	float4 pos : POSITION;
	float2 uv  : TEXCOORD0;
};

VertInOut VSDefault(VertInOut vert_in)
{
	VertInOut vert_out;
	vert_out.pos = mul(float4(vert_in.pos.xyz, 1.0), ViewProj);
	vert_out.uv  = vert_in.uv;
	return vert_out;
}

float srgb_ch_to_linear(float u)
{
	return (u <= 0.04045) ? (u / 12.92) : pow((u + 0.055) / 1.055, 2.4);
}

float3 srgb_to_linear(float3 c)
{
	return float3(srgb_ch_to_linear(c.r), srgb_ch_to_linear(c.g), srgb_ch_to_linear(c.b));
}

float4 PSDrawPremult(VertInOut vert_in) : TARGET
{
	return image.Sample(def_sampler, vert_in.uv) * opacity;
}

float4 PSDrawPremultLinearSrgb(VertInOut vert_in) : TARGET
{
	float4 rgba = image.Sample(def_sampler, vert_in.uv);
	if (rgba.a > 0.0) {
		float3 straight_srgb = saturate(rgba.rgb / rgba.a);
		float3 straight_lin = srgb_to_linear(straight_srgb);
		rgba.rgb = straight_lin * rgba.a;
	}
	return rgba * opacity;
}

technique Draw
{
	pass
	{
		vertex_shader = VSDefault(vert_in);
		pixel_shader  = PSDrawPremult(vert_in);
	}
}

technique DrawLinearSrgb
{
	pass
	{
		vertex_shader = VSDefault(vert_in);
		pixel_shader  = PSDrawPremultLinearSrgb(vert_in);
	}
}
)";

constexpr const char *kMaskEffectSource = R"(
uniform float4x4 ViewProj;
uniform texture2d image;
uniform texture2d target;

sampler_state def_sampler {
	Filter   = Linear;
	AddressU = Clamp;
	AddressV = Clamp;
};

struct VertInOut {
	float4 pos : POSITION;
	float2 uv  : TEXCOORD0;
};

VertInOut VSDefault(VertInOut vert_in)
{
	VertInOut vert_out;
	vert_out.pos = mul(float4(vert_in.pos.xyz, 1.0), ViewProj);
	vert_out.uv  = vert_in.uv;
	return vert_out;
}

float srgb_ch_to_linear(float u)
{
	return (u <= 0.04045) ? (u / 12.92) : pow((u + 0.055) / 1.055, 2.4);
}

float3 srgb_to_linear(float3 c)
{
	return float3(srgb_ch_to_linear(c.r), srgb_ch_to_linear(c.g), srgb_ch_to_linear(c.b));
}

float4 PSMaskAlpha(VertInOut vert_in) : TARGET
{
	float4 layer_c = image.Sample(def_sampler, vert_in.uv);
	float4 mask_c  = target.Sample(def_sampler, vert_in.uv);
	return layer_c * mask_c.a;
}

float4 PSMaskAlphaLinearSrgb(VertInOut vert_in) : TARGET
{
	float4 layer_c = image.Sample(def_sampler, vert_in.uv);
	float4 mask_c  = target.Sample(def_sampler, vert_in.uv);
	float4 rgba    = layer_c * mask_c.a;
	if (rgba.a > 0.0) {
		float3 straight_srgb = saturate(rgba.rgb / rgba.a);
		float3 straight_lin  = srgb_to_linear(straight_srgb);
		rgba.rgb = straight_lin * rgba.a;
	}
	return rgba;
}

float4 PSMaskInverse(VertInOut vert_in) : TARGET
{
	float4 layer_c = image.Sample(def_sampler, vert_in.uv);
	float4 mask_c  = target.Sample(def_sampler, vert_in.uv);
	return layer_c * (1.0 - mask_c.a);
}

float4 PSMaskInverseLinearSrgb(VertInOut vert_in) : TARGET
{
	float4 layer_c = image.Sample(def_sampler, vert_in.uv);
	float4 mask_c  = target.Sample(def_sampler, vert_in.uv);
	float4 rgba    = layer_c * (1.0 - mask_c.a);
	if (rgba.a > 0.0) {
		float3 straight_srgb = saturate(rgba.rgb / rgba.a);
		float3 straight_lin  = srgb_to_linear(straight_srgb);
		rgba.rgb = straight_lin * rgba.a;
	}
	return rgba;
}

technique DrawAlpha
{
	pass
	{
		vertex_shader = VSDefault(vert_in);
		pixel_shader  = PSMaskAlpha(vert_in);
	}
}

technique DrawAlphaLinearSrgb
{
	pass
	{
		vertex_shader = VSDefault(vert_in);
		pixel_shader  = PSMaskAlphaLinearSrgb(vert_in);
	}
}

technique DrawInverse
{
	pass
	{
		vertex_shader = VSDefault(vert_in);
		pixel_shader  = PSMaskInverse(vert_in);
	}
}

technique DrawInverseLinearSrgb
{
	pass
	{
		vertex_shader = VSDefault(vert_in);
		pixel_shader  = PSMaskInverseLinearSrgb(vert_in);
	}
}
)";

void mat2d_to_obs_matrix4(const Mat2D &mat, struct matrix4 &out) noexcept
{
	vec4_set(&out.x, static_cast<float>(mat.a), static_cast<float>(mat.b), 0.0f, 0.0f);
	vec4_set(&out.y, static_cast<float>(mat.c), static_cast<float>(mat.d), 0.0f, 0.0f);
	vec4_set(&out.z, 0.0f, 0.0f, 1.0f, 0.0f);
	vec4_set(&out.t, static_cast<float>(mat.tx), static_cast<float>(mat.ty), 0.0f, 1.0f);
}

} // namespace

Renderer::~Renderer()
{
	texture_cache_.clear();
	texrender_pool_.clear();
	if (layer_effect_ != nullptr || mask_effect_ != nullptr) {
		obs_enter_graphics();
		if (layer_effect_ != nullptr) {
			gs_effect_destroy(layer_effect_);
			layer_effect_ = nullptr;
			image_param_ = nullptr;
			opacity_param_ = nullptr;
		}
		if (mask_effect_ != nullptr) {
			gs_effect_destroy(mask_effect_);
			mask_effect_ = nullptr;
			mask_image_param_ = nullptr;
			mask_target_param_ = nullptr;
		}
		obs_leave_graphics();
	}
}

void Renderer::set_template(std::shared_ptr<const LoadedTemplate> tpl, bool apply_initial_state)
{
	if (!tpl) {
		clear();
		return;
	}

	active_template_ = std::move(tpl);
	scene_ = Scene::from_template(*active_template_, apply_initial_state);
	svg_rasterizer_ = SvgRasterizer(*active_template_);
	svg_rasterizer_.populate_scene_rest_bboxes(scene_);
	text_rasterizer_ = TextRasterizer(*active_template_);
	texture_cache_.reset(scene_.layers().size());

	implicit_bg_dirty_ = svg_rasterizer_.has_implicit_background();
	last_canvas_scale_x_ = 0.0;
	last_canvas_scale_y_ = 0.0;
}

void Renderer::clear()
{
	active_template_.reset();
	scene_ = Scene{};
	svg_rasterizer_ = SvgRasterizer{};
	text_rasterizer_ = TextRasterizer{};
	texture_cache_.clear();
	texrender_pool_.clear();
	implicit_bg_dirty_ = false;
	last_canvas_scale_x_ = 1.0;
	last_canvas_scale_y_ = 1.0;
}

bool Renderer::ensure_effect_in_graphics()
{
	if (layer_effect_ != nullptr) {
		return true;
	}

	char *err_str = nullptr;
	layer_effect_ = gs_effect_create(kLayerEffectSource, "naxgc_layer_effect", &err_str);
	if (err_str != nullptr) {
		blog(LOG_ERROR, "[NaxGC] Failed to compile layer effect: %s", err_str);
		bfree(err_str);
	}
	if (layer_effect_ == nullptr) {
		return false;
	}

	image_param_ = gs_effect_get_param_by_name(layer_effect_, "image");
	opacity_param_ = gs_effect_get_param_by_name(layer_effect_, "opacity");
	return image_param_ != nullptr && opacity_param_ != nullptr;
}

bool Renderer::ensure_mask_effect_in_graphics()
{
	if (mask_effect_ != nullptr) {
		return true;
	}

	char *err_str = nullptr;
	mask_effect_ = gs_effect_create(kMaskEffectSource, "naxgc_mask_effect", &err_str);
	if (err_str != nullptr) {
		blog(LOG_ERROR, "[NaxGC] Failed to compile mask effect: %s", err_str);
		bfree(err_str);
	}
	if (mask_effect_ == nullptr) {
		return false;
	}

	mask_image_param_ = gs_effect_get_param_by_name(mask_effect_, "image");
	mask_target_param_ = gs_effect_get_param_by_name(mask_effect_, "target");
	return mask_image_param_ != nullptr && mask_target_param_ != nullptr;
}

void Renderer::update_dirty_layers_in_graphics(uint32_t output_width, uint32_t output_height,
					       std::size_t max_layers_per_frame)
{
	if (!active_template_) {
		return;
	}

	const double cw = static_cast<double>(std::max(1u, scene_.canvas().width));
	const double ch = static_cast<double>(std::max(1u, scene_.canvas().height));
	const double scale_x = static_cast<double>(std::max(1u, output_width)) / cw;
	const double scale_y = static_cast<double>(std::max(1u, output_height)) / ch;

	if (std::abs(scale_x - last_canvas_scale_x_) > 1e-4 || std::abs(scale_y - last_canvas_scale_y_) > 1e-4) {
		last_canvas_scale_x_ = scale_x;
		last_canvas_scale_y_ = scale_y;
		implicit_bg_dirty_ = svg_rasterizer_.has_implicit_background();
		for (auto &layer : scene_.layers()) {
			layer.content_dirty = true;
		}
	}

	if (implicit_bg_dirty_) {
		const RasterizedBitmap bg_bmp = svg_rasterizer_.rasterize_implicit_background(scale_x, scale_y);
		texture_cache_.upload_implicit_background_in_graphics(bg_bmp);
		implicit_bg_dirty_ = false;
	}

	std::size_t rerastered_count = 0;
	for (std::size_t i = 0; i < scene_.layers().size(); ++i) {
		if (rerastered_count >= max_layers_per_frame) {
			break;
		}
		SceneLayer &layer = scene_.layers()[i];
		if (!layer.content_dirty) {
			continue;
		}

		RasterizedBitmap bmp;
		if (layer.type == LayerType::Svg) {
			bmp = svg_rasterizer_.rasterize_layer(layer, scale_x, scale_y);
		} else if (layer.type == LayerType::Text) {
			bmp = text_rasterizer_.rasterize_layer(layer, scale_x, scale_y);
		}

		texture_cache_.upload_in_graphics(i, bmp);
		layer.content_dirty = false;
		++rerastered_count;
	}
}

void Renderer::draw_slot_with_matrix(const LayerTextureSlot &slot, const Mat2D &canvas_to_output,
				     const Mat2D &world_matrix, double opacity, bool linear_srgb)
{
	if (!slot.has_content || slot.texture == nullptr || slot.width == 0 || slot.height == 0 ||
	    slot.canvas_rect.is_empty() || opacity <= 0.0) {
		return;
	}

	const double tex_to_canvas_sx = slot.canvas_rect.width / static_cast<double>(slot.width);
	const double tex_to_canvas_sy = slot.canvas_rect.height / static_cast<double>(slot.height);
	const Mat2D tex_to_canvas = Mat2D::translation(slot.canvas_rect.x, slot.canvas_rect.y) *
				    Mat2D::scale(tex_to_canvas_sx, tex_to_canvas_sy);

	const Mat2D final_mat = canvas_to_output * world_matrix * tex_to_canvas;

	struct matrix4 obs_mat;
	mat2d_to_obs_matrix4(final_mat, obs_mat);

	gs_effect_set_texture(image_param_, slot.texture);
	gs_effect_set_float(opacity_param_, static_cast<float>(std::clamp(opacity, 0.0, 1.0)));

	const char *tech_name = linear_srgb ? "DrawLinearSrgb" : "Draw";
	gs_technique_t *tech = gs_effect_get_technique(layer_effect_, tech_name);
	if (tech == nullptr) {
		return;
	}

	gs_matrix_push();
	gs_matrix_mul(&obs_mat);

	const size_t passes = gs_technique_begin(tech);
	for (size_t p = 0; p < passes; ++p) {
		if (gs_technique_begin_pass(tech, p)) {
			gs_draw_sprite(slot.texture, 0, slot.width, slot.height);
			gs_technique_end_pass(tech);
		}
	}
	gs_technique_end(tech);

	gs_matrix_pop();
}

void Renderer::draw_mask_composite(gs_texture_t *layer_tex, gs_texture_t *mask_tex, uint32_t width, uint32_t height,
				   MaskMode mode, bool linear_srgb)
{
	if (!ensure_mask_effect_in_graphics() || layer_tex == nullptr || mask_tex == nullptr || width == 0 ||
	    height == 0) {
		return;
	}

	gs_effect_set_texture(mask_image_param_, layer_tex);
	gs_effect_set_texture(mask_target_param_, mask_tex);

	const char *tech_name = nullptr;
	if (mode == MaskMode::Alpha) {
		tech_name = linear_srgb ? "DrawAlphaLinearSrgb" : "DrawAlpha";
	} else {
		tech_name = linear_srgb ? "DrawInverseLinearSrgb" : "DrawInverse";
	}

	gs_technique_t *tech = gs_effect_get_technique(mask_effect_, tech_name);
	if (tech == nullptr) {
		return;
	}

	const size_t passes = gs_technique_begin(tech);
	for (size_t p = 0; p < passes; ++p) {
		if (gs_technique_begin_pass(tech, p)) {
			gs_draw_sprite(layer_tex, 0, width, height);
			gs_technique_end_pass(tech);
		}
	}
	gs_technique_end(tech);
}

void Renderer::draw_masked_layer(std::size_t layer_index, uint32_t output_width, uint32_t output_height,
				 const Mat2D &canvas_to_output, bool linear_srgb)
{
	const SceneLayer &layer = scene_.layers()[layer_index];
	const LayerTextureSlot *layer_slot = texture_cache_.get(layer_index);
	if (layer_slot == nullptr || !layer_slot->has_content) {
		return;
	}

	if (!layer.mask_layer_index.has_value()) {
		draw_slot_with_matrix(*layer_slot, canvas_to_output, layer.world_matrix, layer.effective_opacity,
				      linear_srgb);
		return;
	}

	const std::size_t mask_index = *layer.mask_layer_index;
	if (mask_index >= scene_.layers().size()) {
		draw_slot_with_matrix(*layer_slot, canvas_to_output, layer.world_matrix, layer.effective_opacity,
				      linear_srgb);
		return;
	}

	const SceneLayer &mask_layer = scene_.layers()[mask_index];
	const LayerTextureSlot *mask_slot = texture_cache_.get(mask_index);

	const bool mask_invisible = (!mask_layer.effective_visible || mask_layer.effective_opacity <= 0.0 ||
				     mask_slot == nullptr || !mask_slot->has_content);

	if (mask_invisible) {
		if (layer.mask_mode == MaskMode::Alpha) {
			// Zero alpha mask leaves nothing visible
			return;
		}
		// Inverse mode: 1.0 - 0.0 = 1.0 (drawn unmasked)
		draw_slot_with_matrix(*layer_slot, canvas_to_output, layer.world_matrix, layer.effective_opacity,
				      linear_srgb);
		return;
	}

	const enum gs_color_space space = gs_get_color_space();
	const enum gs_color_format format = gs_get_format_from_space(space);

	gs_texrender_t *tex_layer = texrender_pool_.acquire(format);
	gs_texrender_t *tex_mask = texrender_pool_.acquire(format);
	if (tex_layer == nullptr || tex_mask == nullptr) {
		texrender_pool_.release_all();
		return;
	}

	const struct vec4 transparent = {0.0f, 0.0f, 0.0f, 0.0f};

	// 1. Render masked layer into intermediate texrender A (raw premultiplied sRGB)
	if (gs_texrender_begin_with_color_space(tex_layer, output_width, output_height, space)) {
		gs_ortho(0.0f, static_cast<float>(output_width), 0.0f, static_cast<float>(output_height), -100.0f,
			 100.0f);
		gs_clear(GS_CLEAR_COLOR, &transparent, 0.0f, 0);

		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
		draw_slot_with_matrix(*layer_slot, canvas_to_output, layer.world_matrix, layer.effective_opacity,
				      false);
		gs_blend_state_pop();

		gs_texrender_end(tex_layer);
	}

	// 2. Render mask layer into intermediate texrender B (evaluated in canvas space with its own transform)
	if (gs_texrender_begin_with_color_space(tex_mask, output_width, output_height, space)) {
		gs_ortho(0.0f, static_cast<float>(output_width), 0.0f, static_cast<float>(output_height), -100.0f,
			 100.0f);
		gs_clear(GS_CLEAR_COLOR, &transparent, 0.0f, 0);

		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
		draw_slot_with_matrix(*mask_slot, canvas_to_output, mask_layer.world_matrix,
				      mask_layer.effective_opacity, false);
		gs_blend_state_pop();

		gs_texrender_end(tex_mask);
	}

	// 3. Compose A * alfa(B) into main output framebuffer
	gs_texture_t *layer_target_tex = gs_texrender_get_texture(tex_layer);
	gs_texture_t *mask_target_tex = gs_texrender_get_texture(tex_mask);
	if (layer_target_tex != nullptr && mask_target_tex != nullptr) {
		draw_mask_composite(layer_target_tex, mask_target_tex, output_width, output_height, layer.mask_mode,
				    linear_srgb);
	}

	texrender_pool_.release_all();
}

void Renderer::render(uint32_t output_width, uint32_t output_height)
{
	if (!active_template_ || output_width == 0 || output_height == 0) {
		return;
	}

	if (!ensure_effect_in_graphics()) {
		return;
	}

	update_dirty_layers_in_graphics(output_width, output_height);
	scene_.evaluate_transforms();

	const double cw = static_cast<double>(std::max(1u, scene_.canvas().width));
	const double ch = static_cast<double>(std::max(1u, scene_.canvas().height));
	const Mat2D canvas_to_output =
		Mat2D::scale(static_cast<double>(output_width) / cw, static_cast<double>(output_height) / ch);

	const bool linear_srgb = gs_get_linear_srgb();
	const bool prev_srgb = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(linear_srgb);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

	// Draw implicit static background first if present
	if (svg_rasterizer_.has_implicit_background()) {
		draw_slot_with_matrix(texture_cache_.implicit_background(), canvas_to_output, Mat2D::identity(), 1.0,
				      linear_srgb);
	}

	// Draw visible non-mask layers in SVG z-order
	for (std::size_t i = 0; i < scene_.layers().size(); ++i) {
		const SceneLayer &layer = scene_.layers()[i];
		if (layer.role == LayerRole::Mask) {
			continue;
		}
		if (!layer.effective_visible || layer.effective_opacity <= 0.0) {
			continue;
		}

		if (layer.mask_layer_index.has_value()) {
			draw_masked_layer(i, output_width, output_height, canvas_to_output, linear_srgb);
		} else {
			const LayerTextureSlot *slot = texture_cache_.get(i);
			if (slot != nullptr && slot->has_content) {
				draw_slot_with_matrix(*slot, canvas_to_output, layer.world_matrix,
						      layer.effective_opacity, linear_srgb);
			}
		}
	}

	gs_blend_state_pop();
	gs_enable_framebuffer_srgb(prev_srgb);
}

} // namespace naxgc
