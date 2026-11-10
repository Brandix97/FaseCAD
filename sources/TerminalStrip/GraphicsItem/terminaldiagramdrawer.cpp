/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "terminaldiagramdrawer.h"
#include "terminalstripdrawerhelpers.h"

#include <QCoreApplication>
#include <QFontMetricsF>
#include <QHash>
#include <QPainter>

namespace TerminalStripDrawer {
namespace Diagram {

namespace {

	//The sizes of the drawing, in pixels of the folio
const qreal header_width = 230;
const qreal header_label_width = 30;
const qreal column_width = 18;
const qreal function_height = 200;
const qreal plc_height = 40;
const qreal connection_height = 40;
const qreal bridge_height = 30;
const qreal color_height = 20;
const qreal type_height = 22;
const qreal section_height = 30;
const qreal number_height = 24;
const qreal table_height = function_height + plc_height + connection_height
		+ bridge_height + color_height + type_height + section_height
		+ number_height;
const qreal wire_length = 130;
const qreal box_height = 64;
const qreal box_pins_height = 11;
const qreal box_label_height = 13;

	/// One column of the table: a real terminal, or an empty column
	/// between the terminals of two devices
struct Column
{
	QSharedPointer<AbstractRealTerminalInterface> real;
	DiagramColumn data;
	int physical = -1;
	int real_index = -1;
		/// How many levels the terminal has
	int levels = 1;
	bool gap() const {return real.isNull();}
};

QVector<Column> columns(const AbstractTerminalStripInterface &strip)
{
	QVector<Column> list;
	QString last_device;
	int physical = 0;
	for (const auto &physical_terminal : strip.physicalTerminal()) {
		int real_index = 0;
		const int levels = int(physical_terminal->realTerminals().size());
		for (const auto &real : physical_terminal->realTerminals()) {
			Column column;
			column.real = real;
			column.data = real->diagramColumn();
			column.physical = physical;
			column.real_index = real_index++;
			column.levels = levels;
				//Every device gets the columns of its terminals, with an
				//empty one before the next device
			if (!list.isEmpty() && column.data.device_key != last_device) {
				list << Column();
			}
			last_device = column.data.device_key;
			list << column;
		}
		++physical;
	}
	return list;
}

QString text(const char *source)
{
	return QCoreApplication::translate("TerminalDiagram", source);
}

QFont sizedFont(const QFont &font, qreal pixels)
{
	QFont sized(font);
	sized.setPixelSize(qMax(1, qRound(pixels)));
	return sized;
}

	/// Write @p string from the bottom to the top of @p rect
void drawVerticalText(QPainter *painter, const QRectF &rect,
					  const QString &string, Qt::Alignment alignment)
{
	painter->save();
	painter->translate(rect.bottomLeft());
	painter->rotate(-90);
	const QRectF turned(0, 0, rect.height(), rect.width());
	const QString shown = QFontMetricsF(painter->font()).elidedText(
				string, Qt::ElideRight, turned.width() - 4);
	painter->drawText(turned.adjusted(2, 0, -2, 0), alignment, shown);
	painter->restore();
}

	/// Draw @p picture as large as it fits in @p rect, centered
void drawSymbol(QPainter *painter, const QRectF &rect, const QPicture &picture)
{
	const QRectF source = picture.boundingRect();
	if (source.isEmpty() || rect.width() <= 0 || rect.height() <= 0) {
		return;
	}
	const qreal scale = qMin(rect.width() / source.width(),
							 rect.height() / source.height());
	painter->save();
	painter->translate(rect.center());
	painter->scale(scale, scale);
	painter->translate(-source.center());
	painter->drawPicture(0, 0, picture);
	painter->restore();
}

} // namespace

QRectF boundingRect(const AbstractTerminalStripInterface *strip,
					const TerminalStripLayoutPattern &pattern)
{
	Q_UNUSED(pattern)
	const int count = strip ? int(columns(*strip).size()) : 0;
	return QRectF(0, 0, header_width + qMax(1, count) * column_width,
				  table_height + wire_length + box_height + 2);
}

void paint(QPainter *painter,
		   const AbstractTerminalStripInterface &strip,
		   const TerminalStripLayoutPattern &pattern,
		   bool preview_draw,
		   const QPointF &hover_pos,
		   QVector<XrefRect> *xref_rects)
{
	const QVector<Column> list = columns(strip);
	const QRectF bounds = boundingRect(&strip, pattern);
	const qreal right = bounds.width();

	painter->save();
	painter->setRenderHint(QPainter::Antialiasing, true);
	const QPen pen(Qt::black, 0.8);
	painter->setPen(pen);
	painter->setBrush(Qt::NoBrush);

	if (preview_draw) {
		painter->save();
		painter->setPen(Qt::blue);
		painter->drawRect(bounds);
		painter->restore();
	}

		//The rows, from the top
	const qreal plc_y = function_height;
	const qreal connection_y = plc_y + plc_height;
	const qreal bridge_y = connection_y + connection_height;
	const qreal color_y = bridge_y + bridge_height;
	const qreal type_y = color_y + color_height;
	const qreal section_y = type_y + type_height;
	const qreal number_y = section_y + section_height;

		//The terminal under the mouse
	int hovered = -1;
	for (int i = 0; i < list.size(); ++i) {
		const QRectF rect(header_width + i * column_width, 0,
						  column_width, table_height);
		if (!list.at(i).gap() && !hover_pos.isNull()
				&& rect.contains(hover_pos)) {
			hovered = i;
		}
		if (xref_rects && !list.at(i).gap()) {
			XrefRect xref_rect;
			xref_rect.rect = rect;
			xref_rect.physical = list.at(i).physical;
			xref_rect.real = list.at(i).real_index;
			*xref_rects << xref_rect;
		}
	}
	if (hovered >= 0) {
		painter->fillRect(QRectF(header_width + hovered * column_width, 0,
								 column_width, table_height),
						  QColor(0, 120, 215, 50));
	}

		//The frame and the lines of the rows
	painter->drawRect(QRectF(0, 0, right, table_height));
	for (qreal y : {plc_y, connection_y, bridge_y, color_y, type_y,
				   section_y, number_y}) {
		painter->drawLine(QPointF(0, y), QPointF(right, y));
	}
	painter->drawLine(QPointF(header_width - header_label_width, 0),
					  QPointF(header_width - header_label_width, plc_y));
	for (int i = 0; i <= list.size(); ++i) {
		const qreal x = header_width + i * column_width;
		painter->drawLine(QPointF(x, 0), QPointF(x, table_height));
	}

		//The header: the name of the strip, where it is, and the titles
		//of the rows
	const QFont base = pattern.font();
	painter->setFont(sizedFont(base, 18));
	const QRectF name_rect(4, 0, header_width - header_label_width - 8,
						   function_height / 2);
	painter->drawText(name_rect, Qt::AlignCenter, strip.name());
	QStringList place;
	if (!strip.installation().isEmpty()) {
		place << strip.installation();
	}
	if (!strip.location().isEmpty()) {
		place << strip.location();
	}
	painter->setFont(sizedFont(base, 11));
	painter->drawText(name_rect.translated(0, function_height / 2),
					  Qt::AlignCenter | Qt::TextWordWrap,
					  place.join(QStringLiteral(" ")));
	drawVerticalText(painter,
					 QRectF(header_width - header_label_width, 0,
							header_label_width, function_height),
					 text("Function text"), Qt::AlignLeft | Qt::AlignVCenter);
	const qreal title_width = header_width - 8;
	painter->drawText(QRectF(4, plc_y, title_width, plc_height),
					  Qt::AlignLeft | Qt::AlignVCenter,
					  text("PLC designation\nDevice connection point"));
	painter->drawText(QRectF(4, connection_y, title_width, connection_height),
					  Qt::AlignLeft | Qt::AlignBottom,
					  text("Designation / connection number"));
	painter->drawText(QRectF(4, bridge_y + 2, title_width, bridge_height),
					  Qt::AlignLeft | Qt::AlignTop, text("Bridges"));
	painter->drawText(QRectF(4, color_y, title_width, color_height),
					  Qt::AlignLeft | Qt::AlignVCenter, text("Terminal colour"));
	painter->drawText(QRectF(4, type_y, title_width, type_height),
					  Qt::AlignLeft | Qt::AlignVCenter, text("Terminal type"));
	painter->drawText(QRectF(4, section_y, title_width, section_height),
					  Qt::AlignLeft | Qt::AlignVCenter, text("Cross-section (mm²)"));
	painter->drawText(QRectF(4, number_y, title_width, number_height),
					  Qt::AlignLeft | Qt::AlignVCenter, text("Terminal number"));

		//The bridges: up to two bars per terminal, one row each in the
		//bridges row (the groove of the bar, the first one on top). A
		//bar runs from the first terminal it joins to the last, over
		//the ones between which it leaves out, with a dot on each
		//terminal it joins
	struct Bar
	{
		int row = 0;
		int first = -1;
		int last = -1;
		QVector<int> members;
	};
	QHash<QUuid, Bar> bars;
	for (int i = 0; i < list.size(); ++i) {
		if (list.at(i).gap()) {
			continue;
		}
		const auto bridge_list = list.at(i).real->bridges();
		for (AbstractBridgeInterface *bridge_ : bridge_list) {
			Bar &bar = bars[bridge_->uuid()];
			bar.row = qBound(1, bridge_->track(), 2) - 1;
			if (bar.first < 0) {
				bar.first = i;
			}
			bar.last = i;
			bar.members << i;
		}
		qDeleteAll(bridge_list);
	}
	bool two_rows = false;
	QVector<bool> covered(list.size(), false);
	for (const Bar &bar : std::as_const(bars)) {
		two_rows = two_rows || bar.row == 1;
		for (int i = bar.first; i <= bar.last && bar.members.size() > 1; ++i) {
			covered[i] = true;
		}
	}
	const auto bar_y = [&](int row) {
		if (!two_rows) {
			return bridge_y + bridge_height * 0.6;
		}
		return bridge_y + bridge_height * (row == 0 ? 0.45 : 0.78);
	};

		//The columns
	const QFont cell_font = sizedFont(base, 9);
	const QFont wire_font = sizedFont(base, 7);
	painter->setFont(cell_font);
	QString previous_function;
	bool has_previous = false;
	for (int i = 0; i < list.size(); ++i) {
		const Column &column = list.at(i);
		if (column.gap()) {
			continue;
		}
		const qreal x = header_width + i * column_width;
		const qreal center = x + column_width / 2;
		const DiagramColumn &data = column.data;

			//The same function as the terminal before is written "="
		const QString function = has_previous
				&& !data.function_text.isEmpty()
				&& data.function_text == previous_function
				? QStringLiteral("=") : data.function_text;
		previous_function = data.function_text;
		has_previous = true;
		drawVerticalText(painter, QRectF(x, 0, column_width, function_height),
						 function, Qt::AlignLeft | Qt::AlignVCenter);
		drawVerticalText(painter, QRectF(x, plc_y, column_width, plc_height),
						 data.plc_designation, Qt::AlignCenter);
		drawVerticalText(painter,
						 QRectF(x, connection_y, column_width, connection_height),
						 data.inner_connection, Qt::AlignCenter);

			//A terminal no bar runs over: "-"
		if (!covered.at(i)) {
			const qreal mid = bar_y(0);
			painter->drawLine(QPointF(center - 2, mid),
							  QPointF(center + 2, mid));
		}

			//Colour, type symbol and cross-section of the terminal
		const QColor fill = column.real->color();
		if (fill.isValid()) {
			const QRectF swatch(x + 3, color_y + 4, column_width - 6,
						  color_height - 8);
			painter->fillRect(swatch, fill);
			const QColor stripe = column.real->stripeColor();
			if (stripe.isValid()) {
				painter->fillRect(QRectF(swatch.left(), swatch.center().y() - 1.5,
								swatch.width(), 3), stripe);
			}
			painter->drawRect(swatch);
		}
		const ElementData::TerminalType type = column.real->type();
		if (type != ElementData::TTGeneric || column.real->isLed()) {
			Helpers::drawTypeSymbol(painter,
								QRectF(x + 2, type_y + 3, column_width - 4,
									   type_height - 6),
								type, column.real->isLed());
		}
		drawVerticalText(painter,
						 QRectF(x, section_y, column_width, section_height),
						 data.section, Qt::AlignCenter);

			//Terminal number, after a small terminal symbol; the levels of
			//a terminal of several levels as steps, level 1 the tallest
		if (column.levels > 1) {
			const qreal step = (number_height - 8) / (2.0 * column.levels);
			const qreal inset = step * column.real_index;
			painter->save();
			painter->setBrush(Qt::black);
			painter->drawRect(QRectF(x + 2, number_y + 3 + inset, 2.5,
									number_height - 6 - 2 * inset));
			painter->restore();
				//The levels of one terminal joined under their numbers
			if (column.real_index + 1 < column.levels) {
				painter->drawLine(QPointF(x + 3, number_y + number_height - 2),
										  QPointF(x + column_width + 3,
												  number_y + number_height - 2));
			}
		} else {
			painter->drawEllipse(QPointF(x + 4, number_y + number_height / 2 + 2),
								 1.6, 1.6);
		}
		painter->drawText(QRectF(x + 6, number_y, column_width - 6,
								 number_height),
						  Qt::AlignLeft | Qt::AlignVCenter,
						  Helpers::terminalNumber(column.real->label()));
	}

	for (const Bar &bar : std::as_const(bars)) {
		const qreal y = bar_y(bar.row);
		const auto middle = [](int i) {
			return header_width + i * column_width + column_width / 2;
		};
		if (bar.members.size() > 1) {
			painter->drawLine(QPointF(middle(bar.first), y),
							  QPointF(middle(bar.last), y));
		}
		painter->save();
		painter->setBrush(Qt::black);
		for (int i : bar.members) {
			painter->drawEllipse(QPointF(middle(i), y), 2, 2);
		}
		painter->restore();
	}

		//Under the table: the outer wires and the boxes of the devices
	const qreal box_y = table_height + wire_length;
	int first = -1;
	for (int i = 0; i <= list.size(); ++i) {
		const bool ends = i == list.size() || list.at(i).gap()
				|| (first >= 0 && list.at(i).data.device_key
					!= list.at(first).data.device_key);
		if (ends && first >= 0
				&& list.at(first).data.device_key.isEmpty()) {
			first = -1;
		}
		if (ends && first >= 0) {
			const DiagramColumn &device = list.at(first).data;
			const qreal left = header_width + first * column_width + 1;
			const qreal width = (i - first) * column_width - 2;
			const QRectF box(left, box_y, width, box_height);
			painter->drawRect(box);
			painter->drawLine(QPointF(left, box_y + box_pins_height),
							  QPointF(left + width, box_y + box_pins_height));
			painter->drawLine(
						QPointF(left, box_y + box_pins_height + box_label_height),
						QPointF(left + width,
								box_y + box_pins_height + box_label_height));
			painter->setFont(wire_font);
			for (int j = first; j < i; ++j) {
				painter->drawText(
							QRectF(header_width + j * column_width, box_y,
								   column_width, box_pins_height),
							Qt::AlignCenter, list.at(j).data.device_pin);
			}
			painter->setFont(cell_font);
			const QRectF label_rect(left, box_y + box_pins_height, width,
									box_label_height);
			painter->drawText(label_rect, Qt::AlignCenter,
							  QFontMetricsF(cell_font).elidedText(
								  device.device_label, Qt::ElideRight,
								  width));
			drawSymbol(painter,
					   QRectF(left + 2,
							  box_y + box_pins_height + box_label_height + 2,
							  width - 4,
							  box_height - box_pins_height - box_label_height - 4),
					   device.device_symbol);
			first = -1;
		}
		if (i == list.size() || list.at(i).gap()) {
			continue;
		}
		if (first < 0) {
			first = i;
		}

		const Column &column = list.at(i);
		const qreal center = header_width + i * column_width + column_width / 2;
		if (column.data.device_key.isEmpty() && column.data.outer_wire.isEmpty()) {
			continue;
		}
		painter->drawLine(QPointF(center, table_height),
						  QPointF(center, box_y));
		painter->setFont(wire_font);
		const QRectF top(center + 1.5, table_height + 2, column_width, 10);
		const QRectF bottom(center + 1.5, box_y - 12, column_width, 10);
		painter->drawText(top, Qt::AlignLeft | Qt::AlignVCenter,
						  column.data.outer_wire);
		painter->drawText(bottom, Qt::AlignLeft | Qt::AlignVCenter,
						  column.data.outer_wire);
	}

	painter->restore();
}

} // namespace Diagram
} // namespace TerminalStripDrawer
