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
#ifndef DIAGRAMEVENTADDELEMENT_H
#define DIAGRAMEVENTADDELEMENT_H

#include "../ElementsCollection/elementslocation.h"
#include "diagrameventinterface.h"

#include <QHash>
#include <QPointF>

#include <functional>

class Element;
class QGraphicsLineItem;
class QGraphicsPathItem;
class QStatusBar;
class QUndoCommand;

/**
	@brief The DiagramEventAddElement class
	This diagram event add a new element, for each left click button at the position of click.
	Space key rotate current element by 90°, right click button finish this event.
	With the project's automatic connections on, pressing and dragging
	places one copy on every line that crosses the drag (corners,
	T-junctions, folio reports), as in other CAE software; see AutoConnect::dragPlacements().
*/
class DiagramEventAddElement : public DiagramEventInterface
{
		Q_OBJECT

	public:
		DiagramEventAddElement(ElementsLocation &location, Diagram *diagram, QPointF pos = QPointF(0,0));
		~DiagramEventAddElement() override;

		void mouseMoveEvent        (QGraphicsSceneMouseEvent *event) override;
		void mousePressEvent       (QGraphicsSceneMouseEvent *event) override;
		void mouseReleaseEvent     (QGraphicsSceneMouseEvent *event) override;
		void mouseDoubleClickEvent (QGraphicsSceneMouseEvent *event) override;
		void keyPressEvent (QKeyEvent *event) override;
		void init() override;
			/// The symbol that follows the mouse, until it is dropped
		Element *pendingElement() const { return m_element; }
			/// Called for each element placed, before its step of undo is
			/// pushed: what it adds to that step is undone with it
		void setPrepare(std::function<void(Element *, QUndoCommand *)> prepare)
		{ m_prepare = std::move(prepare); }
			/// The symbols Tab and Shift+Tab go through while placing, the
			/// one placed now among them (a part of the parts collection)
		void setAlternatives(const QList<ElementsLocation> &locations);
		bool switchSymbol(int step);

	private:
		bool buildElement();
		Element *createElement() const;
		void addElement();
		void updateDrag(const QPointF &to);
		void endDrag();
		void addElements(const QList<QPointF> &positions);

	private:
		void showAlternative();

		ElementsLocation m_location;
		QList<ElementsLocation> m_alternatives;
		int m_alternative = 0;
			/// Where each alternative went in the project, once brought in
		QHash<int, QString> m_alternative_paths;
		Element *m_element;
		QString m_integrate_path;
		QPointer<QStatusBar> m_status_bar;

			/// Placing along a drag
		bool m_pressed = false,
			 m_dragging = false,
			 m_drag_cancelled = false;
		QPointF m_press_pos;
		QGraphicsLineItem *m_drag_line = nullptr;
		QGraphicsPathItem *m_preview_wires = nullptr;	///< grey, to each copy's partners
		QList<Element *> m_ghosts;		///< previews of the copies
		QList<QPointF> m_drag_positions;
		std::function<void(Element *, QUndoCommand *)> m_prepare;
};

#endif // DIAGRAMEVENTADDELEMENT_H
