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
#ifndef CABINETDEVICES_H
#define CABINETDEVICES_H

#include "cabinetitem.h"

#include <QCoreApplication>
#include <QList>
#include <QString>
#include <QUuid>

class Diagram;
class QETProject;

/**
	@brief The devices of the schematic drawn on the cabinet folios of a
	project, and the drawing of a device read from the DXF of its parts.

	A device's drawing is a CabinetItem of kind Drawing that names the
	symbol it stands for and its label. Which devices are drawn is read
	from the folios whether they are built or not (lazy folios): a folio
	not built yet is read from the XML kept for it.
*/
class CabinetDevices
{
	Q_DECLARE_TR_FUNCTIONS(CabinetDevices)

	public:
			/// A device drawn on a cabinet folio
		struct Placed {
			QUuid device;		///< the symbol it stands for
			QString label;		///< as written on the drawing
			int folio = 0;		///< its cabinet folio, 1 for the first folio
		};

		static QList<Diagram *> cabinetFolios(QETProject *project);
		static QList<Placed> placed(QETProject *project);

		static bool drawingFromParts(const QString &parts,
									 CabinetItem::Properties &properties,
									 QString *error = nullptr);
};

#endif // CABINETDEVICES_H
