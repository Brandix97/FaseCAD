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
#ifndef PARTSCABLETYPES_H
#define PARTSCABLETYPES_H

#include "partsdatabase.h"
#include "../cablelist/cabletypelist.h"

#include <QList>
#include <QStringList>

/**
	@brief The parts of the parts database which are cables, as lines of
	the cable type file : the window choosing the type of a cable offers
	them beside the lines of that file.

	Pure values : no database, no widget, so it can be tested alone.
*/
class PartsCableTypes
{
	Q_DECLARE_TR_FUNCTIONS(PartsCableTypes)

	public:
			/// The key of the column saying where a line comes from;
			/// no column of the cable type file
		static QString sourceColumn();
			/// The columns a line made from a part fills in
		static QStringList columns();
		static QList<CableTypeRecord> records(const QList<Part> &parts);
		static bool isFromPartsDatabase(const CableTypeRecord &record);
		static QString typeName(const Part &part);
};

#endif // PARTSCABLETYPES_H
