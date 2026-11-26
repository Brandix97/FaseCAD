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
#include "qetapptest.h"
#include "ElementsCollection/elementslocation.h"
#include "ElementsCollection/xmlelementcollection.h"
#include "cabinet/cabinetitem.h"
#include "cabinet/cabinetrouting.h"
#include "conductorproperties.h"
#include "diagram.h"
#include "factory/elementfactory.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminal.h"
#include "qetinformation.h"
#include "qetproject.h"
#include "qetresult.h"

#include <QDomDocument>
#include <QGraphicsSceneMouseEvent>
#include <QFile>
#include <QTemporaryDir>
#include <QUndoStack>

/**
	The routing sheet of a mounting plate (Brandon, 2026-11-19): the wire
	12 of the schematic, from -K1 to -K2, goes from the centre of the
	drawing of -K1 up into the duct along the top of the plate, along it,
	and down to -K2: 125 + 300 + 125 mm.
*/
class tst_cabinetrouting : public QObject
{
	Q_OBJECT

	int m_made = 0;
	QTemporaryDir m_dir;

	Element *place(Diagram *diagram, const QString &label)
	{
		const QString name = QStringLiteral("device%1").arg(++m_made);
		const QString xml = QStringLiteral(
				"<definition type=\"element\" link_type=\"simple\" "
				"width=\"20\" height=\"40\" hotspot_x=\"10\" hotspot_y=\"20\">"
				"<uuid uuid=\"%1\"/>"
				"<names><name lang=\"en\">Test</name></names>"
				"<description><rect x=\"-5\" y=\"-10\" width=\"10\" height=\"20\" "
				"style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
				"<terminal x=\"0\" y=\"-20\" orientation=\"n\" name=\"A1\" uuid=\"%2\"/>"
				"<terminal x=\"0\" y=\"20\" orientation=\"s\" name=\"A2\" uuid=\"%3\"/>"
				"</description></definition>")
				.arg(QUuid::createUuid().toString(), QUuid::createUuid().toString(),
					 QUuid::createUuid().toString());
		QDomDocument definition;
		if (!definition.setContent(xml)
			|| !diagram->project()->embeddedElementCollection()->addElementDefinition(
					QStringLiteral("import"), name, definition.documentElement())) {
			return nullptr;
		}
		int state = 0;
		Element *element = ElementFactory::Instance()->createElement(
					ElementsLocation(QStringLiteral("embed://import/%1.elmt").arg(name),
									 diagram->project()),
					nullptr, &state);
		if (state || !element) {
			delete element;
			return nullptr;
		}
		diagram->addItem(element);
		element->setPos(100 + 100 * m_made, 200);
		DiagramContext information = element->elementInformations();
		information.addValue(QETInformation::ELMT_LABEL, label);
		element->setElementInformations(information);
		return element;
	}

	static CabinetItem *part(Diagram *cabinet, CabinetItem::Kind kind, qreal length, qreal width,
							 const QPointF &mm, const QString &label = QString())
	{
		CabinetItem::Properties properties;
		properties.kind = kind;
		properties.length = length;
		properties.width = width;
		properties.device_label = label;
		if (kind == CabinetItem::Kind::Drawing) {
			QPainterPath drawing;
			drawing.addRect(0, 0, length, width);
			properties.drawing = drawing;
		}
		auto *item = new CabinetItem(properties);
		cabinet->addItem(item);
		item->placeAt(mm * cabinet->pixelsPerMm());
		return item;
	}

		/// A schematic with the wire 12 from -K1 to -K2, a cabinet sheet
		/// with a plate, a duct along its top and the two drawings
	Diagram *build(QETProject &project)
	{
		Diagram *schematic = project.addNewDiagram();
		Element *k1 = place(schematic, QStringLiteral("-K1"));
		Element *k2 = place(schematic, QStringLiteral("-K2"));
		if (!k1 || !k2) return nullptr;
		auto *wire = new Conductor(k1->terminals().last(), k2->terminals().last());
		schematic->addItem(wire);
		ConductorProperties properties = wire->properties();
		properties.text = QStringLiteral("12");
		properties.m_wire_section = QStringLiteral("1,5 mm²");
		wire->setProperties(properties);

		Diagram *cabinet = project.addNewCabinetDiagram();
		part(cabinet, CabinetItem::Kind::MountingPlate, 600, 400, {0, 0});
		auto *duct = part(cabinet, CabinetItem::Kind::WireDuct, 600, 40, {0, 0});
		Q_UNUSED(duct)
		part(cabinet, CabinetItem::Kind::Drawing, 45, 90, {100, 100}, QStringLiteral("-K1"));
		part(cabinet, CabinetItem::Kind::Drawing, 45, 90, {400, 100}, QStringLiteral("-K2"));
		return cabinet;
	}

private slots:
	void initTestCase()
	{
		QETProject::setBackupEnabled(false);
		QVERIFY(m_dir.isValid());
	}

	void theWireTakesTheDuct()
	{
		QETProject project;
		Diagram *cabinet = build(project);
		QVERIFY(cabinet);

			//One plate: no question asked
		Diagram *routing = CabinetRouting::addRoutingSheet(nullptr, cabinet);
		QVERIFY(routing);
		QVERIFY(routing->isRoutingFolio());
		QCOMPARE(project.folioIndex(routing), project.folioIndex(cabinet) + 1);
		QCOMPARE(routing->cabinetScale(), cabinet->cabinetScale());
		QVERIFY(CabinetRouting::plateOf(routing));

		CabinetRouting::Summary summary;
		const QList<CabinetRouting::Route> routes =
				CabinetRouting::calculate(routing, {}, true, &summary);
		QCOMPARE(summary.routed, 1);
		QCOMPARE(summary.straight, 0);
		QCOMPARE(routes.size(), 1);
		const CabinetRouting::Route &route = routes.first();
		QCOMPARE(route.number, QStringLiteral("12"));
		QCOMPARE(route.from_label, QStringLiteral("-K1"));
		QCOMPARE(route.to_label, QStringLiteral("-K2"));
		QCOMPARE(route.section, QStringLiteral("1,5 mm²"));
		QVERIFY(route.through_ducts);
		QCOMPARE(route.length(), 550.0);
		const QVector<QPointF> expected{{122.5, 145}, {122.5, 20}, {422.5, 20}, {422.5, 145}};
		QCOMPARE(route.points, expected);

			//Drawn, one step of undo
		routing->undoStack().push(new SetCableRoutesCommand(routing, {}, routes));
		QCOMPARE(CabinetRouting::routesOf(routing).size(), 1);
		routing->undoStack().undo();
		QCOMPARE(CabinetRouting::routesOf(routing).size(), 0);
		routing->undoStack().redo();
		QList<CableRouteItem *> items;
		for (QGraphicsItem *item : routing->items()) {
			if (auto *way = qgraphicsitem_cast<CableRouteItem *>(item)) items << way;
		}
		QCOMPARE(items.size(), 1);
			//Clicked, it says its wire and its length
		QVERIFY2(items.first()->infoText().contains(QStringLiteral("550 mm")),
				 qPrintable(items.first()->infoText()));
		QVERIFY(items.first()->infoText().contains(QStringLiteral("12")));
		QCOMPARE(items.first()->scenePoints().first(),
				 CabinetRouting::toScene(routing, QPointF(122.5, 145)));

			//For a spreadsheet
		const QStringList lines = CabinetRouting::csv(CabinetRouting::routesOf(routing))
				.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
		QCOMPARE(lines.size(), 2);
		const QStringList cells = lines.at(1).split(QLatin1Char(';'));
		QCOMPARE(cells.size(), 10);
		QCOMPARE(cells.at(0), QStringLiteral("12"));
		QCOMPARE(cells.at(1), QStringLiteral("-K1"));
		QCOMPARE(cells.at(3), QStringLiteral("-K2"));
		QCOMPARE(cells.at(5), QStringLiteral("1,5 mm²"));
		QCOMPARE(cells.at(8), QStringLiteral("550"));
	}

	void aWayChangedByHandIsKept()
	{
		QETProject project;
		Diagram *cabinet = build(project);
		QVERIFY(cabinet);
		Diagram *routing = CabinetRouting::addRoutingSheet(nullptr, cabinet);
		QVERIFY(routing);
		const QList<CabinetRouting::Route> routes = CabinetRouting::calculate(routing, {}, true);
		routing->undoStack().push(new SetCableRoutesCommand(routing, {}, routes));

		CabinetRouting::Route changed = routes.first();
		changed.points.insert(2, QPointF(250, 60));
		changed.manual = true;
		routing->undoStack().push(new ChangeCableRouteCommand(routing, routes.first(), changed));
		QCOMPARE(CabinetRouting::routesOf(routing).first().points, changed.points);

		CabinetRouting::Summary summary;
		QList<CabinetRouting::Route> again = CabinetRouting::calculate(
					routing, CabinetRouting::routesOf(routing), true, &summary);
		QCOMPARE(summary.kept, 1);
		QCOMPARE(again.first().points, changed.points);
		QVERIFY(again.first().manual);

			//Unless told otherwise
		again = CabinetRouting::calculate(routing, CabinetRouting::routesOf(routing), false);
		QCOMPARE(again.first().points, routes.first().points);
		QVERIFY(!again.first().manual);

		routing->undoStack().undo();
		QCOMPARE(CabinetRouting::routesOf(routing).first().points, routes.first().points);
	}

	void theSheetIsSavedWithItsWays()
	{
		const QString path = m_dir.filePath(QStringLiteral("routing.qet"));
		QList<CabinetRouting::Route> routes;
		{
			QETProject project;
			Diagram *cabinet = build(project);
			QVERIFY(cabinet);
			Diagram *routing = CabinetRouting::addRoutingSheet(nullptr, cabinet);
			routes = CabinetRouting::calculate(routing, {}, true);
			routes.first().manual = true;
			CabinetRouting::setRoutes(routing, routes);
			project.setFilePath(path);
			QVERIFY(project.write().isOk());
				//QET_ROUTING_PROJECT=<file.qet> keeps a copy to look at
			const QString keep = qEnvironmentVariable("QET_ROUTING_PROJECT");
			if (!keep.isEmpty()) {
				QFile::remove(keep);
				QFile::copy(path, keep);
			}
		}
		QETProject project(path);
		QCOMPARE(project.diagrams().size(), 3);
		Diagram *routing = project.diagrams().at(2);
		QVERIFY(routing->isRoutingFolio());
		QVERIFY(!project.diagrams().at(0)->isRoutingFolio());
		routing->ensureLoaded();
		const QList<CabinetRouting::Route> read = CabinetRouting::routesOf(routing);
		QCOMPARE(read.size(), 1);
		QVERIFY(read.first() == routes.first());
		project.diagrams().at(1)->ensureLoaded();
		QVERIFY(CabinetRouting::plateOf(routing));

			//A schematic folio is written as before
		QDomDocument document = project.diagrams().at(0)->toXml();
		QVERIFY(!document.documentElement().hasAttribute(QStringLiteral("folioKind")));
	}

	/**
		A stretch of a way dragged across onto the other duct: the wire
		goes that way now, its ends still on their devices (Brandon,
		2026-11-19: "arrastrar el cable hasta la canaleta de arriba")
	*/
	void aStretchIsDraggedOntoAnotherDuct()
	{
		QETProject project;
		Diagram *cabinet = build(project);
		QVERIFY(cabinet);
			//A second duct along the bottom of the plate
		part(cabinet, CabinetItem::Kind::WireDuct, 600, 40, {0, 360});
		Diagram *routing = CabinetRouting::addRoutingSheet(nullptr, cabinet);
		QVERIFY(routing);
		const QList<CabinetRouting::Route> routes = CabinetRouting::calculate(routing, {}, true);
		QCOMPARE(routes.first().points.at(1), QPointF(122.5, 20));
		routing->undoStack().push(new SetCableRoutesCommand(routing, {}, routes));

		auto mouse = [routing](QEvent::Type type, const QPointF &mm) {
			QGraphicsSceneMouseEvent event(type);
			event.setScenePos(CabinetRouting::toScene(routing, mm));
			event.setButton(type == QEvent::GraphicsSceneMouseMove ? Qt::NoButton : Qt::LeftButton);
			event.setButtons(type == QEvent::GraphicsSceneMouseRelease ? Qt::NoButton : Qt::LeftButton);
			QCoreApplication::sendEvent(routing, &event);
		};
			//Pressed on the stretch in the top duct, let go near the
			//middle of the bottom one
		mouse(QEvent::GraphicsSceneMousePress, {272.5, 20});
		mouse(QEvent::GraphicsSceneMouseMove, {272.5, 200});
		mouse(QEvent::GraphicsSceneMouseMove, {272.5, 374});
		mouse(QEvent::GraphicsSceneMouseRelease, {272.5, 374});

		const CabinetRouting::Route moved = CabinetRouting::routesOf(routing).first();
		const QVector<QPointF> expected{{122.5, 145}, {122.5, 380}, {422.5, 380}, {422.5, 145}};
		QCOMPARE(moved.points, expected);
		QVERIFY(moved.manual);
		QCOMPARE(moved.length(), 235.0 + 300.0 + 235.0);

		routing->undoStack().undo();
		QCOMPARE(CabinetRouting::routesOf(routing).first().points, routes.first().points);

			//Its position reset: calculated again, no longer by hand
		routing->undoStack().redo();
		const CabinetRouting::Route reset = CabinetRouting::reset(
					routing, CabinetRouting::routesOf(routing).first());
		QCOMPARE(reset.points, routes.first().points);
		QVERIFY(!reset.manual);
	}

	void noPlateNoSheet()
	{
		QETProject project;
		Diagram *cabinet = project.addNewCabinetDiagram();
		QVERIFY(CabinetRouting::platesOf(cabinet).isEmpty());
	}
};

QET_APP_TEST_MAIN(tst_cabinetrouting)
#include "tst_cabinetrouting.moc"
