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
#ifndef CABINETROUTING_H
#define CABINETROUTING_H

#include "cabinetrouter.h"

#include <QCoreApplication>
#include <QGraphicsObject>
#include <QList>
#include <QPointer>
#include <QUndoCommand>
#include <QVector>

class CabinetItem;
class Diagram;
class QDomDocument;
class QDomElement;
class QWidget;

/**
	@brief The routing sheet of a mounting plate (Brandon, 2026-11-19).

	A routing sheet (Diagram::FolioKind::Routing) stands for one mounting
	plate of a cabinet sheet: it shows that plate with its ducts, rails and
	device drawings as they are on the cabinet sheet (read from there, never
	copied, so what moves there moves here), and the way each wire of the
	schematic takes through the ducts between the centres of the drawings
	of its two devices (CabinetRouter). A terminal goes to the drawing of
	its strip.

	"Calculate the routing" gives each wire its way; a way changed by hand
	(its corners dragged, added, taken away) is kept when the routing is
	calculated again, unless the user says otherwise. Clicking a way shows
	its wire and its length. The ways are exported to CSV.

	The ways are kept in millimetres from the top left corner of the plate:
	moving the plate on its cabinet sheet, or drawing the routing sheet at
	another scale, does not break them.
*/
namespace CabinetRouting
{
		/// The way one wire takes on a routing sheet
	struct Route
	{
			/// The two ends of the wire (symbol and terminal), the same
			/// from one calculation to the next
		QString key;
		QString number;
		QString from_label;
		QString from_terminal;
		QString to_label;
		QString to_terminal;
		QString section;
		QString color;
		QString cable;
			/// In millimetres from the top left corner of the plate
		QVector<QPointF> points;
		bool manual = false;
		bool through_ducts = true;

		qreal length() const;
		QString state() const;
		bool operator==(const Route &other) const;
		bool operator!=(const Route &other) const {return !(*this == other);}
	};

		/// What a calculation did
	struct Summary
	{
		int routed = 0;
		int straight = 0;
		int kept = 0;
			/// Wires with an end with no drawing on the plate
		int off_plate = 0;
	};

	Diagram *sourceOf(const Diagram *routing);
	CabinetItem *plateOf(const Diagram *routing);
	QList<CabinetItem *> platesOf(Diagram *cabinet);
	QPointF toScene(const Diagram *routing, const QPointF &mm);
	QPointF toMm(const Diagram *routing, const QPointF &scene);
		/// The ducts on the plate of @p routing, in millimetres from its
		/// top left corner
	QList<CabinetRouter::Duct> ductsOf(const Diagram *routing);

	QList<Route> routesOf(const Diagram *routing);
	void setRoutes(Diagram *routing, const QList<Route> &routes);
	QList<Route> calculate(Diagram *routing, const QList<Route> &current,
						   bool keep_manual, Summary *summary = nullptr);
		/// @p route as calculated again between its two devices, no longer
		/// changed by hand; as it is when one of them is gone
	Route reset(Diagram *routing, const Route &route);
	QString csv(const QList<Route> &routes);

	QDomElement toXml(const Diagram *routing, QDomDocument &document);
	void fromXml(Diagram *routing, const QDomElement &diagram_root);
		/// Show the plate of @p routing on it, once
	void attach(Diagram *routing);
		/// Draw the ways again: the scale of the sheet changed
	void refresh(Diagram *routing);

	Diagram *addRoutingSheet(QWidget *parent, Diagram *cabinet);
	void calculateWithDialog(QWidget *parent, Diagram *routing);
	void exportCsvWithDialog(QWidget *parent, Diagram *routing);
}

/**
	@brief The way of one wire drawn on a routing sheet. Selected, it says
	its wire and its length; its corners are dragged, a double click on it
	adds a corner there, a double click on a corner takes it away. A
	stretch of it is dragged across -- a horizontal one up or down, a
	vertical one sideways -- and catches on the middle of a duct it comes
	near: that is how a wire is moved into another duct (Brandon,
	2026-11-19); its two ends stay on their devices. Any change makes it a
	way changed by hand, one step of undo -- drawn in the same colour.
	A right click resets its position, or the position of every way
	selected with it: calculated again, no longer changed by hand.
*/
class CableRouteItem : public QGraphicsObject
{
	Q_OBJECT

	public:
		enum { Type = UserType + 1022 };
		int type() const override {return Type;}

		explicit CableRouteItem(const CabinetRouting::Route &route);

		CabinetRouting::Route route() const {return m_route;}
		void setRoute(const CabinetRouting::Route &route);
			/// The sheet moved or scaled: its points stand elsewhere
		void geometryChanged();
		QString infoText() const;
			/// The scene points of the way
		QVector<QPointF> scenePoints() const;

		QRectF boundingRect() const override;
		QPainterPath shape() const override;
		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
				   QWidget *widget) override;

	protected:
		QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
		void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
		void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
		void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
		void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
		void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
		void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
		void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

	private:
		qreal tolerance(const QWidget *widget) const;
		int cornerAt(const QPointF &scene_pos, qreal tolerance) const;
		int segmentAt(const QPointF &scene_pos, qreal tolerance) const;
		void change(const CabinetRouting::Route &after);
		void placeInfo();

		CabinetRouting::Route m_route;
		QGraphicsItem *m_info = nullptr;
		void dragStretch(const QPointF &scene_pos, qreal reach);

		int m_dragged = -1;
			/// The stretch dragged across, from its point m_stretch
		int m_stretch = -1;
		QPointF m_press_mm;
		QList<CabinetRouter::Duct> m_ducts;
		CabinetRouting::Route m_drag_start;
};

/**
	@brief Every way of a routing sheet at once: what a calculation gives
*/
class SetCableRoutesCommand : public QUndoCommand
{
	public:
		SetCableRoutesCommand(Diagram *routing,
							  const QList<CabinetRouting::Route> &before,
							  const QList<CabinetRouting::Route> &after,
							  QUndoCommand *parent = nullptr);
		void undo() override;
		void redo() override;

	private:
		QPointer<Diagram> m_routing;
		QList<CabinetRouting::Route> m_before;
		QList<CabinetRouting::Route> m_after;
};

/**
	@brief One way changed by hand
*/
class ChangeCableRouteCommand : public QUndoCommand
{
	public:
		ChangeCableRouteCommand(Diagram *routing,
								const CabinetRouting::Route &before,
								const CabinetRouting::Route &after,
								QUndoCommand *parent = nullptr);
		void undo() override;
		void redo() override;

	private:
		void apply(const CabinetRouting::Route &route);

		QPointer<Diagram> m_routing;
		CabinetRouting::Route m_before;
		CabinetRouting::Route m_after;
};

#endif // CABINETROUTING_H
