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

	Tab changes the point of the part which goes where the mouse is (its
	anchor), Shift+Tab goes back (Brandon, 2026-11-19): the middle of the
	left side first, then the top left corner, the middle of the top, the
	top right corner, and so round the part to its centre, and again. For
	a duct or a rail, whichever way it runs, left and right are its two
	ends and top and bottom its two sides: it runs from the click along
	the side, or the middle, the anchor names; with the anchor in the
	middle of its length it grows from the click both ways.
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

			/// The point of the part which goes where the mouse is
		enum class Anchor {
			CenterLeft, TopLeft, TopCenter, TopRight, CenterRight,
			BottomRight, BottomCenter, BottomLeft, Center
		};
		Anchor anchor() const {return m_anchor;}
		void setAnchor(Anchor anchor);
			/// The next anchor Tab goes to, the one before for Shift+Tab
		static Anchor nextAnchor(Anchor anchor, bool back = false);
			/// Where @p anchor is in a part, from 0 (left, top) to 1
		static QPointF anchorFraction(Anchor anchor);
		static QString anchorName(Anchor anchor);

			/// How the tool ended: a part put down, given up with a right
			/// click, or left with Escape
		enum class Ending { Placed, Skipped, Cancelled };
		Ending ending() const {return m_ending;}
			/// What the status bar says instead of the usual hint (a
			/// queue of drawings says which one this is), empty for that
		void setHint(const QString &hint);

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
			/// The free space of the parts drawn in red (a DXF)
		bool m_red_zones = false;
		CabinetReadoutItem *m_readout = nullptr;
		bool m_drawing = false;
		QPointF m_start;
		QPointF m_last_pos;
		QPoint m_press_screen_pos;
		QString m_typed;
		Ending m_ending = Ending::Cancelled;
		QString m_hint;
		Anchor m_anchor = Anchor::CenterLeft;
};

#endif // DIAGRAMEVENTADDCABINETITEM_H
