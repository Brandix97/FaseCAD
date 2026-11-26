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
#ifndef CABINETPARTS_H
#define CABINETPARTS_H

#include "../partsdb/partsdatabase.h"
#include "../partsdb/partslinks.h"

#include <QHash>
#include <QUuid>

#include <memory>

class Diagram;

/**
	What the parts database says of the devices of a cabinet folio
	(Brandon, 2026-11-25): the parts of each device of the schematic, by
	its symbol or else its label, read once for all of them. The free
	space of the parts (CabinetClearances) and their heat
	(CabinetThermal) read them here.
*/
class CabinetParts
{
	public:
			/// The parts of the devices of the project of @p folio; the
			/// folios holding @p symbols are built for it
		CabinetParts(Diagram *folio, const QSet<QUuid> &symbols = QSet<QUuid>());

		bool isOpen() const {return bool(m_database);}
			/// The parts of the device of symbol @p symbol, else of
			/// label @p label
		QList<PartsLinks::Link> linksOf(const QUuid &symbol, const QString &label) const;
		std::optional<Part> part(const PartsLinks::Link &link) const;
		std::optional<Part> part(const QString &manufacturer, const QString &order_number) const;

	private:
		std::unique_ptr<PartsDatabase> m_database;
		QHash<QUuid, QString> m_by_symbol;
		QHash<QString, QString> m_by_label;
};

#endif // CABINETPARTS_H
