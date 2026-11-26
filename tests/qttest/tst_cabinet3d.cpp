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
#include "cabinet/cabinet3d.h"
#include "cabinet/cabinet3dview.h"
#include "cabinet/cabinetdevices.h"
#include "cabinet/cabinetenclosure.h"
#include "cabinet/cabinetitem.h"
#include "cabinet/cabinetspace.h"
#include "cabinet/cabinetstep.h"
#include "cabinet/cabinetclearances.h"
#include "cabinet/cabinetthermal.h"
#include "diagramevent/diagrameventaddcabinetitem.h"
#include "ElementsCollection/elementslocation.h"
#include "ElementsCollection/xmlelementcollection.h"
#include "dataBase/projectdatabase.h"
#include "diagram.h"
#include "factory/elementfactory.h"
#include "partsdb/partsdatabase.h"
#include "partsdb/partpickerdialog.h"
#include "partsdb/partslinks.h"
#include "qetgraphicsitem/element.h"
#include "qetinformation.h"
#include "qetproject.h"

#include <QDir>
#include <QHash>

#include <algorithm>
#include <functional>
#include <QElapsedTimer>
#include <QDomDocument>
#include <QFile>
#include <QPainterPath>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QCheckBox>
#include <QSet>
#include <QSettings>
#include <QTableWidget>
#include <QUndoStack>

using Cabinet3d::Solid;

/**
	The cabinet of a cabinet folio in 3D (Brandon, 2026-11-20): a plate of
	600 x 400 mm, a duct along its top, a rail across it and the drawing
	of -K1, 45 x 90 mm, on the rail.
*/
class tst_cabinet3d : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;

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

	int m_made = 0;

		/// A symbol of the schematic labelled @p label, made of @p parts
	Element *symbol(Diagram *diagram, const QString &label, const QString &parts)
	{
		const QString name = QStringLiteral("device%1").arg(++m_made);
		const QString xml = QStringLiteral(
				"<definition type=\"element\" link_type=\"simple\" "
				"width=\"20\" height=\"40\" hotspot_x=\"10\" hotspot_y=\"20\">"
				"<uuid uuid=\"%1\"/>"
				"<names><name lang=\"en\">Test</name></names>"
				"<description><rect x=\"-5\" y=\"-10\" width=\"10\" height=\"20\" "
				"style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
				"<terminal x=\"0\" y=\"-20\" orientation=\"n\" name=\"1\" uuid=\"%2\"/>"
				"</description></definition>")
				.arg(QUuid::createUuid().toString(), QUuid::createUuid().toString());
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
		element->setPos(100 + 60 * m_made, 200);
		DiagramContext information = element->elementInformations();
		information.addValue(QETInformation::ELMT_LABEL, label);
		information.addValue(PartsLinks::infoKey(), parts);
		element->setElementInformations(information);
		return element;
	}

	static Diagram *build(QETProject &project)
	{
		Diagram *cabinet = project.addNewCabinetDiagram();
		part(cabinet, CabinetItem::Kind::MountingPlate, 600, 400, {0, 0});
		part(cabinet, CabinetItem::Kind::WireDuct, 600, 40, {0, 0});
		part(cabinet, CabinetItem::Kind::DinRail, 600, 35, {0, 130});
		part(cabinet, CabinetItem::Kind::Drawing, 45, 90, {100, 100}, QStringLiteral("-K1"));
		return cabinet;
	}

	static const Solid *find(const QList<Solid> &solids, Solid::Kind kind)
	{
		for (const Solid &solid : solids)
		{
			if (solid.kind == kind) return &solid;
		}
		return nullptr;
	}

	static bool near(const QVector3D &a, const QVector3D &b)
	{
		return (a - b).length() < 0.05f;
	}

		/// The STEP file of one box, @p size big, its corner at 0
	QByteArray boxStep(const QVector3D &size)
	{
		Solid box;
		box.kind = Solid::Kind::Device;
		box.name = QStringLiteral("box");
		box.min = QVector3D(0, 0, 0);
		box.max = size;
		box.color = Qt::darkGray;
		const QString path = m_dir.filePath(QStringLiteral("box.step"));
		if (!CabinetStep::write({box}, path)) return QByteArray();
		QFile file(path);
		file.open(QIODevice::ReadOnly);
		return file.readAll();
	}

private slots:
	void initTestCase()
	{
		QETProject::setBackupEnabled(false);
		QVERIFY(m_dir.isValid());
			//Not the cache of the one running the tests
		CabinetStep::setCacheFolder(m_dir.filePath(QStringLiteral("cache")));
	}

	void theCabinetStandsOnThePlate()
	{
		QETProject project;
		Diagram *cabinet = build(project);
		const QList<Solid> solids = Cabinet3d::solidsOf(cabinet, false);
		QCOMPARE(solids.size(), 4);

		const Solid *plate = find(solids, Solid::Kind::Plate);
		QVERIFY(plate);
			//y up: the plate runs down from 0
		QVERIFY(near(plate->min, QVector3D(0, -400, -Cabinet3d::plate_thickness)));
		QVERIFY(near(plate->max, QVector3D(600, 0, 0)));

		const Solid *duct = find(solids, Solid::Kind::Duct);
		QVERIFY(near(duct->min, QVector3D(0, -40, 0)));
		QVERIFY(near(duct->max, QVector3D(600, 0, Cabinet3d::default_duct_height)));

		const Solid *rail = find(solids, Solid::Kind::Rail);
		QVERIFY(rail->max.z() > 5 && rail->max.z() < 20);

			//-K1 stands on the rail
		const Solid *device = find(solids, Solid::Kind::Device);
		QCOMPARE(device->name, QStringLiteral("-K1"));
		QVERIFY(near(device->min, QVector3D(100, -190, rail->max.z())));
		QVERIFY(near(device->max, QVector3D(145, -100, rail->max.z() + Cabinet3d::default_device_depth)));

		QVector3D min, max;
		Cabinet3d::bounds(solids, min, max);
		QVERIFY(near(min, QVector3D(0, -400, -Cabinet3d::plate_thickness)));
	}

	void railsAndDuctsAreMadeOfTheirPieces()
	{
		QETProject project;
		const QList<Solid> solids = Cabinet3d::solidsOf(build(project), false);
		auto inside = [](const Cabinet3d::Piece &piece, const Solid &solid) {
			const QVector3D e(0.01f, 0.01f, 0.01f);
			return piece.min.x() >= solid.min.x() - e.x() && piece.min.y() >= solid.min.y() - e.y()
					&& piece.min.z() >= solid.min.z() - e.z() && piece.max.x() <= solid.max.x() + e.x()
					&& piece.max.y() <= solid.max.y() + e.y() && piece.max.z() <= solid.max.z() + e.z();
		};
		auto covered = [](const QVector<Cabinet3d::Piece> &pieces, const QVector3D &point) {
			for (const Cabinet3d::Piece &piece : pieces)
			{
				if (point.x() > piece.min.x() && point.x() < piece.max.x() && point.y() > piece.min.y()
					&& point.y() < piece.max.y() && point.z() > piece.min.z() && point.z() < piece.max.z())
					return true;
			}
			return false;
		};

			//The rail of 600 mm: 23 long holes in its bottom, the first
			//from 17.5 to 32.5 mm, its lips at the top
		const Solid *rail = find(solids, Solid::Kind::Rail);
		const QVector<Cabinet3d::Piece> rail_pieces = Cabinet3d::piecesOf(*rail);
		QVERIFY(rail_pieces.size() > 23);
		for (const Cabinet3d::Piece &piece : rail_pieces) QVERIFY(inside(piece, *rail));
		QVERIFY(!covered(rail_pieces, QVector3D(25, rail->center().y(), rail->min.z() + 0.5f)));
		QVERIFY(covered(rail_pieces, QVector3D(36, rail->center().y(), rail->min.z() + 0.5f)));
		QVERIFY(covered(rail_pieces, QVector3D(300, rail->min.y() + 2, rail->max.z() - 0.5f)));
			//Nothing in the middle of the hat but air
		QVERIFY(!covered(rail_pieces, QVector3D(300, rail->center().y(), rail->min.z() + 4)));

			//The duct: its slots, and its cover only when asked for
		const Solid *duct = find(solids, Solid::Kind::Duct);
		const QVector<Cabinet3d::Piece> closed = Cabinet3d::piecesOf(*duct, true);
		const QVector<Cabinet3d::Piece> open = Cabinet3d::piecesOf(*duct, false);
		QCOMPARE(closed.size(), open.size() + 1);
		for (const Cabinet3d::Piece &piece : closed) QVERIFY(inside(piece, *duct));
		float top = 0;
		for (const Cabinet3d::Piece &piece : open) top = qMax(top, piece.max.z());
		QVERIFY(top < duct->max.z() - 1);
			//A finger at 3 mm, a slot at 9 mm, in the wall high up
		const float wall_y = duct->min.y() + 1, high = duct->max.z() - 10;
		QVERIFY(covered(open, QVector3D(3, wall_y, high)));
		QVERIFY(!covered(open, QVector3D(9, wall_y, high)));

		const Solid *plate = find(solids, Solid::Kind::Plate);
		QCOMPARE(Cabinet3d::piecesOf(*plate).size(), 1);
	}

	void aModelIsTurnedToItsDrawing()
	{
			//A model lying on its side: 90 wide, 45 high, 70 deep
		const QMatrix4x4 turn = CabinetStep::orientation(QVector3D(90, 45, 70), QVector3D(45, 90, 0));
		const QVector3D corner = turn.map(QVector3D(90, 45, 70));
		QCOMPARE(qRound(qAbs(corner.x())), 45);
		QCOMPARE(qRound(qAbs(corner.y())), 90);
		QCOMPARE(qRound(qAbs(corner.z())), 70);
			//One that fits stays as it is
		QCOMPARE(CabinetStep::orientation(QVector3D(45, 90, 70), QVector3D(45, 90, 60)), QMatrix4x4());

		CabinetStep::Model model;
		model.ok = true;
		model.min = QVector3D(-10, -10, -10);
		model.max = QVector3D(80, 35, 60);
		Solid device;
		device.kind = Solid::Kind::Device;
		device.min = QVector3D(100, -190, 7.5f);
		device.max = QVector3D(145, -100, 67.5f);
		const QMatrix4x4 place = CabinetStep::placement(model, device);
		QVector3D min(1e9f, 1e9f, 1e9f), max(-1e9f, -1e9f, -1e9f);
		for (int i = 0 ; i < 8 ; ++i)
		{
			const QVector3D p = place.map(QVector3D(i & 1 ? model.max.x() : model.min.x(),
													i & 2 ? model.max.y() : model.min.y(),
													i & 4 ? model.max.z() : model.min.z()));
			min = QVector3D(qMin(min.x(), p.x()), qMin(min.y(), p.y()), qMin(min.z(), p.z()));
			max = QVector3D(qMax(max.x(), p.x()), qMax(max.y(), p.y()), qMax(max.z(), p.z()));
		}
			//In the drawing, its back on the rail
		QVERIFY(near(min, QVector3D(100, -190, 7.5f)));
		QVERIFY(near(max, QVector3D(145, -100, 77.5f)));
	}

	void theCabinetIsWrittenAsStep()
	{
		if (!CabinetStep::available()) QSKIP("Built without OpenCASCADE");
		QETProject project;
		QList<Solid> solids = Cabinet3d::solidsOf(build(project), false);

			//-K1 gets a model of its own, lying on its side
		const QByteArray step = boxStep(QVector3D(90, 45, 70));
		QVERIFY(!step.isEmpty());
		const CabinetStep::Model box = CabinetStep::read(step);
		QVERIFY2(box.ok, qPrintable(box.error));
			//Kept on disk too: the next time, no reading
		QVERIFY(!QDir(CabinetStep::cacheFolder()).entryList({QStringLiteral("*.mesh")}).isEmpty());
		QVERIFY(CabinetStep::cached(step));
		QVERIFY(near(box.max - box.min, QVector3D(90, 45, 70)));
		for (Solid &solid : solids)
		{
			if (solid.kind == Solid::Kind::Device) solid.step = step;
		}

		Cabinet3dView view;
		view.setSolids(solids);
		QCOMPARE(view.withModel(), 1);
		int device = -1;
		for (int i = 0 ; i < solids.size() ; ++i)
		{
			if (solids.at(i).kind == Solid::Kind::Device) device = i;
		}
		QVector3D min, max;
		view.drawnBox(device, min, max);
		QVERIFY(near(max - min, QVector3D(45, 90, 70)));
		QVERIFY(near(min, QVector3D(100, -190, solids.at(device).min.z())));

		const QString path = m_dir.filePath(QStringLiteral("cabinet.step"));
		QString error;
		QVERIFY2(CabinetStep::write(solids, path, &error), qPrintable(error));
		QFile file(path);
		QVERIFY(file.open(QIODevice::ReadOnly));
		const QByteArray written = file.readAll();
		QVERIFY(written.contains("-K1"));
		const CabinetStep::Model cabinet = CabinetStep::read(written);
		QVERIFY2(cabinet.ok, qPrintable(cabinet.error));
		QVERIFY(near(cabinet.min, QVector3D(0, -400, -Cabinet3d::plate_thickness)));
		QVERIFY(near(cabinet.max, QVector3D(600, 0, solids.at(device).min.z() + 70)));

			//The colours go with it: the plate's, and the one -K1's own
			//model had (Brandon, 2026-11-20: all grey in another viewer)
		auto has_colour = [&cabinet](const QColor &colour) {
			for (int i = 0 ; i + CabinetStep::vertex_size <= cabinet.vertices.size() ; i += CabinetStep::vertex_size)
			{
				if (qAbs(cabinet.vertices.at(i + 6) - colour.redF()) < 0.01
					&& qAbs(cabinet.vertices.at(i + 7) - colour.greenF()) < 0.01
					&& qAbs(cabinet.vertices.at(i + 8) - colour.blueF()) < 0.01) {
					return true;
				}
			}
			return false;
		};
		QVERIFY(has_colour(find(solids, Solid::Kind::Plate)->color));
		QVERIFY(has_colour(QColor(Qt::darkGray)));
		QVERIFY(written.count("COLOUR_RGB") >= 4);
	}

		/// QET_CABINET3D_PNG=<file.png>, on a platform with OpenGL, draws
		/// a cabinet without showing it, to look at
	void aPictureToLookAt()
	{
		const QString png = qEnvironmentVariable("QET_CABINET3D_PNG");
		if (png.isEmpty()) QSKIP("QET_CABINET3D_PNG not set");
		QETProject project;
		Diagram *cabinet = project.addNewCabinetDiagram();
		part(cabinet, CabinetItem::Kind::MountingPlate, 600, 800, {0, 0});
		part(cabinet, CabinetItem::Kind::WireDuct, 520, 60, {40, 30});
		part(cabinet, CabinetItem::Kind::WireDuct, 520, 60, {40, 330});
		part(cabinet, CabinetItem::Kind::WireDuct, 520, 60, {40, 630});
		part(cabinet, CabinetItem::Kind::DinRail, 520, 35, {40, 190});
		part(cabinet, CabinetItem::Kind::DinRail, 520, 35, {40, 490});
		int x = 60;
		const QByteArray step = CabinetStep::available() ? boxStep(QVector3D(90, 18, 75)) : QByteArray();
		for (int i = 1 ; i <= 6 ; ++i, x += 50)
			part(cabinet, CabinetItem::Kind::Drawing, 18, 90, {qreal(x), 160}, QStringLiteral("-Q%1").arg(i));
		x = 60;
		for (int i = 1 ; i <= 4 ; ++i, x += 70)
			part(cabinet, CabinetItem::Kind::Drawing, 45, 90, {qreal(x), 460}, QStringLiteral("-K%1").arg(i));
		part(cabinet, CabinetItem::Kind::Drawing, 90, 100, {400, 450}, QStringLiteral("-T1"));
		QList<Solid> solids = Cabinet3d::solidsOf(cabinet, false);
		for (Solid &solid : solids)
		{
			if (solid.name.startsWith(QLatin1String("-Q"))) solid.step = step;
		}
		Cabinet3dView view;
		view.resize(1000, 760);
		view.setSolids(solids);
		const QImage image = view.grabFramebuffer();
		QVERIFY(!image.isNull());
		QVERIFY(image.save(png));
			//The same without the covers of the ducts, then nearer
		view.setDuctCovers(false);
		QVERIFY(view.grabFramebuffer().save(QString(png).replace(QLatin1String(".png"), QLatin1String("-open.png"))));
	}

		/// QET_ENCLOSURE_STEP=<file.stp> QET_ENCLOSURE_PLATE=<name of its
		/// plate> QET_ENCLOSURE_HIDE=<names,...> QET_CABINET3D_PNG=<png>:
		/// a real enclosure, its plate taken, drawn without showing it
	void anEnclosureToLookAt()
	{
		const QString step = qEnvironmentVariable("QET_ENCLOSURE_STEP");
		const QString png = qEnvironmentVariable("QET_CABINET3D_PNG");
		if (step.isEmpty() || png.isEmpty()) QSKIP("QET_ENCLOSURE_STEP or QET_CABINET3D_PNG not set");
		QETProject project;
		Diagram *cabinet = project.addNewCabinetDiagram();
		part(cabinet, CabinetItem::Kind::MountingPlate, 500, 1700, {0, 0});
		part(cabinet, CabinetItem::Kind::WireDuct, 460, 60, {20, 40});
		part(cabinet, CabinetItem::Kind::WireDuct, 460, 60, {20, 400});
		part(cabinet, CabinetItem::Kind::DinRail, 460, 35, {20, 200});
		for (int i = 0 ; i < 5 ; ++i)
			part(cabinet, CabinetItem::Kind::Drawing, 36, 90, {40.0 + 50 * i, 170}, QStringLiteral("-Q%1").arg(i + 1));
		part(cabinet, CabinetItem::Kind::Drawing, 110, 100, {320, 165}, QStringLiteral("-K1"));

		Cabinet3dWindow window(cabinet);
		window.resize(1400, 900);
		CabinetEnclosure::Enclosure enclosure;
		enclosure.source = CabinetEnclosure::Enclosure::Source::File;
		enclosure.path = step;
		QElapsedTimer timer;
		timer.start();
		QVERIFY(window.setEnclosure(enclosure));
		window.waitForModels();
		qInfo("read in %lld ms", timer.elapsed());
		const CabinetStep::Model &model = window.view()->enclosureModel();
		QVERIFY2(model.ok, qPrintable(model.error));
		qInfo("%d nodes, %lld triangles", int(model.nodes.size()),
			  qlonglong(model.vertices.size() / CabinetStep::vertex_size / 3));
		QString plate;
		const QString plate_name = qEnvironmentVariable("QET_ENCLOSURE_PLATE");
		for (const CabinetStep::Node &node : model.nodes)
		{
			if (node.name == plate_name) plate = node.id;
		}
		QVERIFY2(!plate.isEmpty(), "QET_ENCLOSURE_PLATE names a part of the enclosure");
		QVERIFY(window.usePlate(plate, true));
		for (const QString &name : qEnvironmentVariable("QET_ENCLOSURE_HIDE").split(QLatin1Char(','), Qt::SkipEmptyParts))
		{
			for (const CabinetStep::Node &node : window.view()->enclosureModel().nodes)
			{
				if (node.name == name) window.setHiddenKey(QStringLiteral("node:") + node.id, true);
			}
		}
		window.view()->resize(1100, 860);
		window.view()->fit();
		const QImage image = window.view()->grabFramebuffer();
		QVERIFY(image.save(png));
	}

		/// QET_DEVICE_STEP=<file.stp>: the colours of a manufacturer's
		/// model, those of its faces over that of the whole part, as the
		/// file writes them (sRGB); QET_CABINET3D_PNG draws it too
	void aModelKeepsTheColoursOfItsFaces()
	{
		const QString path = qEnvironmentVariable("QET_DEVICE_STEP");
		if (path.isEmpty()) QSKIP("QET_DEVICE_STEP not set");
		QFile file(path);
		QVERIFY(file.open(QIODevice::ReadOnly));
		const QByteArray step = file.readAll();
		const CabinetStep::Model model = CabinetStep::read(step);
		QVERIFY2(model.ok, qPrintable(model.error));
		QHash<QRgb, int> colours;
		for (int i = 0 ; i + CabinetStep::vertex_size <= model.vertices.size() ; i += CabinetStep::vertex_size)
		{
			colours[QColor::fromRgbF(model.vertices.at(i + 6), model.vertices.at(i + 7),
									 model.vertices.at(i + 8)).rgb()] += 1;
		}
		QList<QPair<int, QRgb>> sorted;
		for (auto it = colours.constBegin() ; it != colours.constEnd() ; ++it) sorted << qMakePair(it.value(), it.key());
		std::sort(sorted.begin(), sorted.end(), std::greater<>());
		for (int i = 0 ; i < sorted.size() && i < 6 ; ++i)
			qInfo("%s: %d vertices", qPrintable(QColor(sorted.at(i).second).name()), sorted.at(i).first);

		const QString png = qEnvironmentVariable("QET_CABINET3D_PNG");
		if (!png.isEmpty()) {
			Solid device;
			device.kind = Solid::Kind::Device;
			device.name = QStringLiteral("-K1");
			device.min = QVector3D(0, 0, 0);
			device.max = model.max - model.min;
			device.step = step;
			Cabinet3dView view;
			view.resize(900, 700);
			view.setSolids({device});
			QVERIFY(view.grabFramebuffer().save(png));
		}
	}

	void aDeviceStandsOnItsFace()
	{
			//A push button 30 wide, 40 high, 60 deep, drawn looking +z
		CabinetStep::Model button;
		button.ok = true;
		button.min = QVector3D(0, 0, 0);
		button.max = QVector3D(30, 40, 60);
		auto placed = [&button](const CabinetEnclosure::Mounted &mounted, QVector3D &min, QVector3D &max) {
			const QMatrix4x4 place = CabinetEnclosure::mountedPlacement(mounted, button);
			min = QVector3D(1e9f, 1e9f, 1e9f);
			max = -min;
			for (int i = 0 ; i < 8 ; ++i)
			{
				const QVector3D p = place.map(QVector3D(i & 1 ? 30 : 0, i & 2 ? 40 : 0, i & 4 ? 60 : 0));
				min = QVector3D(qMin(min.x(), p.x()), qMin(min.y(), p.y()), qMin(min.z(), p.z()));
				max = QVector3D(qMax(max.x(), p.x()), qMax(max.y(), p.y()), qMax(max.z(), p.z()));
			}
		};
		CabinetEnclosure::Mounted door;
		door.at = QVector3D(100, 200, 0);
		door.normal = QVector3D(0, 0, 1);
		door.up = CabinetEnclosure::faceUp(door.normal);
		QVector3D min, max;
			//Its back on the face, in the middle of the point
		placed(door, min, max);
		QVERIFY(near(min, QVector3D(85, 180, 0)));
		QVERIFY(near(max, QVector3D(115, 220, 60)));
			//Through the sheet
		door.sink = 20;
		placed(door, min, max);
		QVERIFY(qAbs(min.z() + 20) < 0.01f && qAbs(max.z() - 40) < 0.01f);
			//A quarter turn
		door.sink = 0;
		door.quarter_turns = 1;
		placed(door, min, max);
		QVERIFY(qAbs((max - min).x() - 40) < 0.01f && qAbs((max - min).y() - 30) < 0.01f);
			//Its +x side out: 30 deep
		door.quarter_turns = 0;
		door.side = 2;
		placed(door, min, max);
		QVERIFY(qAbs((max - min).z() - 30) < 0.01f);

			//On the right side of an enclosure, looking +x, upright
		CabinetEnclosure::Mounted side;
		side.at = QVector3D(300, 500, -100);
		side.normal = QVector3D(1, 0, 0);
		side.up = CabinetEnclosure::faceUp(side.normal);
		placed(side, min, max);
		QVERIFY(qAbs(min.x() - 300) < 0.01f && qAbs(max.x() - 360) < 0.01f);
		QVERIFY(qAbs((max - min).y() - 40) < 0.01f);
			//On the roof, looking up: a beacon
		CabinetEnclosure::Mounted roof;
		roof.at = QVector3D(0, 1800, -150);
		roof.normal = QVector3D(0, 1, 0);
		roof.up = CabinetEnclosure::faceUp(roof.normal);
		QVERIFY(near(roof.up, QVector3D(0, 0, -1)));
		placed(roof, min, max);
		QVERIFY(qAbs(min.y() - 1800) < 0.01f && qAbs(max.y() - 1860) < 0.01f);

			//Kept as text
		CabinetEnclosure::Enclosure enclosure;
		CabinetEnclosure::Mounted kept = door;
		kept.symbol = QUuid::createUuid();
		kept.label = QStringLiteral("-S1");
		kept.manufacturer = QStringLiteral("Acme");
		kept.order_number = QStringLiteral("PB-22");
		kept.node = QStringLiteral("0:1:1:1/0:1:1:1:9");
		kept.quarter_turns = 3;
		kept.sink = 12.5;
		enclosure.mounted << kept;
		const CabinetEnclosure::Enclosure read =
				CabinetEnclosure::Enclosure::fromString(enclosure.toString());
		QCOMPARE(read.mounted.size(), 1);
		QVERIFY(read.mounted.first() == kept);
		QCOMPARE(read.mountedIndex(kept.symbol), 0);
	}

	void aDeviceOffThePlateIsPlacedIn3D()
	{
		if (!CabinetStep::available()) QSKIP("Built without OpenCASCADE");
			//The parts database: a push button with its STEP model
		const QString database_path = m_dir.filePath(QStringLiteral("parts3d.sqlite"));
		{
			PartsDatabase database;
			QString error;
			QVERIFY2(database.open(database_path, &error), qPrintable(error));
			Part button;
			button.manufacturer = QStringLiteral("Acme");
			button.order_number = QStringLiteral("PB-22");
			button.description = QStringLiteral("Push button");
			const qint64 id = database.addPart(button, &error);
			QVERIFY2(id > 0, qPrintable(error));
			const QByteArray step = boxStep(QVector3D(30, 40, 60));
			QVERIFY(!step.isEmpty());
			QVERIFY2(database.addFile(id, PartFileKind::Step, QStringLiteral("pb-22.stp"), step, &error) > 0,
					 qPrintable(error));
		}
		const bool was_enabled = PartsDatabase::isEnabled();
		const QString was_path = PartsDatabase::configuredPath();
		PartsDatabase::setConfiguredPath(database_path);
		PartsDatabase::setEnabled(true);

			//-S1 on the schematic, made of that part, not on the plate
		QETProject project;
		Diagram *schematic = project.addNewDiagram();
		Element *s1 = symbol(schematic, QStringLiteral("-S1"),
							 PartsLinks::encode({{QStringLiteral("Acme"), QStringLiteral("PB-22"), 1}}));
		QVERIFY(s1);
		Diagram *cabinet = build(project);
		project.dataBase()->updateDB();

		Cabinet3dWindow window(cabinet);
		window.waitForModels();
		QCOMPARE(window.unplaced().size(), 1);
		QCOMPARE(window.unplaced().first().label, QStringLiteral("-S1"));
		QCOMPARE(window.unplaced().first().symbol, s1->uuid());
		bool listed = false;
		for (int i = 0 ; i < window.navigator()->topLevelItemCount() ; ++i)
			listed = listed || window.navigator()->topLevelItem(i)->childCount() == 1
					&& window.navigator()->topLevelItem(i)->child(0)->text(0).startsWith(QLatin1String("-S1"));
		QVERIFY(listed);

			//On the front of the plate, at (400, -300)
		int plate = -1;
		for (int i = 0 ; i < window.view()->solids().size() ; ++i)
			if (window.view()->solids().at(i).kind == Solid::Kind::Plate) plate = i;
		Cabinet3dView::Pick pick;
		pick.solid = plate;
		QVERIFY(window.placeOnFace(0, pick, QVector3D(400, -300, 0), QVector3D(0, 0, 1)));
		window.waitForModels();
		QVERIFY(window.unplaced().isEmpty());
		int placed = -1;
		for (int i = 0 ; i < window.view()->solids().size() ; ++i)
			if (window.view()->solids().at(i).mounted >= 0) placed = i;
		QVERIFY(placed >= 0);
		QCOMPARE(window.view()->solids().at(placed).name, QStringLiteral("-S1"));
		QVector3D min, max;
		window.view()->drawnBox(placed, min, max);
		QVERIFY2(near(min, QVector3D(385, -320, 0)), qPrintable(QStringLiteral("%1 %2 %3")
				 .arg(min.x()).arg(min.y()).arg(min.z())));
		QVERIFY(near(max, QVector3D(415, -280, 60)));
		bool ok = false;
		window.view()->mountedWorld(placed, &ok);
		QVERIFY(ok);

			//Sunk 20 mm, one step of undo
		const int mounted = window.view()->solids().at(placed).mounted;
		QVERIFY(window.changeMounted(mounted, [](CabinetEnclosure::Mounted &each) {each.sink = 20;},
									 QStringLiteral("Sink %1")));
		window.view()->drawnBox(placed, min, max);
		QVERIFY(qAbs(min.z() + 20) < 0.05f);
		cabinet->undoStack().undo();
		window.view()->drawnBox(placed, min, max);
		QVERIFY(qAbs(min.z()) < 0.05f);

			//Written with the rest
		const QString written = m_dir.filePath(QStringLiteral("with-button.step"));
		QString error;
		QVERIFY2(window.exportStep(written, &error), qPrintable(error));
		QFile file(written);
		QVERIFY(file.open(QIODevice::ReadOnly));
		QVERIFY(file.readAll().contains("-S1"));

			//Taken out of 3D: to place again
		QVERIFY(window.changeMounted(mounted, [](CabinetEnclosure::Mounted &each) {each.symbol = QUuid();},
									 QStringLiteral("Take %1 out of 3D")));
		QCOMPARE(window.unplaced().size(), 1);
		cabinet->undoStack().undo();
		QVERIFY(window.unplaced().isEmpty());

		PartsDatabase::setConfiguredPath(was_path);
		PartsDatabase::setEnabled(was_enabled);
	}

	void aDeviceWithoutModelsIsABoxOfItsMeasures()
	{
			//A relay with its mounting measures, no DXF, no STEP
		const QString database_path = m_dir.filePath(QStringLiteral("parts-box.sqlite"));
		{
			PartsDatabase database;
			QString error;
			QVERIFY2(database.open(database_path, &error), qPrintable(error));
			Part relay;
			relay.manufacturer = QStringLiteral("Acme");
			relay.order_number = QStringLiteral("RL-1");
			relay.width = 18;
			relay.height = 90;
			relay.depth = 65;
			QVERIFY2(database.addPart(relay, &error) > 0, qPrintable(error));
			Part flat;
			flat.manufacturer = QStringLiteral("Acme");
			flat.order_number = QStringLiteral("FL-1");
			QVERIFY2(database.addPart(flat, &error) > 0, qPrintable(error));
		}
		const bool was_enabled = PartsDatabase::isEnabled();
		const QString was_path = PartsDatabase::configuredPath();
		PartsDatabase::setConfiguredPath(database_path);
		PartsDatabase::setEnabled(true);

			//On the plate: a box of 18 x 90 mm
		CabinetItem::Properties properties;
		QString error;
		QVERIFY(CabinetDevices::drawingFromParts(
					PartsLinks::encode({{QStringLiteral("Acme"), QStringLiteral("RL-1"), 1}}), properties, &error));
		QCOMPARE(properties.kind, CabinetItem::Kind::Drawing);
		QCOMPARE(properties.length, 18.0);
		QCOMPARE(properties.width, 90.0);
		QCOMPARE(properties.drawing.boundingRect(), QRectF(0, 0, 18, 90));
			//Without measures, nothing to draw
		CabinetItem::Properties none;
		QVERIFY(!CabinetDevices::drawingFromParts(
					PartsLinks::encode({{QStringLiteral("Acme"), QStringLiteral("FL-1"), 1}}), none, &error));

			//In 3D: a box of 18 x 90 x 65 mm to place (-K1 is on the plate)
		if (CabinetStep::available()) {
			QETProject project;
			Diagram *schematic = project.addNewDiagram();
			QVERIFY(symbol(schematic, QStringLiteral("-K7"),
						   PartsLinks::encode({{QStringLiteral("Acme"), QStringLiteral("RL-1"), 1}})));
			Diagram *cabinet = build(project);
			project.dataBase()->updateDB();
			const QList<Cabinet3d::Unplaced> unplaced = Cabinet3d::unplacedOf(cabinet);
			QCOMPARE(unplaced.size(), 1);
			QCOMPARE(unplaced.first().label, QStringLiteral("-K7"));
			const CabinetStep::Model box = CabinetStep::read(unplaced.first().step);
			QVERIFY(box.ok);
			const QVector3D size = box.max - box.min;
			QVERIFY2(near(size, QVector3D(18, 90, 65)), qPrintable(QStringLiteral("%1 %2 %3")
					 .arg(size.x()).arg(size.y()).arg(size.z())));
		}

		PartsDatabase::setConfiguredPath(was_path);
		PartsDatabase::setEnabled(was_enabled);
	}

	void theWindowShowsTheFolio()
	{
		QETProject project;
		Diagram *cabinet = build(project);
		Cabinet3dWindow window(cabinet);
		QCOMPARE(window.view()->solids().size(), 4);
		Cabinet3dView::Pick pick;
		pick.solid = 3;
		window.view()->select(pick);
		QCOMPARE(window.view()->selected().solid, 3);
			//The navigator: plate, ducts, rails, devices
		QCOMPARE(window.navigator()->topLevelItemCount(), 4);
	}

	void aDxfIsPlacedOutOfTheRedZones()
	{
			//While a DXF is placed (Brandon, 2026-11-25), the free space
			//the part of each device asks is drawn in red, read from the
			//parts database; the part placed shows its own
		const QString database_path = m_dir.filePath(QStringLiteral("zones.sqlite"));
		{
			PartsDatabase database;
			QString error;
			QVERIFY2(database.open(database_path, &error), qPrintable(error));
			Part supply;
			supply.manufacturer = QStringLiteral("Acme");
			supply.order_number = QStringLiteral("PS-10");
			supply.clearance_top = 30;
			supply.clearance_bottom = 20;
			supply.clearance_right = 10;
			QVERIFY2(database.addPart(supply, &error) > 0, qPrintable(error));
		}
		const bool was_enabled = PartsDatabase::isEnabled();
		const QString was_path = PartsDatabase::configuredPath();
		PartsDatabase::setConfiguredPath(database_path);
		PartsDatabase::setEnabled(true);

		QETProject project;
		Diagram *schematic = project.addNewDiagram();
		QVERIFY(symbol(schematic, QStringLiteral("-K1"),
					   PartsLinks::encode({{QStringLiteral("Acme"), QStringLiteral("PS-10"), 1}})));
		Diagram *cabinet = build(project);
		CabinetItem *k1 = nullptr;
		for (QGraphicsItem *item : cabinet->items()) {
			auto *found = qgraphicsitem_cast<CabinetItem *>(item);
			if (found && found->properties().kind == CabinetItem::Kind::Drawing) k1 = found;
		}
		QVERIFY(k1);
		const QRectF before = k1->boundingRect();
		QVERIFY(!CabinetItem::placingShown());

		CabinetItem::Properties placed;
		placed.kind = CabinetItem::Kind::Drawing;
		placed.length = 20;
		placed.width = 20;
		placed.device_label = QStringLiteral("-K1");
		QPainterPath drawing;
		drawing.addRect(0, 0, 20, 20);
		placed.drawing = drawing;
		auto *tool = new DiagramEventAddCabinetItem(cabinet, placed);
		QVERIFY(CabinetItem::placingShown());
		QCOMPARE(k1->clearance(), QMarginsF(0, 30, 10, 20));
		QVERIFY(k1->boundingRect().height() > before.height());
		QVERIFY(tool->preview());
		QCOMPARE(tool->preview()->clearance(), QMarginsF(0, 30, 10, 20));
		QCOMPARE(CabinetClearances::of(k1), QMarginsF(0, 30, 10, 20));
		delete tool;
		QVERIFY(!CabinetItem::placingShown());
		QCOMPARE(k1->boundingRect(), before);

		PartsDatabase::setConfiguredPath(was_path);
		PartsDatabase::setEnabled(was_enabled);
	}

	void theHeatOfTheCabinetIsWorkedOut()
	{
			//What the walls give out (Brandon, 2026-11-25): 600 x 800 x
			//300 mm against a wall, 1.4 W (H + D) + 1.8 D H
		using namespace CabinetThermal;
		QVERIFY(qAbs(effectiveSurface(600, 800, 300, Placement::AgainstWall) - 1.356) < 1e-9);
		QVERIFY(qAbs(effectiveSurface(600, 800, 300, Placement::FreeStanding)
					 - (1.8 * 0.8 * 0.9 + 1.4 * 0.6 * 0.3)) < 1e-9);
		Inputs inputs;
		inputs.width = 600;
		inputs.height = 800;
		inputs.depth = 300;
		inputs.placement = Placement::AgainstWall;
		inputs.material = Material::PaintedSteel;
		inputs.ambient = 35;
		inputs.maximum = 40;
		Device known;
		known.label = QStringLiteral("-K1");
		known.watts = 100;
		known.known = true;
		Device unknown;
		unknown.label = QStringLiteral("-Q2");
		Result result = compute(inputs, {known, unknown});
		QCOMPARE(result.losses, 100.0);
		const double gives_out = 5.5 * 1.356;
		QVERIFY(qAbs(result.inside - (35 + 100 / gives_out)) < 1e-9);
		QVERIFY(result.needsCooling());
		QVERIFY(qAbs(result.cooling - (100 - gives_out * 5)) < 1e-9);
		QVERIFY(qAbs(result.airflow - 3.1 * result.cooling / 5) < 1e-9);
		QCOMPARE(result.missing, QStringList{QStringLiteral("-Q2")});
			//Hotter outside than allowed inside: no fan helps
		inputs.ambient = 45;
		result = compute(inputs, {known});
		QVERIFY(qAbs(result.cooling - (100 + gives_out * 5)) < 1e-9);
		QCOMPARE(result.airflow, 0.0);
		QVERIFY(result.missing.isEmpty());
			//Cool enough
		inputs.ambient = 20;
		QVERIFY(!compute(inputs, {known}).needsCooling());

			//The devices of a cabinet folio, from the parts database
		const QString database_path = m_dir.filePath(QStringLiteral("heat.sqlite"));
		{
			PartsDatabase database;
			QString error;
			QVERIFY2(database.open(database_path, &error), qPrintable(error));
			Part supply;
			supply.manufacturer = QStringLiteral("Acme");
			supply.order_number = QStringLiteral("PS-20");
			supply.power_loss = 12.5;
			QVERIFY2(database.addPart(supply, &error) > 0, qPrintable(error));
		}
		const bool was_enabled = PartsDatabase::isEnabled();
		const QString was_path = PartsDatabase::configuredPath();
		PartsDatabase::setConfiguredPath(database_path);
		PartsDatabase::setEnabled(true);
		QETProject project;
		Diagram *schematic = project.addNewDiagram();
		QVERIFY(symbol(schematic, QStringLiteral("-K1"),
					   PartsLinks::encode({{QStringLiteral("Acme"), QStringLiteral("PS-20"), 2}})));
		Diagram *cabinet = build(project);
		part(cabinet, CabinetItem::Kind::Drawing, 20, 20, {300, 100}, QStringLiteral("-Q2"));
		const QList<Device> devices = devicesOf(cabinet);
		QCOMPARE(devices.size(), 2);
		QCOMPARE(devices.at(0).label, QStringLiteral("-K1"));
		QCOMPARE(devices.at(0).watts, 25.0);
		QVERIFY(devices.at(0).known);
		QCOMPARE(devices.at(1).label, QStringLiteral("-Q2"));
		QVERIFY(!devices.at(1).known);

		CabinetThermalDialog dialog(cabinet);
		QCOMPARE(dialog.result().losses, 25.0);
		QVERIFY(dialog.warningText().contains(QStringLiteral("-Q2")));
		QVERIFY(!dialog.warningText().contains(QStringLiteral("-K1")));
			//The heat map: hotter than outside where -K1 stands
		auto *map = dialog.findChild<CabinetHeatMap *>(QStringLiteral("thermal_map"));
		QVERIFY(map);
		QVERIFY(map->hottest() > dialog.inputs().ambient);

		PartsDatabase::setConfiguredPath(was_path);
		PartsDatabase::setEnabled(was_enabled);
	}

	void theRoomIsChecked()
	{
			//Room enough (Brandon, 2026-11-21): -Q1 asks for 30 mm free
			//above it and has a duct 10 mm over it, and is too deep for
			//the door; -K1 and -K2 are into each other
		using CabinetSpace::Body;
		using CabinetSpace::Problem;
		Body q1;
		q1.solid = 0;
		q1.name = QStringLiteral("-Q1");
		q1.min = QVector3D(100, -190, 7.5f);
		q1.max = QVector3D(118, -100, 77.5f);
		q1.clearance.top = 30;
		Body duct;
		duct.solid = 1;
		duct.kind = Solid::Kind::Duct;
		duct.name = QStringLiteral("40x60");
		duct.min = QVector3D(0, -90, 0);
		duct.max = QVector3D(600, -50, 60);
		Body k1;
		k1.solid = 2;
		k1.name = QStringLiteral("-K1");
		k1.min = QVector3D(200, -190, 7.5f);
		k1.max = QVector3D(245, -100, 67.5f);
		Body k2 = k1;
		k2.solid = 3;
		k2.name = QStringLiteral("-K2");
		k2.min = QVector3D(230, -180, 7.5f);
		k2.max = QVector3D(275, -110, 67.5f);
		const QList<Body> bodies{q1, duct, k1, k2};
		const QList<Problem> problems = CabinetSpace::check(
					bodies, true, QVector3D(0, -400, -10), QVector3D(600, 0, 70));
		QCOMPARE(problems.size(), 3);
		QVERIFY(problems.at(0).kind == Problem::Kind::Collision);
		QCOMPARE(problems.at(0).first, 2);
		QCOMPARE(problems.at(0).second, 3);
		QCOMPARE(problems.at(0).missing, 15.0f);
		QVERIFY(problems.at(1).kind == Problem::Kind::Enclosure);
		QVERIFY(problems.at(1).side == CabinetSpace::Front);
		QCOMPARE(problems.at(1).missing, 7.5f);
		QVERIFY(problems.at(2).kind == Problem::Kind::Clearance);
		QCOMPARE(problems.at(2).first, 0);
		QCOMPARE(problems.at(2).second, 1);
		QVERIFY(problems.at(2).side == CabinetSpace::Top);
		QCOMPARE(problems.at(2).asked, 30.0f);
		QCOMPARE(problems.at(2).missing, 20.0f);
		QVERIFY(CabinetSpace::describe(problems.at(2), bodies).contains(QStringLiteral("-Q1")));

			//The space asked above -Q1, none below
		QVector3D min, max;
		QVERIFY(CabinetSpace::zone(q1, CabinetSpace::Top, min, max));
		QCOMPARE(max.y(), -70.0f);
		QVERIFY(!CabinetSpace::zone(q1, CabinetSpace::Bottom, min, max));

			//A face met by a ray, and missed
		QVector<float> plate;
		CabinetStep::appendBox(plate, QVector3D(-10, -10, 50), QVector3D(10, 10, 52), Qt::gray);
		const int count = int(plate.size() / CabinetStep::vertex_size);
		QCOMPARE(CabinetSpace::hit(plate, 0, count, QMatrix4x4(), QVector3D(), QVector3D(0, 0, 1)), 50.0f);
		QCOMPARE(CabinetSpace::hit(plate, 0, count, QMatrix4x4(), QVector3D(), QVector3D(1, 0, 0)), -1.0f);
	}

	void theWindowChecksTheRoom()
	{
			//-K2 drawn over -K1, both too deep for a box 70 mm deep
		QETProject project;
		Diagram *cabinet = build(project);
		part(cabinet, CabinetItem::Kind::Drawing, 45, 90, {120, 110}, QStringLiteral("-K2"));
		Cabinet3dWindow window(cabinet);
		CabinetEnclosure::Enclosure enclosure;
		enclosure.source = CabinetEnclosure::Enclosure::Source::Manual;
		enclosure.width = 700;
		enclosure.height = 500;
		enclosure.depth = 70;
		enclosure.plate_width = 600;
		enclosure.plate_height = 400;
		enclosure.plate = QStringLiteral("plate");
		QVERIFY(window.setEnclosure(enclosure));
		window.waitForModels();
		QVector3D min, max;
		QVERIFY(window.view()->inside(min, max));
			//Its open front: the depth less the sheet and the plate
		QVERIFY2(qAbs(max.z() - 62) < 0.5f, qPrintable(QString::number(max.z())));
		QVERIFY(min.x() < 0 && max.x() > 600);

		window.checkRoom();
		bool collision = false;
		bool door = false;
		for (const CabinetSpace::Problem &problem : window.problemList())
		{
			collision = collision || problem.kind == CabinetSpace::Problem::Kind::Collision;
			door = door || (problem.kind == CabinetSpace::Problem::Kind::Enclosure
							&& problem.side == CabinetSpace::Front);
		}
		QVERIFY(collision);
		QVERIFY(door);
		QVERIFY(window.problems()->topLevelItemCount() >= 2);
			//Picked in the list: the parts picked on the sheet too
		window.showProblem(0);
		QVERIFY(!cabinet->selectedItems().isEmpty());
	}

	void anEnclosureIsKeptAsText()
	{
		CabinetEnclosure::Enclosure enclosure;
		enclosure.source = CabinetEnclosure::Enclosure::Source::Parts;
		enclosure.manufacturer = QStringLiteral("Maker");
		enclosure.order_number = QStringLiteral("BOX-600");
		enclosure.plate = QStringLiteral("0:1:1:1/0:1:1:1:28");
		enclosure.quarter_turns = 1;
		enclosure.hidden = {QStringLiteral("node:0:1:1:1/0:1:1:1:7"), QStringLiteral("item:abc")};
		const CabinetEnclosure::Enclosure read =
				CabinetEnclosure::Enclosure::fromString(enclosure.toString());
		QVERIFY(read == enclosure);
		QVERIFY(read.placed());
		QCOMPARE(read.title(), QStringLiteral("Maker BOX-600"));
		QVERIFY(CabinetEnclosure::Enclosure::fromString(QString()).source
				== CabinetEnclosure::Enclosure::Source::None);
		QVERIFY(CabinetEnclosure::Enclosure().toString().isEmpty());
	}

	void anEnclosureByMeasuresHoldsThePlate()
	{
		CabinetEnclosure::Enclosure enclosure;
		enclosure.source = CabinetEnclosure::Enclosure::Source::Manual;
		enclosure.width = 600;
		enclosure.height = 800;
		enclosure.depth = 300;
		enclosure.plate_width = 500;
		enclosure.plate_height = 700;
		enclosure.plate = QStringLiteral("plate");
		const CabinetStep::Model model = CabinetEnclosure::manualModel(enclosure);
		QVERIFY(model.ok);
		QCOMPARE(model.nodes.size(), 7);
		const int plate = model.node(QStringLiteral("plate"));
		QVERIFY(plate > 0);

			//The folio's plate, 500 x 700, its front at z = 0
		Solid ours;
		ours.kind = Solid::Kind::Plate;
		ours.min = QVector3D(0, -700, -2);
		ours.max = QVector3D(500, 0, 0);
		bool ok = false;
		const QMatrix4x4 place = CabinetEnclosure::placement(model, enclosure, ours, &ok);
		QVERIFY(ok);
		const QVector3D front_min = place.map(model.nodes.at(plate).min);
		const QVector3D front_max = place.map(model.nodes.at(plate).max);
		QVERIFY(near(QVector3D(qMin(front_min.x(), front_max.x()), qMin(front_min.y(), front_max.y()),
							   qMax(front_min.z(), front_max.z())), QVector3D(0, -700, 0)));
		QVERIFY(near(QVector3D(qMax(front_min.x(), front_max.x()), qMax(front_min.y(), front_max.y()), 0),
					 QVector3D(500, 0, 0)));
		QCOMPARE(CabinetEnclosure::plateSize(model, enclosure, ours), QSizeF(500, 700));
			//The back of the box right behind the plate (sheet and plate),
			//its open front towards us, the depth of the box away
		QVERIFY(qAbs(place.map(model.min).z() + 8) < 0.05f);
		QVERIFY(qAbs(place.map(model.max).z() - 292) < 0.05f);
	}

	void theEnclosurePickerShowsThoseWithStep()
	{
		PartsDatabase database;
		QString error;
		QVERIFY2(database.open(m_dir.filePath(QStringLiteral("picker-step.sqlite")), &error), qPrintable(error));
		Part with_model, without_model;
		with_model.manufacturer = without_model.manufacturer = QStringLiteral("Acme");
		with_model.order_number = QStringLiteral("AE-1");
		without_model.order_number = QStringLiteral("AE-2");
		const qint64 id = database.addPart(with_model, &error);
		QVERIFY2(id > 0 && database.addPart(without_model, &error) > 0, qPrintable(error));
		QVERIFY(database.addFile(id, PartFileKind::Step, QStringLiteral("ae-1.stp"), "ISO-10303-21;", &error) > 0);
		QCOMPARE(database.partsWithFile(PartFileKind::Step), QSet<qint64>{id});
		QSettings().remove(QStringLiteral("parts-database/picker-category-enclosure"));
		QSettings().remove(QStringLiteral("parts-database/picker-category-enclosure-with-step"));

			//Not offered elsewhere: every part, no checkbox
		PartPickerDialog plain(&database, nullptr, QStringLiteral("enclosure"));
		auto *plain_table = plain.findChild<QTableWidget *>();
		QCOMPARE(plain_table->rowCount(), 2);
		QVERIFY(plain.findChild<QCheckBox *>(QStringLiteral("picker_with_step"))->isHidden());

			//For the plate: on by default, only the one with STEP
		PartPickerDialog picker(&database, nullptr, QStringLiteral("enclosure"));
		picker.offerOnlyWithStep();
		auto *box = picker.findChild<QCheckBox *>(QStringLiteral("picker_with_step"));
		auto *table = picker.findChild<QTableWidget *>();
		QVERIFY(!box->isHidden());
		QVERIFY(box->isChecked());
		QCOMPARE(table->rowCount(), 1);
		QCOMPARE(table->item(0, 1)->text(), QStringLiteral("AE-1"));
		box->setChecked(false);
		QCOMPARE(table->rowCount(), 2);
		box->setChecked(true);
		QCOMPARE(table->rowCount(), 1);
	}

	void thePlateIsClickedInTheEnclosure()
	{
			//A plate of an enclosure of the parts database (Brandon,
			//2026-11-25): once on the sheet, the 3D asks for its plate,
			//and a click on it gives the plate of the sheet its size
		if (!CabinetStep::available()) QSKIP("Built without OpenCASCADE");
		Solid back;
		back.kind = Solid::Kind::Plate;
		back.name = QStringLiteral("Back wall");
		back.min = QVector3D(-300, 280, 0);
		back.max = QVector3D(300, 300, 1000);
		back.color = Qt::lightGray;
		Solid plate = back;
		plate.name = QStringLiteral("Plate");
		plate.min = QVector3D(-250, 250, 50);
		plate.max = QVector3D(250, 253, 950);
		const QString path = m_dir.filePath(QStringLiteral("clicked.step"));
		QVERIFY(CabinetStep::write({back, plate}, path));

		QETProject project;
		Diagram *cabinet = build(project);
		Cabinet3dWindow window(cabinet);
		CabinetEnclosure::Enclosure enclosure;
		enclosure.source = CabinetEnclosure::Enclosure::Source::File;
		enclosure.path = path;
		QVERIFY(window.setEnclosure(enclosure));
		window.waitForModels();
		const CabinetStep::Model &model = window.view()->enclosureModel();
		QVERIFY2(model.ok, qPrintable(model.error));
		int node = -1;
		for (int i = 0 ; i < model.nodes.size() ; ++i)
			if (model.nodes.at(i).name == QLatin1String("Plate")) node = i;
		QVERIFY(node >= 0);

		window.askForPlate();
		QVERIFY(window.isAskingForPlate());
		Cabinet3dView::Pick pick;
		pick.node = node;
		window.view()->select(pick);
		QVERIFY(!window.isAskingForPlate());
		QVERIFY(window.view()->enclosurePlaced());
		CabinetItem *ours = nullptr;
		for (QGraphicsItem *item : cabinet->items()) {
			auto *found = qgraphicsitem_cast<CabinetItem *>(item);
			if (found && found->kind() == CabinetItem::Kind::MountingPlate) ours = found;
		}
		QVERIFY(ours);
		QVERIFY(qAbs(ours->properties().length - 500) < 0.5);
		QVERIFY(qAbs(ours->properties().width - 900) < 0.5);
	}

	void thePlateOfAStepEnclosureIsTaken()
	{
		if (!CabinetStep::available()) QSKIP("Built without OpenCASCADE");
			//An enclosure drawn z up, open towards -y: a back wall at y
			//280..300, a side as deep as the enclosure, and a plate of
			//500 x 900 at y 250..253
		Solid side;
		side.kind = Solid::Kind::Plate;
		side.name = QStringLiteral("Side");
		side.min = QVector3D(-300, 0, 0);
		side.max = QVector3D(-295, 300, 1000);
		side.color = Qt::lightGray;
		Solid back;
		back.kind = Solid::Kind::Plate;
		back.name = QStringLiteral("Back wall");
		back.min = QVector3D(-300, 280, 0);
		back.max = QVector3D(300, 300, 1000);
		back.color = Qt::lightGray;
		Solid plate = back;
		plate.name = QStringLiteral("Plate");
		plate.min = QVector3D(-250, 250, 50);
		plate.max = QVector3D(250, 253, 950);
		plate.color = Qt::gray;
		const QString path = m_dir.filePath(QStringLiteral("enclosure.step"));
		QVERIFY(CabinetStep::write({side, back, plate}, path));

		QETProject project;
		Diagram *cabinet = build(project);
		Cabinet3dWindow window(cabinet);
		CabinetEnclosure::Enclosure enclosure;
		enclosure.source = CabinetEnclosure::Enclosure::Source::File;
		enclosure.path = path;
		QVERIFY(window.setEnclosure(enclosure));
		window.waitForModels();
		const CabinetStep::Model &model = window.view()->enclosureModel();
		QVERIFY2(model.ok, qPrintable(model.error));
		QVERIFY(!window.view()->enclosurePlaced());
		QString id;
		for (const CabinetStep::Node &node : model.nodes)
		{
			if (node.name == QLatin1String("Plate")) id = node.id;
		}
		QVERIFY2(!id.isEmpty(), "the plate is named in the navigator");
			//The navigator shows the enclosure too
		QCOMPARE(window.navigator()->topLevelItemCount(), 5);

			//Taken, the folio's plate given its size: 500 x 900
		QVERIFY(window.usePlate(id, true));
		QVERIFY(window.view()->enclosurePlaced());
		CabinetItem *ours = nullptr;
		for (QGraphicsItem *item : cabinet->items())
		{
			auto *part = qgraphicsitem_cast<CabinetItem *>(item);
			if (part && part->kind() == CabinetItem::Kind::MountingPlate) ours = part;
		}
		QVERIFY(ours);
		QVERIFY(qAbs(ours->properties().length - 500) < 0.5);
		QVERIFY(qAbs(ours->properties().width - 900) < 0.5);
		const int our_plate = window.view()->enclosurePlate();
		QVERIFY(!window.view()->isShown(our_plate));
			//Its front where ours is: z 0, standing up
		QVector3D min, max;
		window.view()->nodeBox(model.node(id), min, max);
		QVERIFY(qAbs(max.z()) < 0.05f);
		QVERIFY(qAbs((max - min).y() - 900) < 0.5f);
		const Solid &plate_solid = window.view()->solids().at(our_plate);
		QVERIFY(near((min + max) / 2 * QVector3D(1, 1, 0), plate_solid.center() * QVector3D(1, 1, 0)));

			//The duct hidden, kept in the plate, one step of undo
		QString duct;
		for (const Solid &solid : window.view()->solids())
		{
			if (solid.kind == Solid::Kind::Duct) duct = Cabinet3dView::key(solid);
		}
		window.setHiddenKey(duct, true);
		QVERIFY(ours->properties().enclosure.contains(duct.mid(5)));
		QVERIFY(window.view()->hidden().contains(duct));
		cabinet->undoStack().undo();
		QVERIFY(!window.view()->hidden().contains(duct));

			//Written with its enclosure
		const QString written = m_dir.filePath(QStringLiteral("with-enclosure.step"));
		QString error;
		QVERIFY2(window.exportStep(written, &error), qPrintable(error));
		QFile file(written);
		QVERIFY(file.open(QIODevice::ReadOnly));
		QVERIFY(file.readAll().contains("Plate"));

			//Taking the plate undone: beside the folio's plate again
		cabinet->undoStack().undo();
		QVERIFY(!window.view()->enclosurePlaced());
		QVERIFY(window.view()->isShown(our_plate));
	}
};

QET_APP_TEST_MAIN(tst_cabinet3d)
#include "tst_cabinet3d.moc"
