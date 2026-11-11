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

/*
	Cabinet construction 2D: a folio of the kind "cabinet" is drawn to
	scale, and holds mounting plates, DIN rails and wire ducts.

	* a cabinet folio says so in its XML with its scale, a schematic folio
	  is written as it always was, and both read back what they wrote;
	* the parts keep their measures in millimetres: another scale draws
	  them again, where they stood in the cabinet, and comes back on undo;
	* the parts are saved and read with their folio, a part of a kind this
	  program does not know is left out;
	* the tool draws a duct between two clicks, horizontal or vertical,
	  says its length under the mouse, takes a length typed, and a plate
	  is put down with one click; each goes to the undo stack;
	* the magnet on, a part catches the corners and the middles of the
	  sides of the other parts, lines up with them, moves in whole
	  millimetres otherwise, and a marker shows what it caught; the
	  magnet off, nothing catches; the ends of a duct and the points of a
	  dimension catch the same way;
	* a dimension on a cabinet folio says the real measure.
*/

#include "cabinet/cabinetcommands.h"
#include "cabinet/cabinetitem.h"
#include "cabinet/cabinetsnap.h"
#include "diagram.h"
#include "diagramcontent.h"
#include "diagramevent/diagrameventaddcabinetitem.h"
#include "qetgraphicsitem/dimensionitem.h"
#include "qetproject.h"
#include "qetresult.h"
#include "undocommand/addgraphicsobjectcommand.h"

#include <QDomDocument>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QtTest>

class TstCabinetFolio : public QObject
{
		Q_OBJECT

	private slots:
		void initTestCase();
		void kindAndScaleAreSaved();
		void schematicFolioIsWrittenAsBefore();
		void partsKeepTheirMillimetres();
		void scaleChangeIsUndone();
		void partsAreSavedWithTheirFolio();
		void unknownPartIsLeftOut();
		void projectFileKeepsTheCabinet();
		void toolDrawsADuctBetweenTwoClicks();
		void toolDrawsVertically();
		void toolTakesATypedLength();
		void toolPutsAPlateDown();
		void escapeGivesThePartUpThenLeaves();
		void partsCatchCornersAndMiddles();
		void partsAreSelectedAndDeleted();
		void dimensionSaysTheRealMeasure();

	private:
		static void press(DiagramEventAddCabinetItem *tool, const QPointF &pos,
						  Qt::MouseButton button = Qt::LeftButton);
		static void move(DiagramEventAddCabinetItem *tool, const QPointF &pos);
		static void key(Diagram *diagram, int key, const QString &text = QString());
		static QList<CabinetItem *> partsOf(Diagram *diagram);

		QTemporaryDir m_settings;
};

void TstCabinetFolio::initTestCase()
{
		//Settings of its own: the grid snap of the user is not the test's
	QVERIFY(m_settings.isValid());
	QStandardPaths::setTestModeEnabled(true);
	QCoreApplication::setOrganizationName(QStringLiteral("FaseCADTest"));
	QCoreApplication::setApplicationName(QStringLiteral("tst_cabinetfolio"));
	QSettings::setDefaultFormat(QSettings::IniFormat);
	QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
	Diagram::setGridSnap(true);
}

void TstCabinetFolio::press(DiagramEventAddCabinetItem *tool, const QPointF &pos,
							Qt::MouseButton button)
{
	QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMousePress);
	event.setScenePos(pos);
	event.setScreenPos(pos.toPoint());
	event.setButton(button);
	event.setButtons(button);
	tool->mousePressEvent(&event);
}

void TstCabinetFolio::move(DiagramEventAddCabinetItem *tool, const QPointF &pos)
{
	QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMouseMove);
	event.setScenePos(pos);
	event.setScreenPos(pos.toPoint());
	tool->mouseMoveEvent(&event);
}

void TstCabinetFolio::key(Diagram *diagram, int key, const QString &text)
{
	QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier, text);
	QCoreApplication::sendEvent(diagram, &event);
}

QList<CabinetItem *> TstCabinetFolio::partsOf(Diagram *diagram)
{
	QList<CabinetItem *> parts;
	for (QGraphicsItem *item : diagram->items()) {
		if (auto *part = qgraphicsitem_cast<CabinetItem *>(item)) parts << part;
	}
	return parts;
}

void TstCabinetFolio::kindAndScaleAreSaved()
{
	QETProject project;
	Diagram *cabinet = project.addNewCabinetDiagram();
	QVERIFY(cabinet);
	QVERIFY(cabinet->isCabinetFolio());
	QCOMPARE(cabinet->cabinetScale(), Diagram::defaultCabinetScale);
		//2 px for a millimetre of sheet, a fifth of it at 1:5
	QCOMPARE(cabinet->pixelsPerMm(), 0.4);
	QVERIFY(!cabinet->title().isEmpty());

	cabinet->setCabinetScale(10.0);
	QDomDocument document = cabinet->toXml();
	const QDomElement root = document.documentElement();
	QCOMPARE(root.attribute(QStringLiteral("folioKind")), QStringLiteral("cabinet"));
	QCOMPARE(root.attribute(QStringLiteral("cabinetScale")).toDouble(), 10.0);

	Diagram *read = project.addNewDiagram();
	QVERIFY(!read->isCabinetFolio());
	QVERIFY(read->fromXml(document));
	QVERIFY(read->isCabinetFolio());
	QCOMPARE(read->cabinetScale(), 10.0);
	QCOMPARE(read->pixelsPerMm(), 0.2);

		//Adding a cabinet folio is undone like adding any folio
	const int folios = project.diagrams().size();
	project.undoStack()->undo();
	QCOMPARE(project.diagrams().size(), folios - 1);
}

void TstCabinetFolio::schematicFolioIsWrittenAsBefore()
{
	QETProject project;
	Diagram *schematic = project.addNewDiagram();
	QVERIFY(schematic);
	QVERIFY(!schematic->isCabinetFolio());
	QCOMPARE(schematic->drawingScale(), 1.0);
	QCOMPARE(schematic->pixelsPerMm(), 2.0);

	const QDomElement root = schematic->toXml().documentElement();
	QVERIFY(!root.hasAttribute(QStringLiteral("folioKind")));
	QVERIFY(!root.hasAttribute(QStringLiteral("cabinetScale")));
	QVERIFY(root.firstChildElement(QStringLiteral("cabinet_items")).isNull());
}

void TstCabinetFolio::partsKeepTheirMillimetres()
{
	QETProject project;
	Diagram *cabinet = project.addNewCabinetDiagram();

	CabinetItem::Properties duct;
	duct.kind = CabinetItem::Kind::WireDuct;
	duct.length = 600;
	duct.width = 40;
	duct.depth = 60;
	auto *part = new CabinetItem(duct);
	cabinet->addItem(part);
	part->placeAt(QPointF(100, 200));

		//1:5 -> 0.4 px per millimetre
	QCOMPARE(part->rect().size(), QSizeF(240, 16));
	QCOMPARE(part->description(), QStringLiteral("40×60 · 600 mm"));

	duct.orientation = Qt::Vertical;
	part->setProperties(duct);
	QCOMPARE(part->rect().size(), QSizeF(16, 240));

		//1:10: half the pixels, the same millimetres, the same place in
		//the cabinet
	cabinet->setCabinetScale(10.0);
	QCOMPARE(part->properties().length, 600.0);
	QCOMPARE(part->rect().size(), QSizeF(8, 120));
	QCOMPARE(part->pos(), QPointF(50, 100));
}

void TstCabinetFolio::scaleChangeIsUndone()
{
	QETProject project;
	Diagram *cabinet = project.addNewCabinetDiagram();
	CabinetItem::Properties plate;
	plate.length = 600;
	plate.width = 800;
	auto *part = new CabinetItem(plate);
	cabinet->addItem(part);
	part->placeAt(QPointF(40, 60));

	cabinet->undoStack().push(new ChangeCabinetScaleCommand(cabinet, 2.5));
	QCOMPARE(cabinet->cabinetScale(), 2.5);
	QCOMPARE(part->rect().size(), QSizeF(480, 640));
	QCOMPARE(part->pos(), QPointF(80, 120));

	cabinet->undoStack().undo();
	QCOMPARE(cabinet->cabinetScale(), 5.0);
	QCOMPARE(part->rect().size(), QSizeF(240, 320));
	QCOMPARE(part->pos(), QPointF(40, 60));

	cabinet->undoStack().redo();
	QCOMPARE(cabinet->cabinetScale(), 2.5);
}

void TstCabinetFolio::partsAreSavedWithTheirFolio()
{
	QETProject project;
	Diagram *cabinet = project.addNewCabinetDiagram();

	CabinetItem::Properties plate;
	plate.length = 600;
	plate.width = 800;
	auto *plate_item = new CabinetItem(plate);
	cabinet->addItem(plate_item);
	plate_item->placeAt(QPointF(100, 100));

	CabinetItem::Properties rail;
	rail.kind = CabinetItem::Kind::DinRail;
	rail.profile = QStringLiteral("TH35-15");
	rail.length = 450;
	rail.width = 35;
	rail.depth = 15;
	rail.orientation = Qt::Vertical;
	auto *rail_item = new CabinetItem(rail);
	cabinet->addItem(rail_item);
	rail_item->placeAt(QPointF(120.5, 130.25));

	CabinetItem::Properties duct;
	duct.kind = CabinetItem::Kind::WireDuct;
	duct.length = 512.5;
	duct.width = 25;
	duct.depth = 40;
	auto *duct_item = new CabinetItem(duct);
	cabinet->addItem(duct_item);
	duct_item->placeAt(QPointF(100, 100));

	QDomDocument document = cabinet->toXml();
	const QDomElement block = document.documentElement()
			.firstChildElement(QStringLiteral("cabinet_items"));
	QCOMPARE(block.elementsByTagName(QStringLiteral("cabinet_item")).size(), 3);

	Diagram *read = project.addNewDiagram();
	QVERIFY(read->fromXml(document));
	QHash<QUuid, CabinetItem *> by_uuid;
	for (CabinetItem *part : partsOf(read)) by_uuid.insert(part->uuid(), part);
	QCOMPARE(by_uuid.size(), 3);

	for (CabinetItem *source : {plate_item, rail_item, duct_item}) {
		CabinetItem *copy = by_uuid.value(source->uuid());
		QVERIFY(copy);
		QVERIFY(copy->properties() == source->properties());
		QCOMPARE(copy->pos(), source->pos());
		QCOMPARE(copy->zValue(), source->zValue());
	}
		//The plate under what is mounted on it
	QVERIFY(plate_item->zValue() < duct_item->zValue());
	QVERIFY(plate_item->zValue() < rail_item->zValue());
}

void TstCabinetFolio::unknownPartIsLeftOut()
{
	QDomDocument document;
	QDomElement xml = document.createElement(QStringLiteral("cabinet_item"));
	xml.setAttribute(QStringLiteral("kind"), QStringLiteral("something_new"));
	xml.setAttribute(QStringLiteral("length"), QStringLiteral("100"));
	CabinetItem part;
	QVERIFY(!part.fromXml(xml));

	xml.setAttribute(QStringLiteral("kind"), QStringLiteral("din_rail"));
	QVERIFY(part.fromXml(xml));
	QCOMPARE(part.kind(), CabinetItem::Kind::DinRail);
}

void TstCabinetFolio::projectFileKeepsTheCabinet()
{
	const QString path = m_settings.filePath(QStringLiteral("cabinet.qet"));
	QUuid part_uuid;
	{
		QETProject project;
		project.addNewDiagram();
		Diagram *cabinet = project.addNewCabinetDiagram();
		cabinet->setCabinetScale(10.0);
		CabinetItem::Properties duct;
		duct.kind = CabinetItem::Kind::WireDuct;
		duct.length = 750;
		duct.width = 60;
		duct.depth = 80;
		duct.orientation = Qt::Vertical;
		auto *part = new CabinetItem(duct);
		cabinet->addItem(part);
		part->placeAt(QPointF(84, 126));
		part_uuid = part->uuid();
		project.setFilePath(path);
		QVERIFY(project.write().isOk());
	}

		//Opened again, then saved as opened and opened once more: the
		//folio is still a cabinet at 1:10 holding its duct, once
	for (int pass = 0 ; pass < 2 ; ++pass) {
		QETProject project(path);
		QCOMPARE(project.diagrams().size(), 2);
		QVERIFY(!project.diagrams().first()->isCabinetFolio());
		Diagram *cabinet = project.diagrams().at(1);
		QVERIFY(cabinet->isCabinetFolio());
		QCOMPARE(cabinet->cabinetScale(), 10.0);
		if (pass == 0) {
				//Saved without a look at the folio: as it was read
			QVERIFY(project.write().isOk());
			continue;
		}
		cabinet->ensureLoaded();
		const QList<CabinetItem *> parts = partsOf(cabinet);
		QCOMPARE(parts.size(), 1);
		QCOMPARE(parts.first()->uuid(), part_uuid);
		QCOMPARE(parts.first()->properties().length, 750.0);
		QCOMPARE(parts.first()->properties().width, 60.0);
		QCOMPARE(parts.first()->properties().depth, 80.0);
		QCOMPARE(parts.first()->properties().orientation, Qt::Vertical);
		QCOMPARE(parts.first()->pos(), QPointF(84, 126));
		QCOMPARE(parts.first()->rect().size(), QSizeF(12, 150));
	}
}

void TstCabinetFolio::toolDrawsADuctBetweenTwoClicks()
{
	QETProject project;
	Diagram *cabinet = project.addNewCabinetDiagram();
	const int commands = cabinet->undoStack().count();

	CabinetItem::Properties duct;
	duct.kind = CabinetItem::Kind::WireDuct;
	duct.width = 40;
	duct.depth = 60;
	auto *tool = new DiagramEventAddCabinetItem(cabinet, duct);
	cabinet->setEventInterface(tool);
	QVERIFY(cabinet->eventInterfaceIsRunning());
	QVERIFY(!tool->preview());
	QVERIFY(tool->readout().isEmpty());

	press(tool, QPointF(100, 100));
	QVERIFY(tool->isDrawing());
	QVERIFY(tool->preview());

		//200 px to the right, a little down: horizontal, 500 mm at 1:5
	move(tool, QPointF(300, 104));
	QCOMPARE(tool->preview()->properties().orientation, Qt::Horizontal);
	QCOMPARE(tool->preview()->properties().length, 500.0);
	QCOMPARE(tool->readout(), QStringLiteral("500 mm"));
		//Drawn around the line of the first click: 40 mm = 16 px across
	QCOMPARE(tool->preview()->sceneRect(), QRectF(100, 92, 200, 16));
		//Not on the undo stack until the second click
	QCOMPARE(cabinet->undoStack().count(), commands);

	press(tool, QPointF(300, 104));
	QVERIFY(!tool->isDrawing());
	QCOMPARE(cabinet->undoStack().count(), commands + 1);
	QList<CabinetItem *> parts = partsOf(cabinet);
	QCOMPARE(parts.size(), 1);
	QCOMPARE(parts.first()->properties().length, 500.0);
	QCOMPARE(parts.first()->properties().width, 40.0);
	QCOMPARE(parts.first()->properties().depth, 60.0);
		//Left where it was drawn
	QCOMPARE(parts.first()->sceneRect(), QRectF(100, 92, 200, 16));

		//The tool waits for the next duct
	QVERIFY(cabinet->eventInterfaceIsRunning());

	cabinet->undoStack().undo();
	QCOMPARE(partsOf(cabinet).size(), 0);
	cabinet->undoStack().redo();
	QCOMPARE(partsOf(cabinet).size(), 1);
	QCOMPARE(partsOf(cabinet).first()->sceneRect(), QRectF(100, 92, 200, 16));
	cabinet->clearEventInterface();
}

void TstCabinetFolio::toolDrawsVertically()
{
	QETProject project;
	Diagram *cabinet = project.addNewCabinetDiagram();

	CabinetItem::Properties rail;
	rail.kind = CabinetItem::Kind::DinRail;
	rail.profile = CabinetItem::defaultRailProfile();
	rail.width = 35;
	auto *tool = new DiagramEventAddCabinetItem(cabinet, rail);
	cabinet->setEventInterface(tool);

	press(tool, QPointF(200, 300));
		//Upwards: the rail ends where it started, 100 px = 250 mm higher
	move(tool, QPointF(206, 200));
	QCOMPARE(tool->preview()->properties().orientation, Qt::Vertical);
	QCOMPARE(tool->preview()->properties().length, 250.0);
		//35 mm = 14 px across, around the first click
	QCOMPARE(tool->preview()->sceneRect(), QRectF(193, 200, 14, 100));
	QCOMPARE(tool->readout(), QStringLiteral("250 mm"));

		//A click where it started leaves nothing: no rail without length
	move(tool, QPointF(200, 300));
	press(tool, QPointF(200, 300));
	QCOMPARE(partsOf(cabinet).size(), 1);   //the preview only
	QVERIFY(tool->isDrawing());
	cabinet->clearEventInterface();
	QCOMPARE(partsOf(cabinet).size(), 0);
}

void TstCabinetFolio::toolTakesATypedLength()
{
	QETProject project;
	Diagram *cabinet = project.addNewCabinetDiagram();

	CabinetItem::Properties duct;
	duct.kind = CabinetItem::Kind::WireDuct;
	duct.width = 25;
	auto *tool = new DiagramEventAddCabinetItem(cabinet, duct);
	cabinet->setEventInterface(tool);

		//Typing before a duct is started is not the tool's
	QKeyEvent early(QEvent::KeyPress, Qt::Key_6, Qt::NoModifier, QStringLiteral("6"));
	QVERIFY(!tool->wantsKey(&early));

	press(tool, QPointF(100, 100));
	move(tool, QPointF(180, 100));
	QVERIFY(tool->wantsKey(&early));

	key(cabinet, Qt::Key_6, QStringLiteral("6"));
	key(cabinet, Qt::Key_3, QStringLiteral("3"));
	key(cabinet, Qt::Key_Backspace);
	key(cabinet, Qt::Key_0, QStringLiteral("0"));
	key(cabinet, Qt::Key_0, QStringLiteral("0"));
	QCOMPARE(tool->preview()->properties().length, 600.0);
	QVERIFY(tool->readout().startsWith(QStringLiteral("600 mm")));
		//The mouse still says which way, no longer how far
	move(tool, QPointF(20, 100));
	QCOMPARE(tool->preview()->properties().length, 600.0);
	QCOMPARE(tool->preview()->sceneRect().right(), 100.0);

	key(cabinet, Qt::Key_Return);
	QVERIFY(!tool->isDrawing());
	const QList<CabinetItem *> parts = partsOf(cabinet);
	QCOMPARE(parts.size(), 1);
	QCOMPARE(parts.first()->properties().length, 600.0);
	QCOMPARE(parts.first()->sceneRect().width(), 240.0);
	cabinet->clearEventInterface();
}

void TstCabinetFolio::toolPutsAPlateDown()
{
	QETProject project;
	Diagram *cabinet = project.addNewCabinetDiagram();

	CabinetItem::Properties plate;
	plate.length = 600;
	plate.width = 800;
	auto *tool = new DiagramEventAddCabinetItem(cabinet, plate);
	cabinet->setEventInterface(tool);
		//It follows the mouse, hidden until the mouse says where
	QVERIFY(tool->preview());
	QVERIFY(!tool->preview()->isVisible());

	move(tool, QPointF(50.9, 61.1));
	QVERIFY(tool->preview()->isVisible());
		//Near nothing: on the whole millimetre, 0.4 px at 1:5
	QCOMPARE(tool->preview()->pos(), QPointF(50.8, 61.2));
	QCOMPARE(tool->readout(), QStringLiteral("600 × 800 mm"));

	press(tool, QPointF(50.9, 61.1));
		//One plate, and the tool is done
	QVERIFY(!cabinet->eventInterfaceIsRunning());
	const QList<CabinetItem *> parts = partsOf(cabinet);
	QCOMPARE(parts.size(), 1);
	QCOMPARE(parts.first()->kind(), CabinetItem::Kind::MountingPlate);
	QCOMPARE(parts.first()->sceneRect(), QRectF(50.8, 61.2, 240, 320));

	cabinet->undoStack().undo();
	QCOMPARE(partsOf(cabinet).size(), 0);
}

void TstCabinetFolio::escapeGivesThePartUpThenLeaves()
{
	QETProject project;
	Diagram *cabinet = project.addNewCabinetDiagram();

	CabinetItem::Properties duct;
	duct.kind = CabinetItem::Kind::WireDuct;
	duct.width = 40;
	auto *tool = new DiagramEventAddCabinetItem(cabinet, duct);
	cabinet->setEventInterface(tool);
	const int commands = cabinet->undoStack().count();

	press(tool, QPointF(100, 100));
	move(tool, QPointF(300, 100));
	QCOMPARE(partsOf(cabinet).size(), 1);

		//The first Escape gives the duct being drawn up, the tool stays
	key(cabinet, Qt::Key_Escape);
	QCOMPARE(partsOf(cabinet).size(), 0);
	QVERIFY(cabinet->eventInterfaceIsRunning());

		//The second leaves the tool
	key(cabinet, Qt::Key_Escape);
	QVERIFY(!cabinet->eventInterfaceIsRunning());
	QCOMPARE(cabinet->undoStack().count(), commands);
}

void TstCabinetFolio::partsCatchCornersAndMiddles()
{
	QETProject project;
	Diagram *cabinet = project.addNewCabinetDiagram();

	CabinetItem::Properties plate;
	plate.length = 600;
	plate.width = 800;
	auto *plate_item = new CabinetItem(plate);
	cabinet->addItem(plate_item);
	plate_item->placeAt(QPointF(101, 99));   //off the millimetre on purpose

		//Its corners and the middles of its sides
	const QVector<CabinetSnap::Target> targets = CabinetSnap::targetsOf(cabinet);
	QCOMPARE(targets.size(), 8);
	int corners = 0;
	for (const CabinetSnap::Target &target : targets)
		if (target.anchor == CabinetSnap::Anchor::Corner) ++corners;
	QCOMPARE(corners, 4);

		//Dropped near the top left corner of the plate: corner on corner
	CabinetSnap::Snap snap = CabinetSnap::topLeft(QRectF(104, 96, 16, 100), cabinet);
	QVERIFY(snap.found);
	QCOMPARE(snap.anchor, CabinetSnap::Anchor::Corner);
	QCOMPARE(snap.value, QPointF(101, 99));

		//The middle of the end of a rail near the middle of the left side
		//of the plate (101, 259): middle on middle
	snap = CabinetSnap::topLeft(QRectF(102, 253, 100, 14), cabinet);
	QVERIFY(snap.found);
	QCOMPARE(snap.anchor, CabinetSnap::Anchor::Middle);
	QCOMPARE(snap.at, QPointF(101, 259));
	QCOMPARE(snap.value, QPointF(101, 252));

		//Its right side near the right side of the plate, near no corner
		//and no middle: lined up with the side, the height on the mm
	snap = CabinetSnap::topLeft(QRectF(322, 170, 16, 100), cabinet);
	QVERIFY(!snap.found);
	QVERIFY(snap.caught_x);
	QVERIFY(!snap.caught_y);
	QCOMPARE(snap.value, QPointF(325, 170));

		//Near nothing: on the whole millimetre, no longer on a grid
	snap = CabinetSnap::topLeft(QRectF(160.3, 330.9, 16, 100), cabinet);
	QVERIFY(!snap.found);
	QCOMPARE(snap.value, QPointF(160.4, 330.8));

		//A point (the end of a duct, of a dimension) does the same
	snap = CabinetSnap::point(QPointF(339, 257), cabinet);
	QVERIFY(snap.found);
	QCOMPARE(snap.value, QPointF(341, 259));

		//The magnet off: nothing catches, not even the millimetre
	Diagram::setGridSnap(false);
	snap = CabinetSnap::topLeft(QRectF(104, 96, 16, 100), cabinet);
	QVERIFY(!snap.found);
	QCOMPARE(snap.value, QPointF(104, 96));
	Diagram::setGridSnap(true);

		//A part dragged does the same, the plate it is on left where it
		//is, and the marker shows the corner caught until the mouse is up
	CabinetItem::Properties duct;
	duct.kind = CabinetItem::Kind::WireDuct;
	duct.length = 250;
	duct.width = 40;
	auto *duct_item = new CabinetItem(duct);
	cabinet->addItem(duct_item);
	duct_item->setPos(QPointF(103, 102));
	QCOMPARE(duct_item->pos(), QPointF(101, 99));
	QCOMPARE(plate_item->pos(), QPointF(101, 99));
	QVERIFY(CabinetSnap::markerShown(cabinet));
	CabinetSnap::hideMarker(cabinet);
	QVERIFY(!CabinetSnap::markerShown(cabinet));

		//A duct drawn from near the corner of the plate starts on it
	CabinetItem::Properties drawn;
	drawn.kind = CabinetItem::Kind::WireDuct;
	drawn.width = 40;
	auto *tool = new DiagramEventAddCabinetItem(cabinet, drawn);
	cabinet->setEventInterface(tool);
	move(tool, QPointF(339, 418));
	QVERIFY(CabinetSnap::markerShown(cabinet));
	press(tool, QPointF(339, 418));
		//Its other end near the middle of the bottom of the plate
	move(tool, QPointF(223, 421));
	QCOMPARE(tool->preview()->properties().length, 300.0);   //120 px
	QCOMPARE(tool->preview()->sceneRect(), QRectF(221, 411, 120, 16));
	cabinet->clearEventInterface();
	QVERIFY(!CabinetSnap::markerShown(cabinet));
}

void TstCabinetFolio::partsAreSelectedAndDeleted()
{
	QETProject project;
	Diagram *cabinet = project.addNewCabinetDiagram();

	CabinetItem::Properties rail;
	rail.kind = CabinetItem::Kind::DinRail;
	rail.length = 300;
	auto *part = new CabinetItem(rail);
	cabinet->undoStack().push(new AddGraphicsObjectCommand(part, cabinet));
	part->setSelected(true);

	DiagramContent content(cabinet);
	QCOMPARE(content.m_cabinet_items.size(), 1);
	QCOMPARE(content.count(DiagramContent::CabinetItems), 1);
	QVERIFY(content.items(DiagramContent::All).contains(part));
	QVERIFY(content.hasDeletableItems());
	QVERIFY(content.hasCopiableItems());

		//Copied with the selection
	const QDomElement copied = cabinet->toXml(false).documentElement()
			.firstChildElement(QStringLiteral("cabinet_items"));
	QCOMPARE(copied.elementsByTagName(QStringLiteral("cabinet_item")).size(), 1);
}

void TstCabinetFolio::dimensionSaysTheRealMeasure()
{
	QETProject project;
	Diagram *schematic = project.addNewDiagram();
	Diagram *cabinet = project.addNewCabinetDiagram();

	auto *on_schematic = new DimensionItem(QPointF(0, 0), QPointF(100, 0), 20.0);
	schematic->addItem(on_schematic);
	QCOMPARE(on_schematic->dimensionValueMM(), 50.0);

		//100 px of a folio at 1:5 are 250 mm of cabinet
	auto *on_cabinet = new DimensionItem(QPointF(0, 0), QPointF(100, 0), 20.0);
	cabinet->addItem(on_cabinet);
	QCOMPARE(on_cabinet->dimensionValueMM(), 250.0);
}

QTEST_MAIN(TstCabinetFolio)
#include "tst_cabinetfolio.moc"
