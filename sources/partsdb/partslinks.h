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
#ifndef PARTSLINKS_H
#define PARTSLINKS_H

#include <QList>
#include <QString>

/**
	@brief The parts of the parts database an element is made of, with
	how many units of each.

	Kept in one element information, "parts", one line per part: the
	manufacturer, the order number and the units, separated by tabs. The
	manufacturer and the order number are what identifies a part in the
	database; the file format of the project does not change, and a
	program which does not know the key keeps it as it is.
*/
namespace PartsLinks
{
	struct Link {
		QString manufacturer;
		QString order_number;
		double units = 1;

		bool operator==(const Link &other) const {
			return manufacturer == other.manufacturer
					&& order_number == other.order_number
					&& units == other.units;
		}
	};

		/// The element information holding the list
	inline QString infoKey() {return QStringLiteral("parts");}

	QString encode(const QList<Link> &links);
	QList<Link> decode(const QString &value);
}

#endif // PARTSLINKS_H
