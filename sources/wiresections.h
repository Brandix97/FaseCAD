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
#ifndef WIRESECTIONS_H
#define WIRESECTIONS_H

#include <QCoreApplication>
#include <QList>
#include <QPointer>
#include <QString>

class Conductor;
class Element;
class QETProject;
class QUndoCommand;
class Terminal;

/**
	@brief The cross-section of the wires a protection feeds (Brandon,
	2026-11-19): a circuit breaker, a motor protection, a fuse of rated
	current In asks for wires carrying at least In, per IEC 60204-1 table
	6 (copper, PVC, 40 °C), corrected for the ambient temperature (table
	D.1) and for the circuits grouped together (table D.2), and never under
	0.75 mm² for a power circuit in an enclosure (table 5).

	The protections are the symbols of kind "protection"; their rated
	current is their information "Rated current" (16 A, C16...), else the
	rated current of their first part in the parts database. The wires
	they feed are those on their outgoing terminals -- the even ones (2, 4,
	6, 2/T1...), else those at the bottom of the symbol -- and every wire
	joined to them.
*/
namespace WireSections
{
		/// Installation methods of IEC 60204-1 annex D
	enum class Method { B1, B2, C, E };

	struct Conditions
	{
		Method method = Method::B1;
			/// °C
		int ambient = 40;
			/// Circuits grouped together
		int grouped = 1;
	};

		/// The sections of the table, mm²
	QList<qreal> sections();
		/// Iz of @p section in mm² in @p conditions, 0 when off the table
	qreal capacity(qreal section, const Conditions &conditions);
		/// The smallest section carrying @p current amperes, 0 when none
	qreal minimumSection(qreal current, const Conditions &conditions);

		/// "2,5 mm²" -> 2.5, 0 when there is no number
	qreal parseSection(const QString &text);
	QString sectionText(qreal section);
		/// "16 A", "C16", "In = 10 A" -> 16, 16, 10; 0 when none
	qreal parseCurrent(const QString &text);

	bool isProtection(const Element *element);
	qreal ratedCurrent(const Element *element);
	QList<Terminal *> outgoing(const Element *element);

		/// One protection and the wires it feeds
	struct Row
	{
		QPointer<Element> protection;
		QString label;
		qreal current = 0;
		qreal required = 0;
		QList<QPointer<Conductor>> wires;
			/// Wires with no section, and with one under the required one
		int without = 0;
		int too_small = 0;
	};

	QList<Row> check(QETProject *project, const Conditions &conditions);
		/// The undo step giving the required section to the wires of
		/// @p rows with none or a smaller one; nullptr when there is none
	QUndoCommand *apply(const QList<Row> &rows);
}

#endif // WIRESECTIONS_H
