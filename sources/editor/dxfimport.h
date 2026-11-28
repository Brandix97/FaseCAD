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
#ifndef DXFIMPORT_H
#define DXFIMPORT_H

#include <QList>
#include <QPainterPath>
#include <QPolygonF>

/**
	@brief The DxfImport namespace
	A DXF drawing in the element editor, read by the DXF reader of the
	cabinet folios (DxfReader): its lines become the lines and polylines
	of the symbol, curves as short straight pieces.
*/
namespace DxfImport
{
		/// Scene pixels for one millimetre: real size on paper, at 96 px
		/// an inch, as the A4 sheet of the editor
	constexpr double realSize = 96.0 / 25.4;

	struct Piece {
		QPolygonF points;
		bool closed = false;
	};

		/// The pieces of @p drawing (millimetres, as DxfReader gives it)
		/// at @p pixels_per_mm, centred on the hotspot (0, 0)
	QList<Piece> pieces(const QPainterPath &drawing, double pixels_per_mm);
}

#endif // DXFIMPORT_H
