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
#ifndef PROJECTVERSIONS_H
#define PROJECTVERSIONS_H

#include <QCoreApplication>
#include <QDomElement>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

class Diagram;
class QETProject;

/**
	The versions of a project (Brandon, 2026-11-27): "Set version" raises
	the version of the project and, from the second one on, adds a sheet
	with what changed since the version before: the devices added, the ones
	removed and the ones whose wiring changed. A device moved is no change.

	A device is the symbols sharing a label; its wiring is, for each of its
	terminals, the terminals of the other devices it is joined to by wires
	(over folio reports too), so redrawing a wire the same way is no change
	either. The project keeps the picture of the last version only.
*/
namespace ProjectVersions
{

	struct Device
	{
		QString description;
			/// "13 → -S1:14", sorted
		QStringList wiring;
		bool operator==(const Device &other) const {
			return description == other.description && wiring == other.wiring;
		}
	};

		/// The devices of a project, by label
	using Snapshot = QMap<QString, Device>;

	struct Version
	{
		int number = 0;
			/// ISO date and time, UTC
		QString date;
	};

	struct History
	{
		QList<Version> versions;
			/// The devices at the last version
		Snapshot last;
		int current() const {return versions.isEmpty() ? 0 : versions.last().number;}
		bool operator==(const History &other) const;
	};

	struct Changes
	{
		QStringList added;
		QStringList removed;
			/// Label -> wiring lines gone ("−") and new ("+")
		QMap<QString, QStringList> rewired;
		bool isEmpty() const {return added.isEmpty() && removed.isEmpty() && rewired.isEmpty();}
	};

		/// The devices of @p project now (its folios are built)
	Snapshot snapshot(QETProject *project);
	Changes compare(const Snapshot &before, const Snapshot &after);
		/// The lines of the sheet of the changes from @p from to @p to
	QStringList report(const Changes &changes, const Snapshot &before, const Snapshot &after,
					   int from, int to);

	QDomElement toXml(const History &history, QDomDocument &document);
	History fromXml(const QDomElement &element);

		/// Raise the version of @p project: the revision index of its title
		/// block (%indexrev) to 1 when it has no number, else one more, on
		/// every folio;
		/// the picture of its devices kept, and the sheets of the changes
		/// since the version kept before added at its end. @return the
		/// sheets added
	QList<Diagram *> setVersion(QETProject *project);
}

#endif // PROJECTVERSIONS_H
