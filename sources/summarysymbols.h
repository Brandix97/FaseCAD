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
#ifndef SUMMARYSYMBOLS_H
#define SUMMARYSYMBOLS_H

#include <QList>
#include <QStringList>
#include <QRectF>
#include <QDomDocument>
#include <QString>

class Diagram;
class QETProject;

/**
	@brief Function texts and summary symbols (Brandon, 2026-11-28).

	A function text is a free text marked so (IndependentTextItem::
	isFunctionText()): a symbol drawn over or under it, within its width,
	has that text as function, unless it has a function of its own. The
	function is worked out, not saved (Element::setComputedInfo()).

	A summary symbol, put on a summary folio, stands for a terminal of a
	device drawn on the schematic: the one with its label and, for a PLC
	summary symbol, the terminal it names (its information
	"summary_pin", X01:4). Its texts can show that terminal, its function
	(its own or its function text's) and its place, written as the
	project writes its cross references. A network summary symbol without
	a terminal stands for the main part of its device.
*/
namespace SummarySymbols
{
		/// The function text of @p folio over or under @p rect (in the
		/// folio's coordinates), within its width: the nearest; empty if none
	QString functionTextAt(Diagram *folio, const QRectF &rect);
		/// The functions, references and terminals shown on the built
		/// folios of @p project, worked out again
	void refresh(QETProject *project);
		/// The symbol a new PLC summary symbol starts with: a box with
		/// four terminals, X1:1 to X1:4, each with its function text and
		/// place on its left, to change at will
	QDomDocument plcExampleDefinition();
		/// A PLC summary symbol for @p pins: a box, a terminal for each pin
		/// (on the left, or half on each side in @p two_columns), its
		/// function text and place beside it, following it by its name
	QDomDocument plcSummaryDefinition(const QStringList &pins, bool two_columns);
		/// "X1:1-X1:32, PE" as names: X1:1, X1:2 ... X1:32, PE.
		/// I0.0-I3.7 is I0.0 to I0.7, then I1.0 ... I3.7
	QStringList expandPattern(const QString &pattern);
		/// The labels of the devices of @p project, in natural order
	QStringList deviceLabels(QETProject *project);
		/// The names of the terminals of the device @p label of @p project,
		/// from the definitions of its symbols, in natural order
	QStringList deviceTerminals(QETProject *project, const QString &label);
}

#endif // SUMMARYSYMBOLS_H
