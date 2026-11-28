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
#include "dxfimport.h"

#include <QTransform>

QList<DxfImport::Piece> DxfImport::pieces(const QPainterPath &drawing,
										  double pixels_per_mm)
{
	QList<Piece> result;
	if (drawing.isEmpty() || pixels_per_mm <= 0) return result;

	const QPointF centre = drawing.boundingRect().center();
	QTransform transform;
	transform.scale(pixels_per_mm, pixels_per_mm);
	transform.translate(-centre.x(), -centre.y());

	for (QPolygonF polygon : drawing.toSubpathPolygons(transform)) {
			//Points repeated one after the other say nothing
		QPolygonF points;
		for (const QPointF &p : polygon) {
			if (points.isEmpty() || QLineF(points.last(), p).length() > 1e-6)
				points << p;
		}
		if (points.size() < 2) continue;
		Piece piece;
		if (points.size() > 3 && QLineF(points.first(), points.last()).length() < 1e-6) {
			points.removeLast();
			piece.closed = true;
		}
		piece.points = points;
		result << piece;
	}
	return result;
}
