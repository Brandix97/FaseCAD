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
#ifndef TITLEBLOCKDESIGN_H
#define TITLEBLOCKDESIGN_H

#include "../diagramcontext.h"

#include <QColor>
#include <QDomDocument>
#include <QDomElement>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

class QObject;
class TitleBlockTemplate;

/**
	@brief A title block made of blocks (Brandon, 2026-11-19): instead of a
	grid of cells, a row of ready-made blocks -- logo, company, project,
	client and site, revisions, signatures, format and scale, sheet --
	put in order and given a width. The design becomes an ordinary
	title block template (build(), templateXml()): twelve rows, the
	columns of each block, cells with the label small above the value in
	the modern style. The design itself rides along in the template, in
	a \<fasecad-design\> element other programs leave alone, so the
	designer opens it again as blocks.

	The data the blocks show are folio and project variables:
	%{projecttitle}, %{title}, %{folio}, %{author}, %{date}... and the
	project's %{company}, %{client}, %{site}, %{checked-by},
	%{revision1-index}... (TitleBlockDesign::Revision), %{paper} and
	%{scale} worked out for each folio.
*/
namespace TitleBlockDesign
{
	enum class Kind { Logo, Company, Project, Client, Revisions, Signatures, Format, Sheet };
	enum class Style { Modern, Classic };

	struct Block
	{
		Kind kind = Kind::Project;
			/// Percent of the title block's width
		int width = 10;
			/// What each field shows, text and %{variables}; a field
			/// left out shows its default (defaultTexts())
		QStringList texts;
	};

	struct Design
	{
		QString name;
		Style style = Style::Modern;
			/// Height in scene pixels (96 per inch)
		int height = 72;
		QColor accent = QColor(0xFA, 0x13, 0x0C);
			/// A thin bar of the accent colour along the left side
		bool accent_bar = true;
			/// Rows of the revisions table: 2, 3 or 5
		int revision_rows = 3;
		QList<Block> blocks;
			/// The logo's file, empty for none, and its type: "svg" or
			/// "png" (any image Qt reads)
		QByteArray logo;
		QString logo_type;
	};

	QList<Kind> kinds();
	QString kindName(Kind kind);
	QString kindToString(Kind kind);
	Kind kindFromString(const QString &text, bool *ok = nullptr);
	int defaultWidth(Kind kind);
		/// The names of the fields of a block one can change the text of
	QStringList fieldNames(Kind kind);
	QStringList defaultTexts(Kind kind);
		/// The text field @p index of @p block shows
	QString text(const Block &block, int index);

		/// Logo, company, project, client, revisions, signatures, format
		/// and sheet: for an A3 folio
	Design completeDesign();
		/// Logo, project, revisions and sheet: for an A4 folio
	Design compactDesign();

		/// The variables a field can show, as (name, what it is)
	QList<QPair<QString, QString>> variables();

		/// The widths of @p design given to 100 %, the blocks kept
	void normalizeWidths(Design &design);
		/// Where each block of @p design lies at @p width pixels:
		/// (left, right) in pixels
	QList<QPair<int, int>> blockSpans(const Design &design, int width);

		/// The \<titleblocktemplate\> of @p design, in @p document
	QDomElement templateXml(const Design &design, QDomDocument &document);
		/// The template of @p design, nullptr when it cannot be built
	TitleBlockTemplate *build(const Design &design, QObject *parent = nullptr);
		/// Read the design @p template_element carries, false when it
		/// carries none
	bool fromTemplateXml(const QDomElement &template_element, Design &design);

	/**
		@brief A revision of the project, kept in its properties as
		revision<n>-index, -date, -description and -author, 1 being the
		latest: the title block's table shows the latest at the top.
	*/
	struct Revision
	{
		QString index;
		QString date;
		QString description;
		QString author;
		bool operator==(const Revision &other) const;
	};
		/// The revisions of @p properties, the latest first
	QList<Revision> revisions(const DiagramContext &properties);
		/// @p properties with @p list as its revisions, the latest first
	void setRevisions(DiagramContext &properties, const QList<Revision> &list);
		/// The index after @p index: B after A, 3 after 2, A after none
	QString nextIndex(const QString &index);
}

#endif // TITLEBLOCKDESIGN_H
