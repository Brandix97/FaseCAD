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
#ifndef CABINETCLEARANCES_H
#define CABINETCLEARANCES_H

#include <QMarginsF>

class CabinetItem;
class Diagram;

/**
	The free space each device of a cabinet folio asks around it, from the
	mounting clearances of its part in the parts database (Brandon,
	2026-11-25): while a DXF is placed on the folio, they are drawn in
	red, the one being placed too, so it is put down out of them.
*/
namespace CabinetClearances
{
		/// The clearance of the part of @p device (mm: left, top, right,
		/// bottom), none when it has no part or its part none
	QMarginsF of(const CabinetItem *device);
		/// Give every device drawing of @p folio, and @p placing when not
		/// null, the clearance of its part, and draw them in red
	void startPlacing(Diagram *folio, CabinetItem *placing = nullptr);
		/// Back to how they were drawn before
	void endPlacing(Diagram *folio);
}

#endif // CABINETCLEARANCES_H
