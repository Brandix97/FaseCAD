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
	DxfReader: the lines of a DXF drawing, in millimetres, the y axis
	down, the top left corner at 0.

	* lines, circles, arcs, polylines with their arcs (bulges), ellipses,
	  splines and solids are drawn, texts are counted and left out;
	* the unit of the file ($INSUNITS) is turned into millimetres;
	* a block is drawn where an INSERT puts it, turned and scaled;
	* a binary DXF or a file that is no DXF says why it is not read;
	* the path is written to a project file and read back the same;
	* the drawing becomes a part of the cabinet, saved with its folio;
	* the DXF files of devices of a manufacturer are read whole, at the size
	  of a device (folder given by FASECAD_DXF_SAMPLES, skipped if not
	  there; FASECAD_DXF_SHOTS=<folder> saves a picture of each).
*/

#include "cabinet/cabinetitem.h"
#include "cabinet/dxfreader.h"
#include "diagram.h"
#include "qetproject.h"

#include <QDir>
#include <QDomDocument>
#include <QImage>
#include <QPainter>
#include <QtTest>

#include <cmath>

class TstDxfReader : public QObject
{
		Q_OBJECT

	private slots:
		void linesInMillimetres();
		void circleAndArc();
		void polylineBulge();
		void unitsOfTheFile();
		void blockInserted();
		void ellipseAndSpline();
		void textsAreLeftOut();
		void notADxf();
		void pathRoundTrip();
		void drawingIsAPartOfTheCabinet();
		void railIsNotDrawnUnderADevice();
		void manufacturerFiles();

	private:
		static QByteArray dxf(const QStringList &entities,
							  const QStringList &header = {},
							  const QStringList &blocks = {});
		static bool near(const QRectF &a, const QRectF &b, double tolerance = 1e-3);
};

	/// A DXF file: @p entities, each a list of "code\nvalue" pairs
QByteArray TstDxfReader::dxf(const QStringList &entities, const QStringList &header,
							 const QStringList &blocks)
{
	QStringList lines;
	lines << QStringLiteral("0\nSECTION\n2\nHEADER");
	lines << header;
	lines << QStringLiteral("0\nENDSEC");
	if (!blocks.isEmpty()) {
		lines << QStringLiteral("0\nSECTION\n2\nBLOCKS");
		lines << blocks;
		lines << QStringLiteral("0\nENDSEC");
	}
	lines << QStringLiteral("0\nSECTION\n2\nENTITIES");
	lines << entities;
	lines << QStringLiteral("0\nENDSEC\n0\nEOF");
		//As AutoCAD writes them: CR LF, codes right aligned
	return lines.join(QLatin1Char('\n')).replace(QLatin1Char('\n'), QStringLiteral("\r\n")).toLatin1();
}

bool TstDxfReader::near(const QRectF &a, const QRectF &b, double tolerance)
{
	return std::abs(a.left() - b.left()) < tolerance
			&& std::abs(a.top() - b.top()) < tolerance
			&& std::abs(a.width() - b.width()) < tolerance
			&& std::abs(a.height() - b.height()) < tolerance;
}

void TstDxfReader::linesInMillimetres()
{
		//An L: from (10, 50) down to (10, 20), then right to (40, 20)
	const DxfReader::Result result = DxfReader::read(dxf({
		QStringLiteral("  0\nLINE\n  8\n0\n 10\n10\n 20\n50\n 11\n10\n 21\n20"),
		QStringLiteral("  0\nLINE\n  8\n0\n 10\n10\n 20\n20\n 11\n40\n 21\n20")},
		{QStringLiteral("  9\n$INSUNITS\n 70\n4")}));
	QVERIFY2(result.ok, qPrintable(result.error));
	QCOMPARE(result.drawn, 2);
	QCOMPARE(result.unit, 1.0);
	QVERIFY(near(result.path.boundingRect(), QRectF(0, 0, 30, 30)));
		//The y axis points down: the top of the L is at y = 0
	QVERIFY(result.path.contains(QPointF(0, 0)) || result.path.elementAt(0).y == 0.0);
	QCOMPARE(result.path.elementAt(0).x, 0.0);
	QCOMPARE(result.path.elementAt(0).y, 0.0);
	QCOMPARE(result.path.elementAt(1).y, 30.0);
}

void TstDxfReader::circleAndArc()
{
		//A circle of radius 5 at (0, 0), a half arc of radius 5 at (20, 0)
		//over the top (0 to 180 degrees)
	const DxfReader::Result result = DxfReader::read(dxf({
		QStringLiteral("0\nCIRCLE\n10\n0\n20\n0\n40\n5"),
		QStringLiteral("0\nARC\n10\n20\n20\n0\n40\n5\n50\n0\n51\n180")}));
	QVERIFY2(result.ok, qPrintable(result.error));
	QCOMPARE(result.drawn, 2);
		//From x = -5 to 25, from the top of both (y = 5) to the bottom of
		//the circle (y = -5): the arc stays above its centre
	QVERIFY(near(result.path.boundingRect(), QRectF(0, 0, 30, 10), 0.01));
	const QPointF arc_top(25, 0);   //(20, 5) in the file
	bool found = false;
	for (int i = 0 ; i < result.path.elementCount() ; ++i) {
		const QPointF p(result.path.elementAt(i).x, result.path.elementAt(i).y);
		if (std::hypot(p.x() - arc_top.x(), p.y() - arc_top.y()) < 0.05) found = true;
	}
	QVERIFY(found);
}

void TstDxfReader::polylineBulge()
{
		//A closed polyline: a 20 x 10 rectangle whose right side is a half
		//circle bulging out (bulge 1 = half circle, counter-clockwise)
	const DxfReader::Result result = DxfReader::read(dxf({
		QStringLiteral("0\nLWPOLYLINE\n90\n4\n70\n1\n"
					   "10\n0\n20\n0\n"
					   "10\n20\n20\n0\n42\n1\n"
					   "10\n20\n20\n10\n"
					   "10\n0\n20\n10")}));
	QVERIFY2(result.ok, qPrintable(result.error));
		//The half circle of radius 5 reaches x = 25
	QVERIFY(near(result.path.boundingRect(), QRectF(0, 0, 25, 10), 0.01));
}

void TstDxfReader::unitsOfTheFile()
{
	const QStringList line{QStringLiteral("0\nLINE\n10\n0\n20\n0\n11\n2\n21\n1")};
	DxfReader::Result inches = DxfReader::read(dxf(line, {QStringLiteral("9\n$INSUNITS\n70\n1")}));
	QVERIFY(inches.ok);
	QVERIFY(near(inches.path.boundingRect(), QRectF(0, 0, 50.8, 25.4)));
	DxfReader::Result metres = DxfReader::read(dxf(line, {QStringLiteral("9\n$INSUNITS\n70\n6")}));
	QVERIFY(near(metres.path.boundingRect(), QRectF(0, 0, 2000, 1000)));
		//No unit: millimetres
	DxfReader::Result none = DxfReader::read(dxf(line));
	QVERIFY(near(none.path.boundingRect(), QRectF(0, 0, 2, 1)));
}

void TstDxfReader::blockInserted()
{
		//A block: a 10 x 2 bar from its base point (0, 0). Inserted at
		//(100, 100), turned 90 degrees and twice as large: 4 x 20
	const QStringList blocks{
		QStringLiteral("0\nBLOCK\n2\nBAR\n70\n0\n10\n0\n20\n0\n30\n0\n3\nBAR"),
		QStringLiteral("0\nLWPOLYLINE\n90\n4\n70\n1\n10\n0\n20\n0\n10\n10\n20\n0\n10\n10\n20\n2\n10\n0\n20\n2"),
		QStringLiteral("0\nENDBLK")};
	const DxfReader::Result result = DxfReader::read(dxf({
		QStringLiteral("0\nINSERT\n2\nBAR\n10\n100\n20\n100\n41\n2\n42\n2\n50\n90"),
		QStringLiteral("0\nLINE\n10\n90\n20\n90\n11\n91\n21\n90")},
		{}, blocks));
	QVERIFY2(result.ok, qPrintable(result.error));
	QCOMPARE(result.drawn, 2);
		//The bar goes from x = 96 to 100 and y = 100 to 120; the line at
		//(90..91, 90): together 10 x 30
	QVERIFY(near(result.path.boundingRect(), QRectF(0, 0, 10, 30)));
}

void TstDxfReader::ellipseAndSpline()
{
		//An ellipse 20 x 10 at (0, 0); a spline of degree 2 from (30, 0)
		//to (40, 0) pulled up by (35, 10): its highest point is y = 5
	const DxfReader::Result result = DxfReader::read(dxf({
		QStringLiteral("0\nELLIPSE\n10\n0\n20\n0\n11\n10\n21\n0\n40\n0.5\n41\n0\n42\n6.283185307"),
		QStringLiteral("0\nSPLINE\n70\n8\n71\n2\n72\n6\n73\n3\n74\n0\n"
					   "40\n0\n40\n0\n40\n0\n40\n1\n40\n1\n40\n1\n"
					   "10\n30\n20\n0\n10\n35\n20\n10\n10\n40\n20\n0")}));
	QVERIFY2(result.ok, qPrintable(result.error));
	QCOMPARE(result.drawn, 2);
	QVERIFY(near(result.path.boundingRect(), QRectF(0, 0, 50, 10), 0.01));
		//The top of the spline: (35, 5) in the file, 5 mm under the top
	const QRectF spline_part = result.path.boundingRect();
	Q_UNUSED(spline_part)
	bool found = false;
	for (int i = 0 ; i < result.path.elementCount() ; ++i) {
		const QPointF p(result.path.elementAt(i).x, result.path.elementAt(i).y);
		if (std::hypot(p.x() - 45, p.y() - 0) < 0.01) found = true;   //ellipse top is y=0 too
		if (std::hypot(p.x() - 45, p.y() - 5) < 0.01) found = true;
	}
	QVERIFY(found);
}

void TstDxfReader::textsAreLeftOut()
{
	const DxfReader::Result result = DxfReader::read(dxf({
		QStringLiteral("0\nLINE\n10\n0\n20\n0\n11\n10\n21\n0"),
		QStringLiteral("0\nTEXT\n10\n0\n20\n0\n40\n2.5\n1\nK1"),
		QStringLiteral("0\nMTEXT\n10\n0\n20\n0\n40\n2.5\n1\nhello"),
		QStringLiteral("0\nTEXT\n10\n0\n20\n0\n40\n2.5\n1\nK2")}));
	QVERIFY(result.ok);
	QCOMPARE(result.drawn, 1);
	QCOMPARE(result.skipped.value(QStringLiteral("TEXT")), 2);
	QCOMPARE(result.skipped.value(QStringLiteral("MTEXT")), 1);

		//Only texts: nothing to draw
	const DxfReader::Result texts = DxfReader::read(dxf({
		QStringLiteral("0\nTEXT\n10\n0\n20\n0\n40\n2.5\n1\nK1")}));
	QVERIFY(!texts.ok);
	QVERIFY(!texts.error.isEmpty());
}

void TstDxfReader::notADxf()
{
	DxfReader::Result binary = DxfReader::read(QByteArray("AutoCAD Binary DXF\r\n\x1a\x00", 22));
	QVERIFY(!binary.ok);
	QVERIFY(binary.error.contains(QStringLiteral("binary")));
	DxfReader::Result other = DxfReader::read(QByteArray("<svg></svg>\n"));
	QVERIFY(!other.ok);
	QVERIFY(!other.error.isEmpty());
	DxfReader::Result missing = DxfReader::readFile(QStringLiteral("does/not/exist.dxf"));
	QVERIFY(!missing.ok);
}

void TstDxfReader::pathRoundTrip()
{
	QPainterPath path;
	path.moveTo(0, 0);
	path.lineTo(12.5, 0);
	path.cubicTo(13, 1, 14.25, 2.125, 15, 7);
	path.addEllipse(QPointF(5, 5), 2, 2);
	bool ok = false;
	const QPainterPath read = DxfReader::pathFromString(DxfReader::pathToString(path), &ok);
	QVERIFY(ok);
		//The same elements, to the ten thousandth of a millimetre kept
	QCOMPARE(read.elementCount(), path.elementCount());
	for (int i = 0 ; i < path.elementCount() ; ++i) {
		QCOMPARE(read.elementAt(i).type, path.elementAt(i).type);
		QVERIFY(std::abs(read.elementAt(i).x - path.elementAt(i).x) < 1e-4);
		QVERIFY(std::abs(read.elementAt(i).y - path.elementAt(i).y) < 1e-4);
	}

	DxfReader::pathFromString(QStringLiteral("M 0 0 L 1"), &ok);
	QVERIFY(!ok);
	DxfReader::pathFromString(QStringLiteral("M 0 0 Z"), &ok);
	QVERIFY(!ok);
}

void TstDxfReader::drawingIsAPartOfTheCabinet()
{
	const DxfReader::Result result = DxfReader::read(dxf({
		QStringLiteral("0\nLWPOLYLINE\n90\n4\n70\n1\n10\n0\n20\n0\n10\n18\n20\n0\n10\n18\n20\n90\n10\n0\n20\n90"),
		QStringLiteral("0\nCIRCLE\n10\n9\n20\n70\n40\n4")}));
	QVERIFY(result.ok);

	QETProject project;
	Diagram *cabinet = project.addNewCabinetDiagram();
	CabinetItem::Properties properties;
	properties.kind = CabinetItem::Kind::Drawing;
	properties.drawing = result.path;
	properties.length = result.path.boundingRect().width();
	properties.width = result.path.boundingRect().height();
	properties.source = QStringLiteral("MCB 1P");
	auto *part = new CabinetItem(properties);
	cabinet->addItem(part);
	part->placeAt(QPointF(40, 40));

		//18 x 90 mm at 1:5
	QCOMPARE(part->rect().size(), QSizeF(7.2, 36));
	QCOMPARE(part->description(), QStringLiteral("MCB 1P · 18 × 90 mm"));
	QVERIFY(!part->isLinear());

	QDomDocument document = cabinet->toXml();
	Diagram *read = project.addNewDiagram();
	QVERIFY(read->fromXml(document));
	CabinetItem *copy = nullptr;
	for (QGraphicsItem *item : read->items())
		if (auto *found = qgraphicsitem_cast<CabinetItem *>(item)) copy = found;
	QVERIFY(copy);
	QCOMPARE(copy->kind(), CabinetItem::Kind::Drawing);
	QCOMPARE(copy->properties().source, QStringLiteral("MCB 1P"));
	QVERIFY(near(copy->properties().drawing.boundingRect(), result.path.boundingRect()));
	QCOMPARE(copy->properties().drawing.elementCount(), result.path.elementCount());
	QCOMPARE(copy->pos(), QPointF(40, 40));
		//Drawn over the rails and the ducts
	QVERIFY(copy->zValue() > 6);
}

void TstDxfReader::railIsNotDrawnUnderADevice()
{
	QETProject project;
	Diagram *cabinet = project.addNewCabinetDiagram();

		//A rail of 500 mm at (100, 100): 200 x 14 px at 1:5
	CabinetItem::Properties rail;
	rail.kind = CabinetItem::Kind::DinRail;
	rail.profile = CabinetItem::defaultRailProfile();
	rail.length = 500;
	rail.width = 35;
	auto *rail_item = new CabinetItem(rail);
	cabinet->addItem(rail_item);
	rail_item->placeAt(QPointF(100, 100));

		//A device of 20 x 90 mm on it, only its outline drawn: 8 x 36 px
	QPainterPath outline;
	outline.addRect(0, 0, 20, 90);
	CabinetItem::Properties device;
	device.kind = CabinetItem::Kind::Drawing;
	device.drawing = outline;
	device.length = 20;
	device.width = 90;
	auto *device_item = new CabinetItem(device);
	cabinet->addItem(device_item);
	device_item->placeAt(QPointF(150, 89));

	const QRectF area(90, 80, 220, 60);
	const qreal zoom = 8.0;
	QImage image((area.size() * zoom).toSize(), QImage::Format_ARGB32);
	auto render = [&]() {
		image.fill(Qt::white);
		QPainter painter(&image);
		cabinet->render(&painter, QRectF(image.rect()), area);
	};
	auto colour = [&](const QPointF &scene_point) {
		return image.pixelColor(((scene_point - area.topLeft()) * zoom).toPoint());
	};
	const QPointF inside_device(153.7, 107.3);
	const QPointF beside_device(131.3, 107.3);
	const QPointF device_line(150.0, 107.3);

	render();
		//The rail is drawn beside the device, not inside it
	QVERIFY(colour(beside_device) != QColor(Qt::white));
	QCOMPARE(colour(inside_device), QColor(Qt::white));
		//The lines of the device are drawn
	QVERIFY(colour(device_line).lightness() < 200);

		//The device taken off: the rail is whole again
	cabinet->removeItem(device_item);
	render();
	QVERIFY(colour(inside_device) != QColor(Qt::white));
	delete device_item;
}

void TstDxfReader::manufacturerFiles()
{
	const QString folder = qEnvironmentVariable("FASECAD_DXF_SAMPLES");
	if (folder.isEmpty() || !QDir(folder).exists())
		QSKIP("FASECAD_DXF_SAMPLES does not name a folder of DXF files");
	const QStringList files = QDir(folder).entryList({QStringLiteral("*.dxf")}, QDir::Files);
	if (files.isEmpty()) QSKIP("no DXF file in FASECAD_DXF_SAMPLES");

	for (const QString &name : files) {
		const QString path = QDir(folder).filePath(name);
		const DxfReader::Result result = DxfReader::readFile(path);
		QVERIFY2(result.ok, qPrintable(name + QStringLiteral(": ") + result.error));
		QVERIFY2(result.drawn > 10, qPrintable(name));

			//Every entity drawn: nothing of a front view is left out
		QVERIFY2(result.skipped.isEmpty(), qPrintable(name));
			//The size of a device. Not the $EXTMAX of the file: the program
			//writing it may leave it as it was before the last change.
		const QSizeF size = result.path.boundingRect().size();
		qInfo().noquote() << name << "drawn" << result.drawn
						  << "size" << size.width() << "x" << size.height() << "mm";
		QVERIFY2(size.width() > 5 && size.width() < 500
				 && size.height() > 5 && size.height() < 500, qPrintable(name));

		const QString shots = qEnvironmentVariable("FASECAD_DXF_SHOTS");
		if (!shots.isEmpty()) {
			const qreal zoom = 8.0;
			QImage image((size * zoom).toSize() + QSize(20, 20), QImage::Format_ARGB32);
			image.fill(Qt::white);
			QPainter painter(&image);
			painter.setRenderHint(QPainter::Antialiasing, true);
			painter.translate(10, 10);
			painter.scale(zoom, zoom);
			painter.setPen(QPen(Qt::black, 1.2 / zoom));
			painter.drawPath(result.path);
			painter.end();
			QVERIFY(image.save(QDir(shots).filePath(name + QStringLiteral(".png"))));
		}
	}
}

QTEST_MAIN(TstDxfReader)
#include "tst_dxfreader.moc"
