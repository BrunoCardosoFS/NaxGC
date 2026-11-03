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

#include "render/rasterizer_text.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

#include <QByteArray>
#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QTextLayout>
#include <QTextOption>

namespace naxgc {

namespace {

QFont build_qfont(const std::string &family_name, int weight, bool italic, double pixel_size, double letter_spacing)
{
	QFont font;
	if (!family_name.empty()) {
		font.setFamily(QString::fromUtf8(family_name.data(), static_cast<qsizetype>(family_name.size())));
	}
	font.setWeight(static_cast<QFont::Weight>(std::clamp(weight, 100, 900)));
	font.setItalic(italic);
	font.setPixelSize(std::max(1, static_cast<int>(std::lround(pixel_size))));
	font.setLetterSpacing(QFont::AbsoluteSpacing, letter_spacing);
	font.setStyleStrategy(QFont::PreferAntialias);
	return font;
}

QStringList wrap_text_into_lines(const QString &text, const QFont &font, double max_width, int max_lines)
{
	QStringList out_lines;
	if (text.isEmpty() || max_lines <= 0 || max_width <= 0.0) {
		return out_lines;
	}

	const QFontMetricsF fm(font);
	const QStringList paragraphs = text.split(u'\n');

	for (int p = 0; p < paragraphs.size() && out_lines.size() < max_lines; ++p) {
		const QString &para = paragraphs[p];
		if (para.isEmpty()) {
			out_lines.append(QString());
			continue;
		}

		QTextLayout layout(para, font);
		QTextOption opt;
		opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
		layout.setTextOption(opt);

		layout.beginLayout();
		while (out_lines.size() < max_lines) {
			QTextLine line = layout.createLine();
			if (!line.isValid()) {
				break;
			}
			line.setLineWidth(max_width);
			const int start = line.textStart();
			const int length = line.textLength();

			const bool is_last_allowed_line = (out_lines.size() + 1 == max_lines);
			const bool has_more_in_para = (start + length < para.size());
			const bool has_more_paras = (p + 1 < paragraphs.size());

			if (is_last_allowed_line && (has_more_in_para || has_more_paras)) {
				QString remaining = para.mid(start).trimmed();
				if (has_more_paras) {
					remaining += QStringLiteral(" …");
				}
				out_lines.append(fm.elidedText(remaining, Qt::ElideRight, max_width));
				break;
			}

			out_lines.append(para.mid(start, length).trimmed());
		}
		layout.endLayout();
	}

	return out_lines;
}

} // namespace

TextRasterizer::TextRasterizer(const LoadedTemplate &tpl)
{
	for (const auto &font_data : tpl.font_files) {
		if (font_data.bytes.empty()) {
			continue;
		}
		const QByteArray raw(font_data.bytes.data(), static_cast<qsizetype>(font_data.bytes.size()));
		const int id = QFontDatabase::addApplicationFontFromData(raw);
		if (id != -1) {
			registered_font_ids_.push_back(id);
			const QStringList families = QFontDatabase::applicationFontFamilies(id);
			if (!families.isEmpty() && !font_data.family.empty()) {
				family_alias_to_real_.emplace(font_data.family, families.first().toStdString());
			}
		}
	}
}

TextRasterizer::~TextRasterizer()
{
	unload_fonts();
}

TextRasterizer::TextRasterizer(TextRasterizer &&other) noexcept
	: registered_font_ids_(std::move(other.registered_font_ids_)),
	  family_alias_to_real_(std::move(other.family_alias_to_real_))
{
	other.registered_font_ids_.clear();
}

TextRasterizer &TextRasterizer::operator=(TextRasterizer &&other) noexcept
{
	if (this != &other) {
		unload_fonts();
		registered_font_ids_ = std::move(other.registered_font_ids_);
		family_alias_to_real_ = std::move(other.family_alias_to_real_);
		other.registered_font_ids_.clear();
	}
	return *this;
}

void TextRasterizer::unload_fonts() noexcept
{
	for (int id : registered_font_ids_) {
		QFontDatabase::removeApplicationFont(id);
	}
	registered_font_ids_.clear();
	family_alias_to_real_.clear();
}

RasterizedBitmap TextRasterizer::rasterize_layer(const SceneLayer &layer, double canvas_scale_x,
						 double canvas_scale_y) const
{
	RasterizedBitmap out;
	if (layer.type != LayerType::Text || layer.rest_bbox.is_empty()) {
		return out;
	}

	out.canvas_rect = layer.rest_bbox;

	const std::string &raw_text = layer.props.text.effective();
	if (raw_text.empty()) {
		return out;
	}

	QString qtext = QString::fromUtf8(raw_text.data(), static_cast<qsizetype>(raw_text.size()));
	if (layer.uppercase) {
		qtext = qtext.toUpper();
	}

	const double fw = layer.rest_bbox.width;
	const double fh = layer.rest_bbox.height;

	const double eff_sx = std::max(0.05, canvas_scale_x * layer.effective_raster_scale);
	const double eff_sy = std::max(0.05, canvas_scale_y * layer.effective_raster_scale);

	const uint32_t bmp_w =
		std::clamp(static_cast<uint32_t>(std::lround(fw * eff_sx)), 1u, kMaxLayerTextureDimension);
	const uint32_t bmp_h =
		std::clamp(static_cast<uint32_t>(std::lround(fh * eff_sy)), 1u, kMaxLayerTextureDimension);

	std::string resolved_family = layer.font;
	auto alias_it = family_alias_to_real_.find(layer.font);
	if (alias_it != family_alias_to_real_.end()) {
		resolved_family = alias_it->second;
	}

	double target_size = std::max(1.0, layer.props.size.effective());
	const double min_size = std::clamp(layer.min_size, 1.0, target_size);

	QFont font = build_qfont(resolved_family, layer.weight, layer.italic, target_size, layer.letter_spacing);

	QStringList lines;
	if (layer.overflow == TextOverflow::Shrink) {
		QString single_line = qtext;
		single_line.replace(u'\n', u' ');
		while (target_size > min_size) {
			font = build_qfont(resolved_family, layer.weight, layer.italic, target_size,
					   layer.letter_spacing);
			const QFontMetricsF fm(font);
			if (fm.horizontalAdvance(single_line) <= fw) {
				break;
			}
			target_size = std::max(min_size, target_size - 1.0);
		}
		font = build_qfont(resolved_family, layer.weight, layer.italic, target_size, layer.letter_spacing);
		lines.append(single_line);
	} else if (layer.overflow == TextOverflow::Ellipsis) {
		QString single_line = qtext;
		single_line.replace(u'\n', u' ');
		const QFontMetricsF fm(font);
		lines.append(fm.elidedText(single_line, Qt::ElideRight, fw));
	} else if (layer.overflow == TextOverflow::Wrap) {
		lines = wrap_text_into_lines(qtext, font, fw, std::max(1, layer.max_lines));
	} else {
		// TextOverflow::Clip
		QString single_line = qtext;
		single_line.replace(u'\n', u' ');
		lines.append(single_line);
	}

	if (lines.isEmpty()) {
		return out;
	}

	QImage image(static_cast<int>(bmp_w), static_cast<int>(bmp_h), QImage::Format_ARGB32_Premultiplied);
	image.fill(Qt::transparent);

	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setRenderHint(QPainter::TextAntialiasing, true);

	// Scale painter so drawing coordinates match canvas frame units [0..fw, 0..fh]
	painter.scale(static_cast<double>(bmp_w) / fw, static_cast<double>(bmp_h) / fh);
	painter.setClipRect(QRectF(0.0, 0.0, fw, fh));
	painter.setFont(font);

	const Color c = layer.props.color.effective();
	painter.setPen(QColor(c.r, c.g, c.b, c.a));

	const QFontMetricsF fm(font);
	const double line_step = target_size * std::max(0.1, layer.line_height);
	const int n_lines = static_cast<int>(lines.size());
	const double total_block_h = fm.ascent() + fm.descent() + static_cast<double>(n_lines - 1) * line_step;

	double first_baseline_y = fm.ascent();
	switch (layer.valign) {
	case TextVAlign::Top:
		first_baseline_y = fm.ascent();
		break;
	case TextVAlign::Middle:
		first_baseline_y = (fh - total_block_h) * 0.5 + fm.ascent();
		break;
	case TextVAlign::Bottom:
		first_baseline_y = fh - total_block_h + fm.ascent();
		break;
	}

	for (int i = 0; i < n_lines; ++i) {
		const QString &line_str = lines[i];
		if (line_str.isEmpty()) {
			continue;
		}
		const double adv = fm.horizontalAdvance(line_str);
		double x = 0.0;
		switch (layer.align) {
		case TextAlign::Left:
			x = 0.0;
			break;
		case TextAlign::Center:
			x = (fw - adv) * 0.5;
			break;
		case TextAlign::Right:
			x = fw - adv;
			break;
		}
		const double y = first_baseline_y + static_cast<double>(i) * line_step;
		painter.drawText(QPointF(x, y), line_str);
	}

	painter.end();

	const std::size_t row_bytes = static_cast<std::size_t>(bmp_w) * 4u;
	out.bgra_premultiplied.resize(row_bytes * static_cast<std::size_t>(bmp_h));
	for (uint32_t y = 0; y < bmp_h; ++y) {
		std::memcpy(out.bgra_premultiplied.data() + static_cast<std::size_t>(y) * row_bytes,
			    image.constScanLine(static_cast<int>(y)), row_bytes);
	}
	out.width = bmp_w;
	out.height = bmp_h;
	return out;
}

} // namespace naxgc
