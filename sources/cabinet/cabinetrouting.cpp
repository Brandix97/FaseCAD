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
#include "cabinetrouting.h"

#include "cabinetitem.h"
#include "cabinetrouter.h"
#include "../diagram.h"
#include "../qetproject.h"
#include "../wiringlistexport.h"

#include <QDomDocument>
#include <QFile>
#include <QFileDialog>
#include <QFontMetricsF>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QHash>
#include <QInputDialog>
#include <QLineF>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPathStroker>
#include <QStyleOptionGraphicsItem>
#include <QTextStream>
#include <QTimer>

#include <cmath>

namespace
{
	QString tr(const char *text, const char *comment = nullptr, int n = -1)
	{
		return QCoreApplication::translate("CabinetRouting", text, comment, n);
	}

		/// Where the top left corner of the plate stands on a routing sheet
	QPointF originOf(const Diagram *routing)
	{
		const QRectF inside = routing->border_and_titleblock.insideBorderRect();
		return inside.isValid() ? inside.topLeft() + QPointF(20, 20) : QPointF(50, 50);
	}

	QString mmText(qreal value)
	{
		return QString::number(qRound(value));
	}

	/**
		@brief The plate of a routing sheet and what stands on it, drawn as
		it is on its cabinet sheet: read from there each time it is painted,
		never kept here, never saved with the routing sheet.
	*/
	class RoutingBackdrop : public QGraphicsObject
	{
		public:
			enum { Type = QGraphicsItem::UserType + 1023 };
			int type() const override {return Type;}

			explicit RoutingBackdrop(Diagram *routing) :
				m_routing(routing)
			{
				setZValue(-1000);
				setAcceptedMouseButtons(Qt::NoButton);
				setFlag(ItemIsSelectable, false);
				watchSource();
			}

				/// Drawn again: the sheet's scale changed
			void refresh()
			{
				prepareGeometryChange();
				update();
			}

			void watchSource()
			{
				Diagram *source = CabinetRouting::sourceOf(m_routing);
				if (source == m_source) {
					return;
				}
				if (m_source) {
					disconnect(m_source, nullptr, this, nullptr);
				}
				m_source = source;
				if (m_source) {
						//What moves on the cabinet sheet moves here
					connect(m_source, &QGraphicsScene::changed, this, [this]() {
						prepareGeometryChange();
						update();
					});
				}
			}

			QRectF boundingRect() const override
			{
				CabinetItem *plate = CabinetRouting::plateOf(m_routing);
				if (!plate) {
					return QRectF(CabinetRouting::toScene(m_routing, QPointF()), QSizeF(400, 60));
				}
				const QSizeF size = plate->sizeMm() * m_routing->pixelsPerMm();
				return QRectF(CabinetRouting::toScene(m_routing, QPointF()), size)
						.adjusted(-10, -10, 10, 10);
			}

			void paint(QPainter *painter, const QStyleOptionGraphicsItem *,
					   QWidget *) override
			{
				Diagram *source = CabinetRouting::sourceOf(m_routing);
				if (source && !source->isLoaded()) {
						//Its cabinet sheet built once, the first time the
						//plate is drawn (an export to a picture too)
					static bool building = false;
					if (building) {
						return;
					}
					building = true;
					source->ensureLoaded();
					building = false;
					watchSource();
				}
				CabinetItem *plate = CabinetRouting::plateOf(m_routing);
				if (!plate) {
					painter->setPen(QColor(200, 30, 30));
					painter->drawText(boundingRect(), Qt::AlignLeft | Qt::AlignTop,
									  QCoreApplication::translate("CabinetRouting",
										  "The mounting plate of this routing sheet is no longer on its cabinet sheet."));
					return;
				}
				const qreal source_pixels = plate->pixelsPerMm();
				const qreal ratio = m_routing->pixelsPerMm() / source_pixels;
				const QRectF plate_rect = plate->sceneRect();

				QList<CabinetItem *> parts{plate};
				for (QGraphicsItem *item : source->items(Qt::AscendingOrder)) {
					auto *part = qgraphicsitem_cast<CabinetItem *>(item);
					if (part && part != plate
						&& part->kind() != CabinetItem::Kind::MountingPlate
						&& part->sceneRect().intersects(plate_rect)) {
						parts << part;
					}
				}
				for (CabinetItem *part : std::as_const(parts))
				{
					const QPointF mm = (part->sceneRect().topLeft() - plate_rect.topLeft())
							/ source_pixels;
					QStyleOptionGraphicsItem option;
					option.exposedRect = part->boundingRect();
					option.state = QStyle::State_None;
					painter->save();
					painter->translate(CabinetRouting::toScene(m_routing, mm));
					painter->scale(ratio, ratio);
						//The parts paint from their own top left corner
					painter->translate(-part->rect().topLeft());
					part->paint(painter, &option, nullptr);
					painter->restore();
				}
			}

		private:
			Diagram *m_routing;
			QPointer<Diagram> m_source;
	};

		/// The info box of a selected way: its size on the screen whatever
		/// the zoom
	class RouteInfoItem : public QGraphicsItem
	{
		public:
			RouteInfoItem(QGraphicsItem *parent) :
				QGraphicsItem(parent)
			{
				setFlag(ItemIgnoresTransformations, true);
				setAcceptedMouseButtons(Qt::NoButton);
				m_font.setPixelSize(12);
			}
			void setText(const QString &text)
			{
				prepareGeometryChange();
				m_text = text;
				update();
			}
			QString text() const {return m_text;}
			QRectF boundingRect() const override
			{
				return box().adjusted(-2, -2, 2, 2);
			}
			void paint(QPainter *painter, const QStyleOptionGraphicsItem *,
					   QWidget *) override
			{
				painter->setRenderHint(QPainter::Antialiasing, true);
				painter->setPen(QPen(QColor(40, 48, 60), 1));
				painter->setBrush(QColor(255, 250, 205));
				painter->drawRoundedRect(box(), 3, 3);
				painter->setFont(m_font);
				painter->setPen(QColor(20, 24, 30));
				painter->drawText(box().adjusted(8, 4, -8, -4), Qt::AlignLeft | Qt::AlignVCenter, m_text);
			}
		private:
			QRectF box() const
			{
				const QFontMetricsF metrics(m_font);
				qreal width = 0;
				const QStringList lines = m_text.split(QLatin1Char('\n'));
				for (const QString &line : lines) {
					width = qMax(width, metrics.horizontalAdvance(line));
				}
				return QRectF(12, 12, width + 16, metrics.lineSpacing() * lines.size() + 8);
			}
			QString m_text;
			QFont m_font;
	};

	QList<CableRouteItem *> routeItemsOf(const Diagram *routing)
	{
		QList<CableRouteItem *> list;
		for (QGraphicsItem *item : routing->items(Qt::AscendingOrder)) {
			if (auto *route = qgraphicsitem_cast<CableRouteItem *>(item)) {
				list << route;
			}
		}
		return list;
	}

	QString pointsText(const QVector<QPointF> &points)
	{
		QStringList list;
		for (const QPointF &point : points) {
			list << QStringLiteral("%1,%2").arg(point.x()).arg(point.y());
		}
		return list.join(QLatin1Char(' '));
	}

	QVector<QPointF> pointsFrom(const QString &text)
	{
		QVector<QPointF> points;
		for (const QString &pair : text.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
			const QStringList xy = pair.split(QLatin1Char(','));
			if (xy.size() == 2) {
				points << QPointF(xy.at(0).toDouble(), xy.at(1).toDouble());
			}
		}
		return points;
	}

		/// The device a wire end stands for: its label, or for a terminal
		/// ("-X1:3") its strip
	CabinetItem *drawingFor(const QHash<QString, CabinetItem *> &drawings, const QString &label)
	{
		const QString name = label.trimmed();
		if (name.isEmpty()) {
			return nullptr;
		}
		if (CabinetItem *drawing = drawings.value(name)) {
			return drawing;
		}
		const int colon = name.indexOf(QLatin1Char(':'));
		return colon > 0 ? drawings.value(name.left(colon)) : nullptr;
	}
}

qreal CabinetRouting::Route::length() const
{
	return CabinetRouter::lengthOf(points);
}

QString CabinetRouting::Route::state() const
{
	if (manual) {
		return tr("changed by hand");
	}
	return through_ducts ? tr("through the ducts") : tr("no duct: straight line");
}

bool CabinetRouting::Route::operator==(const Route &other) const
{
	return key == other.key && number == other.number
			&& from_label == other.from_label && from_terminal == other.from_terminal
			&& to_label == other.to_label && to_terminal == other.to_terminal
			&& section == other.section && color == other.color && cable == other.cable
			&& points == other.points && manual == other.manual
			&& through_ducts == other.through_ducts;
}

Diagram *CabinetRouting::sourceOf(const Diagram *routing)
{
	if (!routing || !routing->isRoutingFolio() || !routing->project()) {
		return nullptr;
	}
	Diagram *source = routing->project()->diagramByUuid(routing->routingSource());
	return source && source->isCabinetFolio() ? source : nullptr;
}

CabinetItem *CabinetRouting::plateOf(const Diagram *routing)
{
	Diagram *source = sourceOf(routing);
	if (!source || !source->isLoaded()) {
		return nullptr;
	}
	for (QGraphicsItem *item : source->items()) {
		auto *part = qgraphicsitem_cast<CabinetItem *>(item);
		if (part && part->kind() == CabinetItem::Kind::MountingPlate
			&& part->uuid() == routing->routingPlate()) {
			return part;
		}
	}
	return nullptr;
}

QList<CabinetItem *> CabinetRouting::platesOf(Diagram *cabinet)
{
	QList<CabinetItem *> plates;
	if (!cabinet) {
		return plates;
	}
	cabinet->ensureLoaded();
	for (QGraphicsItem *item : cabinet->items(Qt::AscendingOrder)) {
		auto *part = qgraphicsitem_cast<CabinetItem *>(item);
		if (part && part->kind() == CabinetItem::Kind::MountingPlate) {
			plates << part;
		}
	}
	std::sort(plates.begin(), plates.end(), [](CabinetItem *a, CabinetItem *b) {
		const QPointF pa = a->sceneRect().topLeft();
		const QPointF pb = b->sceneRect().topLeft();
		return pa.y() != pb.y() ? pa.y() < pb.y() : pa.x() < pb.x();
	});
	return plates;
}

QPointF CabinetRouting::toScene(const Diagram *routing, const QPointF &mm)
{
	return originOf(routing) + mm * routing->pixelsPerMm();
}

QPointF CabinetRouting::toMm(const Diagram *routing, const QPointF &scene)
{
	return (scene - originOf(routing)) / routing->pixelsPerMm();
}

QList<CabinetRouter::Duct> CabinetRouting::ductsOf(const Diagram *routing)
{
	QList<CabinetRouter::Duct> ducts;
	Diagram *source = sourceOf(routing);
	CabinetItem *plate = plateOf(routing);
	if (!source || !plate) {
		return ducts;
	}
	const qreal pixels = plate->pixelsPerMm();
	const QRectF plate_rect = plate->sceneRect();
	for (QGraphicsItem *item : source->items(Qt::AscendingOrder))
	{
		auto *part = qgraphicsitem_cast<CabinetItem *>(item);
		if (part && part->kind() == CabinetItem::Kind::WireDuct
			&& part->sceneRect().intersects(plate_rect)) {
			const QRectF rect = part->sceneRect();
			ducts << CabinetRouter::Duct{
					 QRectF((rect.topLeft() - plate_rect.topLeft()) / pixels, rect.size() / pixels),
					 part->properties().orientation};
		}
	}
	return ducts;
}

QList<CabinetRouting::Route> CabinetRouting::routesOf(const Diagram *routing)
{
	QList<Route> routes;
	if (!routing) {
		return routes;
	}
	for (CableRouteItem *item : routeItemsOf(routing)) {
		routes << item->route();
	}
	return routes;
}

/**
	@brief CabinetRouting::setRoutes
	Draw @p routes on @p routing in place of the ways it shows
*/
void CabinetRouting::setRoutes(Diagram *routing, const QList<Route> &routes)
{
	if (!routing) {
		return;
	}
	for (CableRouteItem *item : routeItemsOf(routing)) {
		routing->removeItem(item);
		delete item;
	}
	for (const Route &route : routes) {
		routing->addItem(new CableRouteItem(route));
	}
}

/**
	@brief CabinetRouting::calculate
	@return the way of every wire of the project whose two ends have a
	drawing on the plate of @p routing. With @p keep_manual, a way of
	@p current changed by hand is kept as it is.
*/
QList<CabinetRouting::Route> CabinetRouting::calculate(Diagram *routing,
													   const QList<Route> &current,
													   bool keep_manual,
													   Summary *summary)
{
	QList<Route> routes;
	Summary count;
	Diagram *source = sourceOf(routing);
	if (source) {
		source->ensureLoaded();
	}
	CabinetItem *plate = plateOf(routing);
	if (!plate || !routing->project()) {
		if (summary) *summary = count;
		return routes;
	}
	const qreal pixels = plate->pixelsPerMm();
	const QRectF plate_rect = plate->sceneRect();
	auto mmRect = [&](const QRectF &scene_rect) {
		return QRectF((scene_rect.topLeft() - plate_rect.topLeft()) / pixels,
					  scene_rect.size() / pixels);
	};

		//The ducts and the device drawings standing on the plate
	const QList<CabinetRouter::Duct> ducts = ductsOf(routing);
	QHash<QString, CabinetItem *> drawings;
	QList<QRectF> devices;
	for (QGraphicsItem *item : source->items(Qt::AscendingOrder))
	{
		auto *part = qgraphicsitem_cast<CabinetItem *>(item);
		if (!part || part == plate || !part->sceneRect().intersects(plate_rect)) {
			continue;
		}
		if (part->kind() == CabinetItem::Kind::Drawing) {
			devices << mmRect(part->sceneRect());
			const QString label = part->properties().device_label.trimmed();
			if (!label.isEmpty() && !drawings.contains(label)) {
				drawings.insert(label, part);
			}
		}
	}
	const CabinetRouter router(ducts, devices);

	QHash<QString, Route> by_hand;
	if (keep_manual) {
		for (const Route &route : current) {
			if (route.manual) {
				by_hand.insert(route.key, route);
			}
		}
	}

	WiringListExport list(routing->project(), nullptr);
	const QList<ConductorData> wires = list.wires();
	for (const ConductorData &wire : wires)
	{
		CabinetItem *from = drawingFor(drawings, wire.element1_label);
		CabinetItem *to = drawingFor(drawings, wire.element2_label);
		if (!from || !to) {
			++count.off_plate;
			continue;
		}
			//A wire inside one device has no way to take
		if (from == to) {
			continue;
		}

		QString end1 = wire.el1_uuid + QLatin1Char('/') + wire.terminalname1;
		QString end2 = wire.el2_uuid + QLatin1Char('/') + wire.terminalname2;
		if (end2 < end1) {
			std::swap(end1, end2);
		}
		Route route;
		route.key = end1 + QLatin1Char('|') + end2;
		route.number = wire.number;
		route.from_label = wire.element1_label;
		route.from_terminal = wire.terminalname1;
		route.to_label = wire.element2_label;
		route.to_terminal = wire.terminalname2;
		route.section = wire.conductor_section;
		route.color = wire.conductor_color;
		route.cable = wire.cable;

		if (by_hand.contains(route.key)) {
			const Route kept = by_hand.value(route.key);
			route.points = kept.points;
			route.manual = true;
			route.through_ducts = kept.through_ducts;
			++count.kept;
		} else {
			const QRectF from_rect = mmRect(from->sceneRect());
			const QRectF to_rect = mmRect(to->sceneRect());
			const CabinetRouter::Route way = router.route(
						from_rect.center(), to_rect.center(), from_rect, to_rect);
			route.points = way.points;
			route.through_ducts = way.through_ducts;
			if (!way.through_ducts) {
				++count.straight;
			}
		}
		++count.routed;
		routes << route;
	}
	if (summary) *summary = count;
	return routes;
}

/**
	@brief CabinetRouting::reset
	@return @p route calculated again between the drawings of its two
	devices on the plate, no longer a way changed by hand
*/
CabinetRouting::Route CabinetRouting::reset(Diagram *routing, const Route &route)
{
	CabinetItem *plate = plateOf(routing);
	Diagram *source = sourceOf(routing);
	if (!plate || !source) {
		return route;
	}
	const qreal pixels = plate->pixelsPerMm();
	const QRectF plate_rect = plate->sceneRect();
	auto mmRect = [&](const QRectF &scene_rect) {
		return QRectF((scene_rect.topLeft() - plate_rect.topLeft()) / pixels,
					  scene_rect.size() / pixels);
	};
	QHash<QString, CabinetItem *> drawings;
	QList<QRectF> devices;
	for (QGraphicsItem *item : source->items(Qt::AscendingOrder))
	{
		auto *part = qgraphicsitem_cast<CabinetItem *>(item);
		if (!part || part->kind() != CabinetItem::Kind::Drawing
			|| !part->sceneRect().intersects(plate_rect)) {
			continue;
		}
		devices << mmRect(part->sceneRect());
		const QString label = part->properties().device_label.trimmed();
		if (!label.isEmpty() && !drawings.contains(label)) {
			drawings.insert(label, part);
		}
	}
	CabinetItem *from = drawingFor(drawings, route.from_label);
	CabinetItem *to = drawingFor(drawings, route.to_label);
	if (!from || !to || from == to) {
		return route;
	}
	const QRectF from_rect = mmRect(from->sceneRect());
	const QRectF to_rect = mmRect(to->sceneRect());
	const CabinetRouter router(ductsOf(routing), devices);
	const CabinetRouter::Route way = router.route(from_rect.center(), to_rect.center(),
												  from_rect, to_rect);
	Route result = route;
	result.points = way.points;
	result.through_ducts = way.through_ducts;
	result.manual = false;
	return result;
}

/**
	@brief CabinetRouting::csv
	@return @p routes as a CSV file for a spreadsheet: one line per wire,
	with its length in millimetres
*/
QString CabinetRouting::csv(const QList<Route> &routes)
{
	auto cell = [](QString text) {
		text.replace(QLatin1Char('"'), QStringLiteral("\"\""));
		if (text.contains(QLatin1Char(';')) || text.contains(QLatin1Char('"'))
			|| text.contains(QLatin1Char('\n'))) {
			text = QLatin1Char('"') + text + QLatin1Char('"');
		}
		return text;
	};
	QString csv;
	QTextStream out(&csv);
	out << tr("Wire", "routing CSV header") << ';'
		<< tr("From", "routing CSV header") << ';'
		<< tr("Terminal", "routing CSV header") << ';'
		<< tr("To", "routing CSV header") << ';'
		<< tr("Terminal", "routing CSV header") << ';'
		<< tr("Section", "routing CSV header") << ';'
		<< tr("Colour", "routing CSV header") << ';'
		<< tr("Cable", "routing CSV header") << ';'
		<< tr("Length (mm)", "routing CSV header") << ';'
		<< tr("Way", "routing CSV header") << '\n';
	for (const Route &route : routes) {
		out << cell(route.number) << ';'
			<< cell(route.from_label) << ';'
			<< cell(route.from_terminal) << ';'
			<< cell(route.to_label) << ';'
			<< cell(route.to_terminal) << ';'
			<< cell(route.section) << ';'
			<< cell(route.color) << ';'
			<< cell(route.cable) << ';'
			<< mmText(route.length()) << ';'
			<< cell(route.state()) << '\n';
	}
	return csv;
}

QDomElement CabinetRouting::toXml(const Diagram *routing, QDomDocument &document)
{
	QDomElement list = document.createElement(QStringLiteral("cable_routes"));
	for (const Route &route : routesOf(routing))
	{
		QDomElement xml = document.createElement(QStringLiteral("cable_route"));
		xml.setAttribute(QStringLiteral("key"), route.key);
		xml.setAttribute(QStringLiteral("number"), route.number);
		xml.setAttribute(QStringLiteral("from"), route.from_label);
		xml.setAttribute(QStringLiteral("from_terminal"), route.from_terminal);
		xml.setAttribute(QStringLiteral("to"), route.to_label);
		xml.setAttribute(QStringLiteral("to_terminal"), route.to_terminal);
		xml.setAttribute(QStringLiteral("section"), route.section);
		xml.setAttribute(QStringLiteral("color"), route.color);
		xml.setAttribute(QStringLiteral("cable"), route.cable);
		xml.setAttribute(QStringLiteral("points"), pointsText(route.points));
		if (route.manual) {
			xml.setAttribute(QStringLiteral("manual"), QStringLiteral("true"));
		}
		if (!route.through_ducts) {
			xml.setAttribute(QStringLiteral("straight"), QStringLiteral("true"));
		}
		list.appendChild(xml);
	}
	return list;
}

void CabinetRouting::fromXml(Diagram *routing, const QDomElement &diagram_root)
{
	QList<Route> routes;
	const QDomElement list = diagram_root.firstChildElement(QStringLiteral("cable_routes"));
	for (QDomElement xml = list.firstChildElement(QStringLiteral("cable_route"));
		 !xml.isNull(); xml = xml.nextSiblingElement(QStringLiteral("cable_route")))
	{
		Route route;
		route.key = xml.attribute(QStringLiteral("key"));
		route.number = xml.attribute(QStringLiteral("number"));
		route.from_label = xml.attribute(QStringLiteral("from"));
		route.from_terminal = xml.attribute(QStringLiteral("from_terminal"));
		route.to_label = xml.attribute(QStringLiteral("to"));
		route.to_terminal = xml.attribute(QStringLiteral("to_terminal"));
		route.section = xml.attribute(QStringLiteral("section"));
		route.color = xml.attribute(QStringLiteral("color"));
		route.cable = xml.attribute(QStringLiteral("cable"));
		route.points = pointsFrom(xml.attribute(QStringLiteral("points")));
		route.manual = xml.attribute(QStringLiteral("manual")) == QLatin1String("true");
		route.through_ducts = xml.attribute(QStringLiteral("straight")) != QLatin1String("true");
		if (route.points.size() >= 2) {
			routes << route;
		}
	}
	setRoutes(routing, routes);
}

void CabinetRouting::attach(Diagram *routing)
{
	if (!routing || !routing->isRoutingFolio()) {
		return;
	}
	for (QGraphicsItem *item : routing->items()) {
		if (item->type() == RoutingBackdrop::Type) {
			return;
		}
	}
		//Not one of the items the folio saves (Diagram::toXml())
	routing->QGraphicsScene::addItem(new RoutingBackdrop(routing));
}

void CabinetRouting::refresh(Diagram *routing)
{
	if (!routing) {
		return;
	}
	for (QGraphicsItem *item : routing->items()) {
		if (item->type() == RoutingBackdrop::Type) {
			static_cast<RoutingBackdrop *>(item)->refresh();
		} else if (auto *route = qgraphicsitem_cast<CableRouteItem *>(item)) {
			route->geometryChanged();
		}
	}
}

/**
	@brief CabinetRouting::addRoutingSheet
	Add, right after @p cabinet, a routing sheet for one of its mounting
	plates: the only one, or the one the user chooses.
	@return the new sheet, nullptr if none was added
*/
Diagram *CabinetRouting::addRoutingSheet(QWidget *parent, Diagram *cabinet)
{
	if (!cabinet || !cabinet->isCabinetFolio() || !cabinet->project()) {
		return nullptr;
	}
	const QList<CabinetItem *> plates = platesOf(cabinet);
	if (plates.isEmpty()) {
		QMessageBox::information(parent, tr("Routing sheet"),
								 tr("Put a mounting plate on this cabinet sheet first: a routing sheet shows the ways of the wires on one plate."));
		return nullptr;
	}
	CabinetItem *plate = plates.first();
	if (plates.size() > 1)
	{
		QStringList names;
		for (int i = 0 ; i < plates.size() ; ++i) {
			const QSizeF size = plates.at(i)->sizeMm();
			names << tr("Plate %1: %2 × %3 mm", "a mounting plate to route")
					 .arg(i + 1).arg(mmText(size.width()), mmText(size.height()));
		}
		bool ok = false;
		const QString chosen = QInputDialog::getItem(parent, tr("Routing sheet"),
													 tr("Mounting plate to route:"),
													 names, 0, false, &ok);
		if (!ok) {
			return nullptr;
		}
		plate = plates.at(names.indexOf(chosen));
	}
	QETProject *project = cabinet->project();
	return project->addNewRoutingDiagram(project->folioIndex(cabinet) + 1, cabinet, plate->uuid());
}

void CabinetRouting::calculateWithDialog(QWidget *parent, Diagram *routing)
{
	if (!routing || !routing->isRoutingFolio()) {
		return;
	}
	if (Diagram *source = sourceOf(routing)) {
		source->ensureLoaded();
	}
	if (!plateOf(routing)) {
		QMessageBox::warning(parent, tr("Calculate the routing"),
							 tr("The mounting plate of this routing sheet is no longer on its cabinet sheet."));
		return;
	}
	const QList<Route> current = routesOf(routing);
	int by_hand = 0;
	for (const Route &route : current) {
		if (route.manual) ++by_hand;
	}
	bool keep = true;
	if (by_hand)
	{
		const auto answer = QMessageBox::question(
					parent, tr("Calculate the routing"),
					tr("Keep the %n way(s) changed by hand?", "", by_hand),
					QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
		if (answer == QMessageBox::Cancel) {
			return;
		}
		keep = answer == QMessageBox::Yes;
	}

	Summary summary;
	const QList<Route> routes = calculate(routing, current, keep, &summary);
	if (routes != current) {
		auto *command = new SetCableRoutesCommand(routing, current, routes);
		command->setText(tr("Calculate the routing"));
		routing->undoStack().push(command);
	}

	QStringList lines;
	lines << tr("%n wire(s) routed on this plate.", "", summary.routed);
	if (summary.kept) {
		lines << tr("%n way(s) changed by hand kept.", "", summary.kept);
	}
	if (summary.straight) {
		lines << tr("%n wire(s) with no duct between their devices: a straight line, in red.", "", summary.straight);
	}
	if (summary.off_plate) {
		lines << tr("%n wire(s) left out: one of their ends has no drawing on this plate.", "", summary.off_plate);
	}
	QMessageBox::information(parent, tr("Calculate the routing"), lines.join(QLatin1Char('\n')));
}

void CabinetRouting::exportCsvWithDialog(QWidget *parent, Diagram *routing)
{
	if (!routing || !routing->isRoutingFolio()) {
		return;
	}
	const QList<Route> routes = routesOf(routing);
	if (routes.isEmpty()) {
		QMessageBox::information(parent, tr("Export the routing"),
								 tr("This routing sheet has no way yet: calculate the routing first."));
		return;
	}
	const QString path = QFileDialog::getSaveFileName(
				parent, tr("Export the routing"), QString(), tr("CSV files (*.csv)"));
	if (path.isEmpty()) {
		return;
	}
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		QMessageBox::warning(parent, tr("Export the routing"),
							 tr("Unable to open the file for writing."));
		return;
	}
		//With its byte order mark, a spreadsheet reads the accents and mm²
	file.write("\xEF\xBB\xBF");
	file.write(csv(routes).toUtf8());
	file.close();
}

/* ---------------------------------------------------------------------- */

CableRouteItem::CableRouteItem(const CabinetRouting::Route &route) :
	m_route(route)
{
	setFlag(ItemIsSelectable, true);
	setAcceptHoverEvents(true);
	setZValue(10);
	m_info = new RouteInfoItem(this);
	m_info->hide();
	setToolTip(infoText());
}

void CableRouteItem::setRoute(const CabinetRouting::Route &route)
{
	prepareGeometryChange();
	m_route = route;
	setToolTip(infoText());
	placeInfo();
	update();
}

void CableRouteItem::geometryChanged()
{
	prepareGeometryChange();
	placeInfo();
	update();
}

QString CableRouteItem::infoText() const
{
	auto end = [](const QString &label, const QString &terminal) {
		return terminal.isEmpty() ? label : label + QLatin1Char(':') + terminal;
	};
	QStringList head;
	if (!m_route.number.isEmpty()) {
		head << QCoreApplication::translate("CabinetRouting", "Wire %1").arg(m_route.number);
	}
	head << QStringLiteral("%1 → %2").arg(end(m_route.from_label, m_route.from_terminal),
										  end(m_route.to_label, m_route.to_terminal));
	QStringList tail;
	tail << QCoreApplication::translate("CabinetRouting", "Length: %1 mm")
			.arg(mmText(m_route.length()));
	if (!m_route.section.isEmpty()) {
		tail << m_route.section;
	}
	tail << m_route.state();
	return head.join(QStringLiteral(" · ")) + QLatin1Char('\n') + tail.join(QStringLiteral(" · "));
}

QVector<QPointF> CableRouteItem::scenePoints() const
{
	QVector<QPointF> points;
	auto *routing = qobject_cast<Diagram *>(scene());
	if (!routing) {
		return points;
	}
	for (const QPointF &mm : m_route.points) {
		points << CabinetRouting::toScene(routing, mm);
	}
	return points;
}

QRectF CableRouteItem::boundingRect() const
{
	const QVector<QPointF> points = scenePoints();
	if (points.isEmpty()) {
		return QRectF();
	}
	QPolygonF polygon(points);
	return polygon.boundingRect().adjusted(-6, -6, 6, 6);
}

QPainterPath CableRouteItem::shape() const
{
	QPainterPath path;
	const QVector<QPointF> points = scenePoints();
	if (points.size() < 2) {
		return path;
	}
	path.addPolygon(QPolygonF(points));
	QPainterPathStroker stroker;
	stroker.setWidth(6);
	return stroker.createStroke(path);
}

void CableRouteItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
						   QWidget *)
{
	const QVector<QPointF> points = scenePoints();
	if (points.size() < 2) {
		return;
	}
		//One colour, moved by hand or not: red only for a way no duct
		//joins, as it was calculated
	QColor color(30, 110, 200);
	if (!m_route.through_ducts && !m_route.manual) {
		color = QColor(210, 30, 30);
	}
	const bool selected = option->state & QStyle::State_Selected;
	QPen pen(color, selected ? 3 : 1.5);
	pen.setCosmetic(true);
	pen.setJoinStyle(Qt::RoundJoin);
	painter->setRenderHint(QPainter::Antialiasing, true);
	painter->setPen(pen);
	painter->setBrush(Qt::NoBrush);
	painter->drawPolyline(QPolygonF(points));

		//Its two ends, and its corners to drag when selected
	const qreal unit = 1.0 / qMax<qreal>(painter->worldTransform().m11(), 0.01);
	painter->setPen(Qt::NoPen);
	painter->setBrush(color);
	painter->drawEllipse(points.first(), 2.5 * unit, 2.5 * unit);
	painter->drawEllipse(points.last(), 2.5 * unit, 2.5 * unit);
	if (selected) {
		painter->setBrush(Qt::white);
		painter->setPen(QPen(color, 1.0 * unit));
		for (int i = 1 ; i < points.size() - 1 ; ++i) {
			painter->drawRect(QRectF(points.at(i) - QPointF(3.5, 3.5) * unit,
									 QSizeF(7, 7) * unit));
		}
	}
}

QVariant CableRouteItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
	if (change == ItemSelectedHasChanged) {
			//Selected, it says its wire and its length
		placeInfo();
		m_info->setVisible(value.toBool());
	} else if (change == ItemSceneHasChanged) {
		placeInfo();
	}
	return QGraphicsObject::itemChange(change, value);
}

void CableRouteItem::placeInfo()
{
	if (!m_info) {
		return;
	}
	static_cast<RouteInfoItem *>(m_info)->setText(infoText());
	const QVector<QPointF> points = scenePoints();
	if (points.size() < 2) {
		return;
	}
		//At the middle of its length
	const qreal half = CabinetRouter::lengthOf(points) / 2.0;
	qreal run = 0;
	QPointF middle = points.first();
	for (int i = 1 ; i < points.size() ; ++i) {
		const QLineF segment(points.at(i - 1), points.at(i));
		if (run + segment.length() >= half) {
			middle = segment.pointAt(segment.length() > 0 ? (half - run) / segment.length() : 0);
			break;
		}
		run += segment.length();
	}
	m_info->setPos(middle);
}

qreal CableRouteItem::tolerance(const QWidget *widget) const
{
	qreal scale = 1.0;
	if (widget) {
		if (auto *view = qobject_cast<const QGraphicsView *>(widget->parentWidget())) {
			scale = view->transform().m11();
		}
	}
	return 6.0 / qMax<qreal>(scale, 0.01);
}

int CableRouteItem::cornerAt(const QPointF &scene_pos, qreal tolerance) const
{
	const QVector<QPointF> points = scenePoints();
	for (int i = 1 ; i < points.size() - 1 ; ++i) {
		if (QLineF(points.at(i), scene_pos).length() <= tolerance) {
			return i;
		}
	}
	return -1;
}

int CableRouteItem::segmentAt(const QPointF &scene_pos, qreal tolerance) const
{
	const QVector<QPointF> points = scenePoints();
	for (int i = 1 ; i < points.size() ; ++i) {
		QPainterPath path;
		path.moveTo(points.at(i - 1));
		path.lineTo(points.at(i));
		QPainterPathStroker stroker;
		stroker.setWidth(tolerance * 2);
		if (stroker.createStroke(path).contains(scene_pos)) {
			return i - 1;
		}
	}
	return -1;
}

void CableRouteItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	auto *routing = qobject_cast<Diagram *>(scene());
	if (event->button() == Qt::LeftButton && routing) {
		const qreal reach = tolerance(event->widget());
		const int corner = isSelected() ? cornerAt(event->scenePos(), reach) : -1;
		if (corner >= 0) {
			m_dragged = corner;
			m_drag_start = m_route;
			event->accept();
			return;
		}
			//A stretch: selected with the press, dragged across at once
		const int stretch = segmentAt(event->scenePos(), reach);
		QGraphicsObject::mousePressEvent(event);
		if (stretch >= 0) {
			m_stretch = stretch;
			m_drag_start = m_route;
			m_press_mm = CabinetRouting::toMm(routing, event->scenePos());
			m_ducts = CabinetRouting::ductsOf(routing);
			event->accept();
		}
		return;
	}
	QGraphicsObject::mousePressEvent(event);
}

/**
	@brief CableRouteItem::dragStretch
	The stretch pressed follows the mouse across, its ends square to the
	stretches before and after it; near the middle of a duct running the
	same way, it catches on it. A stretch from a device gets a corner: the
	device stays where it is.
*/
void CableRouteItem::dragStretch(const QPointF &scene_pos, qreal reach)
{
	auto *routing = qobject_cast<Diagram *>(scene());
	if (!routing || m_stretch < 0) {
		return;
	}
	QVector<QPointF> points = m_drag_start.points;
	int a = m_stretch;
	int b = m_stretch + 1;
	if (b >= points.size()) {
		return;
	}
	if (a == 0) {
		points.insert(0, points.first());
		++a;
		++b;
	}
	if (b == points.size() - 1) {
		points.insert(points.size() - 1, points.last());
	}

	const QPointF p = points.at(a);
	const QPointF q = points.at(b);
	const QPointF mm = CabinetRouting::toMm(routing, scene_pos);
	const qreal catch_mm = reach * 2.0 / routing->pixelsPerMm();
	const bool horizontal = std::abs(p.y() - q.y()) < 1e-6;
	const bool vertical = std::abs(p.x() - q.x()) < 1e-6;
	if (horizontal || vertical)
	{
		const Qt::Orientation way = horizontal ? Qt::Horizontal : Qt::Vertical;
		qreal across = std::round(horizontal ? mm.y() : mm.x());
		const qreal low = horizontal ? qMin(p.x(), q.x()) : qMin(p.y(), q.y());
		const qreal high = horizontal ? qMax(p.x(), q.x()) : qMax(p.y(), q.y());
			//The middle of the nearest duct running the same way
		qreal best = catch_mm;
		for (const CabinetRouter::Duct &duct : std::as_const(m_ducts)) {
			if (duct.orientation != way) {
				continue;
			}
			const QRectF rect = duct.rect.normalized();
			const qreal level = horizontal ? rect.center().y() : rect.center().x();
			const qreal from = horizontal ? rect.left() : rect.top();
			const qreal to = horizontal ? rect.right() : rect.bottom();
			const qreal wanted = horizontal ? mm.y() : mm.x();
			if (to < low || from > high || std::abs(level - wanted) > best) {
				continue;
			}
			best = std::abs(level - wanted);
			across = level;
		}
		if (horizontal) {
			points[a].setY(across);
			points[b].setY(across);
		} else {
			points[a].setX(across);
			points[b].setX(across);
		}
	}
	else
	{
		const QPointF delta = mm - m_press_mm;
		points[a] += delta;
		points[b] += delta;
	}
	CabinetRouting::Route moved = m_drag_start;
	moved.points = points;
	setRoute(moved);
}

void CableRouteItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
		//What a press there would drag
	const qreal reach = tolerance(event->widget());
	if (isSelected() && cornerAt(event->scenePos(), reach) >= 0) {
		setCursor(Qt::SizeAllCursor);
	} else {
		const int stretch = segmentAt(event->scenePos(), reach);
		const QVector<QPointF> points = scenePoints();
		if (stretch >= 0 && stretch + 1 < points.size()) {
			const QPointF p = points.at(stretch);
			const QPointF q = points.at(stretch + 1);
			setCursor(std::abs(p.y() - q.y()) < 1e-6 ? Qt::SizeVerCursor
				: std::abs(p.x() - q.x()) < 1e-6 ? Qt::SizeHorCursor : Qt::SizeAllCursor);
		} else {
			unsetCursor();
		}
	}
	QGraphicsObject::hoverMoveEvent(event);
}

/**
	@brief CableRouteItem::contextMenuEvent
	Reset the position of this way -- of every way selected with it --:
	calculated again between its devices, one step of undo
*/
void CableRouteItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
	auto *routing = qobject_cast<Diagram *>(scene());
	if (!routing) {
		event->ignore();
		return;
	}
	QList<CableRouteItem *> ways;
	if (isSelected()) {
		for (QGraphicsItem *item : routing->selectedItems()) {
			if (auto *way = qgraphicsitem_cast<CableRouteItem *>(item)) {
				ways << way;
			}
		}
	} else {
		routing->clearSelection();
		setSelected(true);
		ways << this;
	}

	QMenu menu;
	QAction *reset = menu.addAction(QCoreApplication::translate("CabinetRouting", "Reset the position"));
	if (menu.exec(event->screenPos()) == reset)
	{
		auto *undo = new QUndoCommand(QCoreApplication::translate(
										  "CabinetRouting", "Reset the position of %n wire(s)", "",
										  ways.size()));
		for (CableRouteItem *way : std::as_const(ways)) {
			const CabinetRouting::Route after = CabinetRouting::reset(routing, way->route());
			if (after != way->route()) {
				new ChangeCableRouteCommand(routing, way->route(), after, undo);
			}
		}
		if (undo->childCount()) {
			routing->undoStack().push(undo);
		} else {
			delete undo;
		}
	}
	event->accept();
}

void CableRouteItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
	unsetCursor();
	QGraphicsObject::hoverLeaveEvent(event);
}

void CableRouteItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	auto *routing = qobject_cast<Diagram *>(scene());
	if (m_stretch >= 0 && routing) {
		dragStretch(event->scenePos(), tolerance(event->widget()));
		event->accept();
		return;
	}
	if (m_dragged < 0 || !routing) {
		QGraphicsObject::mouseMoveEvent(event);
		return;
	}
	CabinetRouting::Route moved = m_route;
	QPointF mm = CabinetRouting::toMm(routing, event->scenePos());
		//Whole millimetres
	mm = QPointF(std::round(mm.x()), std::round(mm.y()));
	moved.points[m_dragged] = mm;
	setRoute(moved);
	event->accept();
}

void CableRouteItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	if (m_stretch >= 0) {
		m_stretch = -1;
		m_ducts.clear();
		CabinetRouting::Route after = m_route;
		after.points = CabinetRouter::simplified(after.points);
		setRoute(m_drag_start);
		if (after.points != m_drag_start.points) {
			after.manual = true;
			change(after);
		}
		event->accept();
		return;
	}
	if (m_dragged >= 0) {
		m_dragged = -1;
		CabinetRouting::Route after = m_route;
		setRoute(m_drag_start);
		if (after.points != m_drag_start.points) {
			after.manual = true;
			change(after);
		}
		event->accept();
		return;
	}
	QGraphicsObject::mouseReleaseEvent(event);
}

void CableRouteItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
	auto *routing = qobject_cast<Diagram *>(scene());
	if (event->button() != Qt::LeftButton || !routing) {
		QGraphicsObject::mouseDoubleClickEvent(event);
		return;
	}
	const qreal reach = tolerance(event->widget());
	CabinetRouting::Route after = m_route;
	const int corner = cornerAt(event->scenePos(), reach);
	if (corner >= 0) {
			//A corner taken away
		after.points.removeAt(corner);
	} else {
		const int segment = segmentAt(event->scenePos(), reach);
		if (segment < 0) {
			QGraphicsObject::mouseDoubleClickEvent(event);
			return;
		}
			//A corner added where the double click was
		QPointF mm = CabinetRouting::toMm(routing, event->scenePos());
		mm = QPointF(std::round(mm.x()), std::round(mm.y()));
		after.points.insert(segment + 1, mm);
	}
	after.manual = true;
	change(after);
	setSelected(true);
	event->accept();
}

void CableRouteItem::change(const CabinetRouting::Route &after)
{
	auto *routing = qobject_cast<Diagram *>(scene());
	if (!routing) {
		return;
	}
	routing->undoStack().push(new ChangeCableRouteCommand(routing, m_route, after));
}

/* ---------------------------------------------------------------------- */

SetCableRoutesCommand::SetCableRoutesCommand(Diagram *routing,
											 const QList<CabinetRouting::Route> &before,
											 const QList<CabinetRouting::Route> &after,
											 QUndoCommand *parent) :
	QUndoCommand(parent),
	m_routing(routing),
	m_before(before),
	m_after(after)
{
	setText(QCoreApplication::translate("CabinetRouting", "Calculate the routing"));
}

void SetCableRoutesCommand::undo()
{
	if (m_routing) {
		m_routing->showMe();
		CabinetRouting::setRoutes(m_routing, m_before);
	}
}

void SetCableRoutesCommand::redo()
{
	if (m_routing) {
		m_routing->showMe();
		CabinetRouting::setRoutes(m_routing, m_after);
	}
}

ChangeCableRouteCommand::ChangeCableRouteCommand(Diagram *routing,
												 const CabinetRouting::Route &before,
												 const CabinetRouting::Route &after,
												 QUndoCommand *parent) :
	QUndoCommand(parent),
	m_routing(routing),
	m_before(before),
	m_after(after)
{
	setText(QCoreApplication::translate("CabinetRouting", "Change the way of a wire"));
}

void ChangeCableRouteCommand::undo()
{
	apply(m_before);
}

void ChangeCableRouteCommand::redo()
{
	apply(m_after);
}

void ChangeCableRouteCommand::apply(const CabinetRouting::Route &route)
{
	if (!m_routing) {
		return;
	}
	for (QGraphicsItem *item : m_routing->items()) {
		auto *way = qgraphicsitem_cast<CableRouteItem *>(item);
		if (way && way->route().key == route.key) {
			way->setRoute(route);
			return;
		}
	}
}
