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
#ifndef WIRENUMBERING_H
#define WIRENUMBERING_H

#include <QCoreApplication>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QString>

class Conductor;
class Diagram;
class QETProject;
class QUndoCommand;

/**
	@brief Numbering the wires of a project at once (Brandon, 2026-11-19):
	by a formula, from one of the examples or the user's own.

	A formula is text with variables:
	 - %N  a running number over the project;
	 - %n  a running number starting again on each sheet;
	 - %F  the sheet, as its title block numbers it;
	 - %C  the column and %R the row of the sheet where the wire starts;
	 - %P  the potential type with its own running number (L1, L2, N, PE,
		   P1 for 24 V, M1 for 0 V; a plain number without a type);
	 - %S  the device and %T the terminal the wire starts from, %D and %E
		   those it goes to;
	 - %%  a percent sign.
	A number between % and the letter is its width, with zeros in front:
	%3N gives 001.

	A formula names a potential -- every wire joined to it, also through
	the sheet references -- or each wire on its own (as the device and
	terminal names do). Two potentials never get the same name: the second
	one gets ".2". The wires whose number is fixed by hand keep it.
*/
namespace WireNumbering
{
	struct Scheme
	{
		QString name;
		QString formula;
			/// A name for each wire rather than for each potential
		bool per_wire = false;
	};

		/// The six examples
	QList<Scheme> examples();
		/// The user's own formulas, kept in the settings
	QList<Scheme> ownSchemes();
	void setOwnSchemes(const QList<Scheme> &schemes);

	enum class Scope { Project, Sheet, Selection };
		/// The order wires are numbered in on a sheet
	enum class Order { RowsFirst, ColumnsFirst };

	struct Options
	{
		QString formula = QStringLiteral("%N");
		bool per_wire = false;
		Scope scope = Scope::Project;
			/// The sheet for Scope::Sheet
		Diagram *sheet = nullptr;
			/// The wires for Scope::Selection
		QList<Conductor *> selection;
		Order order = Order::RowsFirst;
		int start = 1;
			/// Keep the numbers fixed by hand (Conductor::isFreezeLabel())
		bool keep_fixed = true;
	};

		/// One name given: a potential, or a wire
	struct Change
	{
		QList<QPointer<Conductor>> conductors;
		QString before;
		QString after;
		QPointer<Diagram> sheet;
		QString from;
		QString to;
	};

		/// What numbering @p project with @p options would change
	QList<Change> plan(QETProject *project, const Options &options);
		/// The undo step making @p changes, nullptr when there is none
	QUndoCommand *command(const QList<Change> &changes);
		/// @p formula with the values of one wire, for a preview
	QString evaluate(const QString &formula, const QHash<QChar, QString> &values,
					 int number, int sheet_number);
}

#endif // WIRENUMBERING_H
