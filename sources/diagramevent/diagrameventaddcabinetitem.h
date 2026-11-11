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
#ifndef DIAGRAMEVENTADDCABINETITEM_H
#define DIAGRAMEVENTADDCABINETITEM_H

#include "../cabinet/cabinetitem.h"
#include "diagrameventinterface.h"

#include <QPoint>
#include <QPointF>

class CabinetReadoutItem;

/**
	@brief The DiagramEventAddCabinetItem class
	The tool placing the parts of a cabinet on a cabinet folio.

	A wire duct and a DIN rail are drawn: a click where the part starts,
	another where it ends (or press, drag, release). The part runs
	horizontally or vertically, whichever the mouse went furthest, and a
	box under the mouse says how long it is, in millimetres. Typing a
	number and Enter gives it that length exactly. Its two ends catch the
	corners and the middles of the other parts (CabinetSnap). The tool then waits
	for the next part; a right click or Escape ends it.

	A mounting plate and a DXF drawing have their measures already: they
	follow the mouse and a click puts them down.
*/
class DiagramEventAddCabinetItem : public DiagramEventInterface
{
	Q_OBJECT

	public:
		DiagramEventAddCabinetItem(Diagram *diagram,
								   const CabinetItem::Properties &properties);
		~DiagramEventAddCabinetItem() override;

		void init() override;
		void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
		void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
		void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
		void keyPressEvent(QKeyEvent *event) override;
		bool wantsKey(const QKeyEvent *event) const override;

			/// The part being drawn, nullptr between two parts
		CabinetItem *preview() const {return m_item;}
			/// What the box under the mouse says
		QString readout() const;
		bool isDrawing() const {return m_drawing;}

	private:
		bool isDrawn() const;
		void begin(const QPointF &scene_pos);
		void track(const QPointF &scene_pos);
		bool commit();
		void dropPreview();
		void endTool(bool aborted);
		void showReadout(const QPointF &scene_pos);
		void updateStatusHint();
		qreal typedLength(bool *ok) const;

		CabinetItem::Properties m_properties;
		CabinetItem *m_item = nullptr;
		CabinetReadoutItem *m_readout = nullptr;
		bool m_drawing = false;
		QPointF m_start;
		QPointF m_last_pos;
		QPoint m_press_screen_pos;
		QString m_typed;
};

#endif // DIAGRAMEVENTADDCABINETITEM_H
