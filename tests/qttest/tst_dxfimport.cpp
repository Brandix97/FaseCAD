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
#include "../../sources/editor/dxfimport.h"

#include <QtTest>

/**
	A DXF drawing in the element editor: the pieces of the drawing the
	DXF reader gives, scaled and centred on the hotspot.
*/
class TestDxfImport : public QObject
{
	Q_OBJECT

private slots:
	void aSquareAndALineCentredAtRealSize()
	{
		QPainterPath drawing;
		drawing.addRect(0, 0, 20, 10);       //a closed outline
		drawing.moveTo(0, 20);
		drawing.lineTo(20, 20);              //a line under it
		const QList<DxfImport::Piece> pieces = DxfImport::pieces(drawing, 2.0);
		QCOMPARE(pieces.size(), 2);

		QVERIFY(pieces.at(0).closed);
		QCOMPARE(pieces.at(0).points.size(), 4);
			//20 x 20 mm drawing, centre (10, 10): at 2 px a mm, -20..20
		QCOMPARE(pieces.at(0).points.boundingRect(), QRectF(-20, -20, 40, 20));

		QVERIFY(!pieces.at(1).closed);
		QCOMPARE(pieces.at(1).points, QPolygonF({QPointF(-20, 20), QPointF(20, 20)}));
	}

	void nothingToDraw()
	{
		QVERIFY(DxfImport::pieces(QPainterPath(), 1.0).isEmpty());
		QPainterPath dot;
		dot.moveTo(5, 5);
		dot.lineTo(5, 5);
		QVERIFY(DxfImport::pieces(dot, 1.0).isEmpty());
	}

	void realSizeIsThePaperScale()
	{
		QCOMPARE(qRound(297 * DxfImport::realSize), 1123);
	}
};

QTEST_MAIN(TestDxfImport)
#include "tst_dxfimport.moc"
