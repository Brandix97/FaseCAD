/*
	Copyright 2026 The FaseCAD Team
	This file is part of FaseCAD.

	FaseCAD is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	FaseCAD is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with FaseCAD.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "titleblockdesign.h"

#include "../qetapp.h"
#include "../titleblockcell.h"
#include "../titleblocktemplate.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QTextStream>

#include <algorithm>

namespace TitleBlockDesign
{
namespace
{
	const int rows_count = 12;
		/// Width of the accent bar, pixels
	const int accent_width = 4;
	const char *context = "TitleBlockDesign";

	QString translated(const char *source)
	{
		return QCoreApplication::translate(context, source);
	}

	/**
		A cell of a block: its rows in the title block's twelve, its
		columns in the block's own
	*/
	struct Cell
	{
		int row = 0;
		int rows = rows_count;
		int column = 0;
		int columns = 1;
			/// English, translated when built; nullptr for no label
		const char *label = nullptr;
		QString text;
			/// The text is English to translate when built (headers)
		const char *fixed = nullptr;
		int font_size = 7;
		bool bold = false;
		Qt::Alignment align = Qt::AlignLeft;
		bool header = false;
		bool logo = false;
		bool accent = false;
		Qt::Edges borders = TitleBlockCell::allBorders();
	};

	struct Layout
	{
			/// Share of the block's width of each of its columns
		QList<qreal> columns;
		QList<Cell> cells;
	};

	Cell field(int row, int rows, const char *label, const QString &text,
			   int font_size, bool bold = false)
	{
		Cell cell;
		cell.row = row;
		cell.rows = rows;
		cell.label = label;
		cell.text = text;
		cell.font_size = font_size;
		cell.bold = bold;
		return cell;
	}

	Layout layoutOf(const Block &block, const Design &design)
	{
		Layout layout;
		const bool modern = design.style == Style::Modern;
		switch (block.kind)
		{
			case Kind::Logo: {
				layout.columns = {1.0};
				Cell cell;
				cell.logo = !design.logo.isEmpty();
				layout.cells << cell;
				break;
			}
			case Kind::Company: {
				layout.columns = {1.0};
				Cell name = field(0, 6, nullptr, text(block, 0), 10, true);
				Cell address = field(6, 3, nullptr, text(block, 1), 6);
				Cell contact = field(9, 3, nullptr, text(block, 2), 6);
				if (modern) {
						//A letterhead: no line between its lines
					name.borders = Qt::LeftEdge | Qt::TopEdge | Qt::RightEdge;
					address.borders = Qt::LeftEdge | Qt::RightEdge;
					contact.borders = Qt::LeftEdge | Qt::RightEdge | Qt::BottomEdge;
				}
				layout.cells << name << address << contact;
				break;
			}
			case Kind::Project:
				layout.columns = {1.0};
				layout.cells << field(0, 7, QT_TRANSLATE_NOOP("TitleBlockDesign", "Project"),
									  text(block, 0), 10, true)
							 << field(7, 5, QT_TRANSLATE_NOOP("TitleBlockDesign", "Sheet title"),
									  text(block, 1), 8);
				break;
			case Kind::Client:
				layout.columns = {1.0};
				layout.cells << field(0, 6, QT_TRANSLATE_NOOP("TitleBlockDesign", "Client"),
									  text(block, 0), 9, true)
							 << field(6, 3, QT_TRANSLATE_NOOP("TitleBlockDesign", "Site"),
									  text(block, 1), 7)
							 << field(9, 3, QT_TRANSLATE_NOOP("TitleBlockDesign", "Address"),
									  text(block, 2), 7);
				break;
			case Kind::Revisions: {
				layout.columns = {0.14, 0.22, 0.44, 0.20};
				const int count = design.revision_rows == 2 || design.revision_rows == 5
						? design.revision_rows : 3;
				const int span = rows_count / (count + 1);
				const char *headers[] = {QT_TRANSLATE_NOOP("TitleBlockDesign", "Rev"),
										 QT_TRANSLATE_NOOP("TitleBlockDesign", "Date"),
										 QT_TRANSLATE_NOOP("TitleBlockDesign", "Description"),
										 QT_TRANSLATE_NOOP("TitleBlockDesign", "By")};
				const char *fields[] = {"index", "date", "description", "author"};
				for (int column = 0 ; column < 4 ; ++column)
				{
					Cell header = field(0, span, nullptr, QString(), 6, true);
					header.column = column;
					header.fixed = headers[column];
					header.header = true;
					layout.cells << header;
					for (int revision = 1 ; revision <= count ; ++revision)
					{
						Cell cell = field(revision * span, span, nullptr,
										  QStringLiteral("%{revision%1-%2}")
										  .arg(revision).arg(QLatin1String(fields[column])), 7);
						cell.column = column;
						layout.cells << cell;
					}
				}
				break;
			}
			case Kind::Signatures: {
				layout.columns = {0.30, 0.42, 0.28};
				const char *headers[] = {nullptr,
										 QT_TRANSLATE_NOOP("TitleBlockDesign", "Name"),
										 QT_TRANSLATE_NOOP("TitleBlockDesign", "Date")};
				const char *roles[] = {QT_TRANSLATE_NOOP("TitleBlockDesign", "Drawn"),
									   QT_TRANSLATE_NOOP("TitleBlockDesign", "Checked"),
									   QT_TRANSLATE_NOOP("TitleBlockDesign", "Approved")};
				for (int column = 0 ; column < 3 ; ++column)
				{
					Cell header = field(0, 3, nullptr, QString(), 6, true);
					header.column = column;
					header.fixed = headers[column];
					header.header = true;
					layout.cells << header;
				}
				for (int role = 0 ; role < 3 ; ++role)
				{
					Cell name = field(3 + role * 3, 3, nullptr, QString(), 6, true);
					name.fixed = roles[role];
					Cell who = field(3 + role * 3, 3, nullptr, text(block, role * 2), 7);
					who.column = 1;
					Cell when = field(3 + role * 3, 3, nullptr, text(block, role * 2 + 1), 7);
					when.column = 2;
					layout.cells << name << who << when;
				}
				break;
			}
			case Kind::Format:
				layout.columns = {1.0};
				layout.cells << field(0, 4, QT_TRANSLATE_NOOP("TitleBlockDesign", "Format"),
									  text(block, 0), 8)
							 << field(4, 4, QT_TRANSLATE_NOOP("TitleBlockDesign", "Scale"),
									  text(block, 1), 8)
							 << field(8, 4, QT_TRANSLATE_NOOP("TitleBlockDesign", "Standard"),
									  text(block, 2), 7);
				break;
			case Kind::Sheet: {
				layout.columns = {0.5, 0.5};
				Cell sheet = field(0, 7, QT_TRANSLATE_NOOP("TitleBlockDesign", "Sheet"),
								   text(block, 0), 12, true);
				sheet.columns = 2;
				sheet.align = Qt::AlignHCenter;
				Cell plant = field(7, 5, QT_TRANSLATE_NOOP("TitleBlockDesign", "Installation"),
								   text(block, 1), 7);
				Cell location = field(7, 5, QT_TRANSLATE_NOOP("TitleBlockDesign", "Location"),
									  text(block, 2), 7);
				location.column = 1;
				layout.cells << sheet << plant << location;
				break;
			}
		}
		return layout;
	}

		/// <translation> children of @p parent: the English text and, for
		/// another language, its translation
	void addTranslations(QDomDocument &document, QDomElement &parent,
						 const QString &english, const QString &local)
	{
		QDomElement en = document.createElement(QStringLiteral("translation"));
		en.setAttribute(QStringLiteral("lang"), QStringLiteral("en"));
		en.appendChild(document.createTextNode(english));
		parent.appendChild(en);
		const QString language = QETApp::langFromSetting().section(QLatin1Char('_'), 0, 0);
		if (!language.isEmpty() && language != QLatin1String("en") && local != english) {
			QDomElement other = document.createElement(QStringLiteral("translation"));
			other.setAttribute(QStringLiteral("lang"), language);
			other.appendChild(document.createTextNode(local));
			parent.appendChild(other);
		}
	}

		/// The percents, in whole numbers, of columns ending at @p ends
		/// (fractions of the whole width)
	QList<int> percents(const QList<qreal> &ends)
	{
		QList<int> result;
		int previous = 0;
		for (qreal end : ends)
		{
			const int here = qMax(previous + 1, qRound(end * 100));
			result << here - previous;
			previous = here;
		}
		return result;
	}

		/// The columns of the template, and the first one of each block
	struct Columns
	{
		QStringList widths;
		QList<int> first;
		bool bar = false;
	};

	Columns columnsOf(const Design &design, const QList<Layout> &layouts)
	{
		Columns columns;
		columns.bar = design.style == Style::Modern && design.accent_bar;
		if (columns.bar) columns.widths << QString::number(accent_width);

		int total = 0;
		for (const Block &block : design.blocks) total += qMax(1, block.width);
		QList<qreal> ends;
		qreal start = 0;
		for (int i = 0 ; i < design.blocks.size() ; ++i)
		{
			const qreal width = qMax(1, design.blocks.at(i).width) / qreal(total);
			columns.first << columns.widths.size() + ends.size();
			qreal share = 0;
			for (qreal column : layouts.at(i).columns)
			{
				share += column;
				ends << start + width * share;
			}
			start += width;
		}
		const QList<int> values = percents(ends);
		for (int i = 0 ; i < values.size() ; ++i)
		{
				//The last one takes what is left: the bar, the rounding
			columns.widths << (i == values.size() - 1
							   ? QStringLiteral("r100%")
							   : QStringLiteral("t%1%").arg(values.at(i)));
		}
		return columns;
	}

	QString styleToString(Style style)
	{
		return style == Style::Classic ? QStringLiteral("classic") : QStringLiteral("modern");
	}
}

QList<Kind> kinds()
{
	return {Kind::Logo, Kind::Company, Kind::Project, Kind::Client,
			Kind::Revisions, Kind::Signatures, Kind::Format, Kind::Sheet};
}

QString kindName(Kind kind)
{
	switch (kind)
	{
		case Kind::Logo: return translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Logo"));
		case Kind::Company: return translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Company"));
		case Kind::Project: return translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Project"));
		case Kind::Client: return translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Client and site"));
		case Kind::Revisions: return translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Revisions"));
		case Kind::Signatures: return translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Signatures"));
		case Kind::Format: return translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Format and scale"));
		case Kind::Sheet: return translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Sheet"));
	}
	return QString();
}

QString kindToString(Kind kind)
{
	switch (kind)
	{
		case Kind::Logo: return QStringLiteral("logo");
		case Kind::Company: return QStringLiteral("company");
		case Kind::Project: return QStringLiteral("project");
		case Kind::Client: return QStringLiteral("client");
		case Kind::Revisions: return QStringLiteral("revisions");
		case Kind::Signatures: return QStringLiteral("signatures");
		case Kind::Format: return QStringLiteral("format");
		case Kind::Sheet: return QStringLiteral("sheet");
	}
	return QString();
}

Kind kindFromString(const QString &text, bool *ok)
{
	for (Kind kind : kinds())
	{
		if (kindToString(kind) == text) {
			if (ok) *ok = true;
			return kind;
		}
	}
	if (ok) *ok = false;
	return Kind::Project;
}

int defaultWidth(Kind kind)
{
	switch (kind)
	{
		case Kind::Logo: return 7;
		case Kind::Company: return 13;
		case Kind::Project: return 18;
		case Kind::Client: return 12;
		case Kind::Revisions: return 20;
		case Kind::Signatures: return 13;
		case Kind::Format: return 8;
		case Kind::Sheet: return 9;
	}
	return 10;
}

QStringList fieldNames(Kind kind)
{
	switch (kind)
	{
		case Kind::Company:
			return {translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Name")),
					translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Address")),
					translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Contact"))};
		case Kind::Project:
			return {translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Project")),
					translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Sheet title"))};
		case Kind::Client:
			return {translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Client")),
					translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Site")),
					translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Address"))};
		case Kind::Signatures:
			return {translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Drawn by")),
					translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Drawn on")),
					translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Checked by")),
					translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Checked on")),
					translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Approved by")),
					translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Approved on"))};
		case Kind::Format:
			return {translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Format")),
					translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Scale")),
					translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Standard"))};
		case Kind::Sheet:
			return {translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Sheet")),
					translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Installation")),
					translated(QT_TRANSLATE_NOOP("TitleBlockDesign", "Location"))};
		case Kind::Logo:
		case Kind::Revisions:
			break;
	}
	return {};
}

QStringList defaultTexts(Kind kind)
{
	switch (kind)
	{
		case Kind::Company:
			return {QStringLiteral("%{company}"), QStringLiteral("%{company-address}"),
					QStringLiteral("%{company-contact}")};
		case Kind::Project:
			return {QStringLiteral("%{projecttitle}"), QStringLiteral("%{title}")};
		case Kind::Client:
			return {QStringLiteral("%{client}"), QStringLiteral("%{site}"),
					QStringLiteral("%{site-address}")};
		case Kind::Signatures:
			return {QStringLiteral("%{author}"), QStringLiteral("%{date}"),
					QStringLiteral("%{checked-by}"), QStringLiteral("%{checked-date}"),
					QStringLiteral("%{approved-by}"), QStringLiteral("%{approved-date}")};
		case Kind::Format:
			return {QStringLiteral("%{paper}"), QStringLiteral("%{scale}"),
					QStringLiteral("%{standard}")};
		case Kind::Sheet:
			return {QStringLiteral("%{folio}"), QStringLiteral("%{plant}"),
					QStringLiteral("%{locmach}")};
		case Kind::Logo:
		case Kind::Revisions:
			break;
	}
	return {};
}

QString text(const Block &block, int index)
{
	if (index >= 0 && index < block.texts.size()) return block.texts.at(index);
	const QStringList texts = defaultTexts(block.kind);
	return index >= 0 && index < texts.size() ? texts.at(index) : QString();
}

Design completeDesign()
{
	Design design;
	design.name = QStringLiteral("fasecad-a3");
	for (Kind kind : kinds())
	{
		Block block;
		block.kind = kind;
		block.width = defaultWidth(kind);
		design.blocks << block;
	}
	return design;
}

Design compactDesign()
{
	Design design;
	design.name = QStringLiteral("fasecad-a4");
	design.revision_rows = 2;
	const QList<QPair<Kind, int>> blocks {{Kind::Logo, 11}, {Kind::Project, 35},
										  {Kind::Revisions, 34}, {Kind::Sheet, 20}};
	for (const auto &pair : blocks)
	{
		Block block;
		block.kind = pair.first;
		block.width = pair.second;
		design.blocks << block;
	}
	return design;
}

QList<QPair<QString, QString>> variables()
{
	const QList<QPair<QString, const char *>> list {
		{QStringLiteral("projecttitle"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Project title")},
		{QStringLiteral("title"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Sheet title")},
		{QStringLiteral("folio"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Sheet number")},
		{QStringLiteral("folio-id"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Position of the sheet")},
		{QStringLiteral("folio-total"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Number of sheets")},
		{QStringLiteral("author"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Author")},
		{QStringLiteral("date"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Date")},
		{QStringLiteral("plant"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Installation")},
		{QStringLiteral("locmach"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Location")},
		{QStringLiteral("indexrev"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Revision of the sheet")},
		{QStringLiteral("version"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Version")},
		{QStringLiteral("filename"), QT_TRANSLATE_NOOP("TitleBlockDesign", "File name")},
		{QStringLiteral("company"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Company")},
		{QStringLiteral("company-address"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Company address")},
		{QStringLiteral("company-contact"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Company contact")},
		{QStringLiteral("client"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Client")},
		{QStringLiteral("site"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Site")},
		{QStringLiteral("site-address"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Site address")},
		{QStringLiteral("checked-by"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Checked by")},
		{QStringLiteral("checked-date"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Checked on")},
		{QStringLiteral("approved-by"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Approved by")},
		{QStringLiteral("approved-date"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Approved on")},
		{QStringLiteral("standard"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Standard")},
		{QStringLiteral("paper"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Paper size")},
		{QStringLiteral("scale"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Scale")},
		{QStringLiteral("revision1-index"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Latest revision")},
		{QStringLiteral("revision1-date"), QT_TRANSLATE_NOOP("TitleBlockDesign", "Date of the latest revision")}};
	QList<QPair<QString, QString>> result;
	for (const auto &pair : list) result << qMakePair(pair.first, translated(pair.second));
	return result;
}

void normalizeWidths(Design &design)
{
	if (design.blocks.isEmpty()) return;
	int total = 0;
	for (Block &block : design.blocks)
	{
		block.width = qMax(3, block.width);
		total += block.width;
	}
	if (total == 100) return;
		//Largest remainders, so the widths add up to 100 exactly
	QList<QPair<qreal, int>> remainders;
	int given = 0;
	for (int i = 0 ; i < design.blocks.size() ; ++i)
	{
		const qreal exact = design.blocks.at(i).width * 100.0 / total;
		design.blocks[i].width = qMax(1, int(exact));
		given += design.blocks.at(i).width;
		remainders << qMakePair(exact - int(exact), i);
	}
	std::sort(remainders.begin(), remainders.end(),
			  [](const QPair<qreal, int> &a, const QPair<qreal, int> &b) {return a.first > b.first;});
	for (int i = 0 ; given < 100 ; ++i, ++given)
	{
		design.blocks[remainders.at(i % remainders.size()).second].width += 1;
	}
		//Blocks kept at 3 % may give more than 100: the widest pay
	while (given > 100)
	{
		auto widest = std::max_element(design.blocks.begin(), design.blocks.end(),
									   [](const Block &a, const Block &b) {return a.width < b.width;});
		if (widest->width <= 1) break;
		widest->width -= 1;
		--given;
	}
}

QList<QPair<int, int>> blockSpans(const Design &design, int width)
{
	QList<QPair<int, int>> spans;
	QList<Layout> layouts;
	for (const Block &block : design.blocks) layouts << layoutOf(block, design);
	const Columns columns = columnsOf(design, layouts);
	TitleBlockTemplate *built = build(design);
	if (!built) return spans;
	const QList<int> widths = built->columnsWidth(width);
	delete built;
	auto x_of = [&widths](int column) {
		int x = 0;
		for (int i = 0 ; i < column && i < widths.size() ; ++i) x += widths.at(i);
		return x;
	};
	for (int i = 0 ; i < design.blocks.size() ; ++i)
	{
		const int first = columns.first.at(i);
		const int last = first + layouts.at(i).columns.size();
		spans << qMakePair(i == 0 ? 0 : x_of(first), x_of(last));
	}
	return spans;
}

QDomElement templateXml(const Design &design, QDomDocument &document)
{
	QDomElement root = document.createElement(QStringLiteral("titleblocktemplate"));
	root.setAttribute(QStringLiteral("name"),
					  design.name.isEmpty() ? QStringLiteral("design") : design.name);

	QDomElement information = document.createElement(QStringLiteral("information"));
	information.appendChild(document.createTextNode(
			translated(QT_TRANSLATE_NOOP("TitleBlockDesign",
										 "Made of blocks with the FaseCAD title block designer"))));
	root.appendChild(information);

	QDomElement logos = document.createElement(QStringLiteral("logos"));
	if (!design.logo.isEmpty()) {
		QDomElement logo = document.createElement(QStringLiteral("logo"));
		logo.setAttribute(QStringLiteral("name"), QStringLiteral("logo"));
		logo.setAttribute(QStringLiteral("type"),
						  design.logo_type == QLatin1String("svg")
						  ? QStringLiteral("svg") : QStringLiteral("png"));
		logo.setAttribute(QStringLiteral("storage"), QStringLiteral("base64"));
		logo.appendChild(document.createTextNode(QString::fromLatin1(design.logo.toBase64())));
		logos.appendChild(logo);
	}
	root.appendChild(logos);

	QList<Layout> layouts;
	for (const Block &block : design.blocks) layouts << layoutOf(block, design);
	const Columns columns = columnsOf(design, layouts);

	QStringList rows;
	const int height = qMax(rows_count, design.height);
	for (int row = 0 ; row < rows_count ; ++row)
	{
		rows << QString::number(height / rows_count + (row < height % rows_count ? 1 : 0));
	}

	QDomElement grid = document.createElement(QStringLiteral("grid"));
	grid.setAttribute(QStringLiteral("rows"), rows.join(QLatin1Char(';')) + QLatin1Char(';'));
	grid.setAttribute(QStringLiteral("cols"), columns.widths.join(QLatin1Char(';')) + QLatin1Char(';'));
	root.appendChild(grid);

	const bool modern = design.style == Style::Modern;
	QColor header_fill = design.accent;
	header_fill.setAlpha(38);

	auto add_cell = [&](const Cell &cell, int first_column, const QString &name) {
		if (cell.logo) {
			QDomElement logo = document.createElement(QStringLiteral("logo"));
			logo.setAttribute(QStringLiteral("row"), cell.row);
			logo.setAttribute(QStringLiteral("col"), first_column + cell.column);
			if (cell.rows > 1) logo.setAttribute(QStringLiteral("rowspan"), cell.rows - 1);
			if (cell.columns > 1) logo.setAttribute(QStringLiteral("colspan"), cell.columns - 1);
			logo.setAttribute(QStringLiteral("name"), name);
			logo.setAttribute(QStringLiteral("resource"), QStringLiteral("logo"));
			grid.appendChild(logo);
			return;
		}
		QDomElement element = document.createElement(QStringLiteral("field"));
		element.setAttribute(QStringLiteral("row"), cell.row);
		element.setAttribute(QStringLiteral("col"), first_column + cell.column);
		if (cell.rows > 1) element.setAttribute(QStringLiteral("rowspan"), cell.rows - 1);
		if (cell.columns > 1) element.setAttribute(QStringLiteral("colspan"), cell.columns - 1);
		element.setAttribute(QStringLiteral("name"), name);
		element.setAttribute(QStringLiteral("fontsize"), cell.font_size);
		element.setAttribute(QStringLiteral("align"),
							 cell.align & Qt::AlignHCenter ? QStringLiteral("center")
														   : QStringLiteral("left"));
		element.setAttribute(QStringLiteral("valign"), QStringLiteral("center"));
		element.setAttribute(QStringLiteral("hadjust"), QStringLiteral("true"));
		element.setAttribute(QStringLiteral("displaylabel"),
							 cell.label ? QStringLiteral("true") : QStringLiteral("false"));
		if (cell.bold) {
			QFont font = QETApp::diagramTextsFont(cell.font_size);
			font.setBold(true);
			element.setAttribute(QStringLiteral("value-font"), font.toString());
		}
		if (modern && cell.label) element.setAttribute(QStringLiteral("stacked"), QStringLiteral("true"));
		if (cell.accent) {
			element.setAttribute(QStringLiteral("fill"), design.accent.name(QColor::HexArgb));
			element.setAttribute(QStringLiteral("borders"), QStringLiteral("none"));
		} else if (modern && cell.header) {
			element.setAttribute(QStringLiteral("fill"), header_fill.name(QColor::HexArgb));
		}
		if (cell.borders != TitleBlockCell::allBorders()) {
			element.setAttribute(QStringLiteral("borders"), TitleBlockCell::bordersToString(cell.borders));
		}

		QDomElement value = document.createElement(QStringLiteral("value"));
		if (cell.fixed) {
			addTranslations(document, value, QString::fromLatin1(cell.fixed), translated(cell.fixed));
		} else {
			addTranslations(document, value, cell.text, cell.text);
		}
		element.appendChild(value);
		QDomElement label = document.createElement(QStringLiteral("label"));
		if (cell.label) {
			addTranslations(document, label, QString::fromLatin1(cell.label), translated(cell.label));
		}
		element.appendChild(label);
		grid.appendChild(element);
	};

	if (columns.bar) {
		Cell bar;
		bar.accent = true;
		add_cell(bar, 0, QStringLiteral("accent"));
	}
	for (int i = 0 ; i < design.blocks.size() ; ++i)
	{
		const QList<Cell> &cells = layouts.at(i).cells;
		for (int j = 0 ; j < cells.size() ; ++j)
		{
			add_cell(cells.at(j), columns.first.at(i),
					 QStringLiteral("%1-%2").arg(kindToString(design.blocks.at(i).kind)).arg(j + 1));
		}
	}

		//The design, for the designer to open it again
	QDomElement kept = document.createElement(QStringLiteral("fasecad-design"));
	kept.setAttribute(QStringLiteral("style"), styleToString(design.style));
	kept.setAttribute(QStringLiteral("height"), design.height);
	kept.setAttribute(QStringLiteral("accent"), design.accent.name());
	kept.setAttribute(QStringLiteral("accentbar"),
					  design.accent_bar ? QStringLiteral("true") : QStringLiteral("false"));
	kept.setAttribute(QStringLiteral("revisionrows"), design.revision_rows);
	for (const Block &block : design.blocks)
	{
		QDomElement element = document.createElement(QStringLiteral("block"));
		element.setAttribute(QStringLiteral("kind"), kindToString(block.kind));
		element.setAttribute(QStringLiteral("width"), block.width);
		for (int i = 0 ; i < block.texts.size() ; ++i)
		{
			QDomElement text = document.createElement(QStringLiteral("text"));
			text.setAttribute(QStringLiteral("index"), i);
			text.appendChild(document.createTextNode(block.texts.at(i)));
			element.appendChild(text);
		}
		kept.appendChild(element);
	}
	root.appendChild(kept);
	return root;
}

TitleBlockTemplate *build(const Design &design, QObject *parent)
{
	QDomDocument document;
	const QDomElement xml = templateXml(design, document);
	auto *built = new TitleBlockTemplate(parent);
	if (!built->loadFromXmlElement(xml)) {
		delete built;
		return nullptr;
	}
	return built;
}

bool fromTemplateXml(const QDomElement &template_element, Design &design)
{
	const QDomElement kept = template_element.firstChildElement(QStringLiteral("fasecad-design"));
	if (kept.isNull()) return false;

	Design read;
	read.name = template_element.attribute(QStringLiteral("name"));
	read.style = kept.attribute(QStringLiteral("style")) == QLatin1String("classic")
			? Style::Classic : Style::Modern;
	read.height = qBound(24, kept.attribute(QStringLiteral("height"), QStringLiteral("72")).toInt(), 400);
	const QColor accent(kept.attribute(QStringLiteral("accent")));
	if (accent.isValid()) read.accent = accent;
	read.accent_bar = kept.attribute(QStringLiteral("accentbar"), QStringLiteral("true")) == QLatin1String("true");
	read.revision_rows = kept.attribute(QStringLiteral("revisionrows"), QStringLiteral("3")).toInt();
	for (QDomElement element = kept.firstChildElement(QStringLiteral("block")) ;
		 !element.isNull() ; element = element.nextSiblingElement(QStringLiteral("block")))
	{
		bool ok = false;
		Block block;
		block.kind = kindFromString(element.attribute(QStringLiteral("kind")), &ok);
		if (!ok) continue;
		block.width = element.attribute(QStringLiteral("width")).toInt();
		for (QDomElement text = element.firstChildElement(QStringLiteral("text")) ;
			 !text.isNull() ; text = text.nextSiblingElement(QStringLiteral("text")))
		{
			const int index = text.attribute(QStringLiteral("index")).toInt();
			if (index < 0 || index > 20) continue;
			while (block.texts.size() <= index) block.texts << TitleBlockDesign::text(block, block.texts.size());
			block.texts[index] = text.text();
		}
		read.blocks << block;
	}

		//The logo is the template's own
	const QDomElement logos = template_element.firstChildElement(QStringLiteral("logos"));
	for (QDomElement logo = logos.firstChildElement(QStringLiteral("logo")) ;
		 !logo.isNull() ; logo = logo.nextSiblingElement(QStringLiteral("logo")))
	{
		if (logo.attribute(QStringLiteral("name")) != QLatin1String("logo")) continue;
		read.logo_type = logo.attribute(QStringLiteral("type"));
		if (logo.attribute(QStringLiteral("storage"), QStringLiteral("base64")) == QLatin1String("xml")) {
			QTextStream stream(&read.logo);
			logo.firstChildElement().save(stream, 0);
		} else {
			read.logo = QByteArray::fromBase64(logo.text().toLatin1());
		}
	}
	design = read;
	return true;
}

bool Revision::operator==(const Revision &other) const
{
	return index == other.index && date == other.date
			&& description == other.description && author == other.author;
}

QList<Revision> revisions(const DiagramContext &properties)
{
	QList<Revision> list;
	for (int n = 1 ; n < 1000 ; ++n)
	{
		const QString prefix = QStringLiteral("revision%1-").arg(n);
		if (!properties.contains(prefix + QLatin1String("index"))
			&& !properties.contains(prefix + QLatin1String("description"))) {
			break;
		}
		Revision revision;
		revision.index = properties.value(prefix + QLatin1String("index")).toString();
		revision.date = properties.value(prefix + QLatin1String("date")).toString();
		revision.description = properties.value(prefix + QLatin1String("description")).toString();
		revision.author = properties.value(prefix + QLatin1String("author")).toString();
		list << revision;
	}
	return list;
}

void setRevisions(DiagramContext &properties, const QList<Revision> &list)
{
	static const QRegularExpression revision_key(
			QStringLiteral("^revision\\d+-(index|date|description|author)$"));
	for (const QString &key : properties.keys())
	{
		if (revision_key.match(key).hasMatch()) properties.remove(key);
	}
	for (int i = 0 ; i < list.size() ; ++i)
	{
		const QString prefix = QStringLiteral("revision%1-").arg(i + 1);
		properties.addValue(prefix + QLatin1String("index"), list.at(i).index);
		properties.addValue(prefix + QLatin1String("date"), list.at(i).date);
		properties.addValue(prefix + QLatin1String("description"), list.at(i).description);
		properties.addValue(prefix + QLatin1String("author"), list.at(i).author);
	}
}

QString nextIndex(const QString &index)
{
	const QString trimmed = index.trimmed();
	if (trimmed.isEmpty()) return QStringLiteral("A");

	static const QRegularExpression number(QStringLiteral("^(.*?)(\\d+)$"));
	const QRegularExpressionMatch match = number.match(trimmed);
	if (match.hasMatch()) {
		const QString digits = match.captured(2);
		const QString next = QString::number(digits.toLongLong() + 1);
		return match.captured(1) + next.rightJustified(digits.size(), QLatin1Char('0'));
	}

		//Letters: A -> B, Z -> AA, as columns are named
	QString letters = trimmed;
	int i = letters.size() - 1;
	while (i >= 0)
	{
		const QChar c = letters.at(i);
		if (c == QLatin1Char('Z') || c == QLatin1Char('z')) {
			letters[i] = c == QLatin1Char('Z') ? QLatin1Char('A') : QLatin1Char('a');
			--i;
			continue;
		}
		if (!c.isLetter()) return trimmed + QStringLiteral("1");
		letters[i] = QChar(c.unicode() + 1);
		return letters;
	}
	return (trimmed.at(0).isLower() ? QStringLiteral("a") : QStringLiteral("A")) + letters;
}
}
