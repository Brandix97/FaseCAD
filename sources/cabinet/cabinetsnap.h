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
#ifndef CABINETSNAP_H
#define CABINETSNAP_H

#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QVector>

class Diagram;
class QGraphicsItem;

/**
	Where things catch on a cabinet folio, the magnet of the status bar on
	(Diagram::gridSnap()): not on the grid, but on the corners and on the
	middles of the sides of the parts already there (plates, rails, ducts,
	drawings), so that a part goes flush with another or in its middle.
	Near no corner and no middle, a part still lines up with the corners
	and middles of the others along one direction, and otherwise moves in
	whole millimetres. Ctrl held, or the magnet off: nothing catches.

	Where something caught is shown by a marker: a square on a corner, a
	triangle on the middle of a side.
*/
namespace CabinetSnap
{
	enum class Anchor { Corner, Middle };

	struct Target {
		QPointF point;
		Anchor anchor;
	};

	struct Snap {
			/// The point, or the top left corner, where it goes
		QPointF value;
			/// A corner or a middle of another part caught it
		bool found = false;
			/// That corner or middle, for the marker
		QPointF at;
		Anchor anchor = Anchor::Corner;
			/// Each direction caught by a part, a corner, a middle or an
			/// edge; otherwise rounded to the millimetre
		bool caught_x = false;
		bool caught_y = false;
	};

	bool active();
	QVector<Target> anchorsOf(const QRectF &rect);
	QVector<Target> targetsOf(const Diagram *diagram,
							  const QSet<const QGraphicsItem *> &ignored = {});
	qreal tolerance(const Diagram *diagram);
	qreal roundMm(qreal value, const Diagram *diagram);

	Snap point(const QPointF &point, const Diagram *diagram,
			   const QSet<const QGraphicsItem *> &ignored = {});
	Snap topLeft(const QRectF &rect, const Diagram *diagram,
				 const QSet<const QGraphicsItem *> &ignored = {});

	void showMarker(Diagram *diagram, const Snap &snap);
	void hideMarker(Diagram *diagram);
	bool markerShown(const Diagram *diagram);
}

#endif // CABINETSNAP_H
