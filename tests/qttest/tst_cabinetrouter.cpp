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
#include "cabinet/cabinetrouter.h"

#include <QTest>

/**
	The way a wire takes through the ducts of a mounting plate, in
	millimetres (Brandon, 2026-11-19): square into the nearest duct, along
	the ducts the shortest way, square out to the other device.
*/
class tst_cabinetrouter : public QObject
{
	Q_OBJECT

	static CabinetRouter::Duct horizontal(qreal x, qreal y, qreal length, qreal width = 40)
	{
		return {QRectF(x, y, length, width), Qt::Horizontal};
	}
	static CabinetRouter::Duct vertical(qreal x, qreal y, qreal length, qreal width = 40)
	{
		return {QRectF(x, y, width, length), Qt::Vertical};
	}

private slots:
	void theShortestWayAlongALadder()
	{
			//Two horizontal ducts, top and bottom, two vertical ones at
			//the sides
		const CabinetRouter router({horizontal(0, 0, 400), horizontal(0, 280, 400),
									vertical(0, 0, 320), vertical(360, 0, 320)});
		const CabinetRouter::Route route = router.route({100, 160}, {300, 160});
		QVERIFY(route.through_ducts);
			//Up into a duct, along it, down: 140 + 200 + 140
		QCOMPARE(route.length, 480.0);
		QCOMPARE(route.points.size(), 4);
		QCOMPARE(route.points.first(), QPointF(100, 160));
		QCOMPARE(route.points.last(), QPointF(300, 160));
		const qreal level = route.points.at(1).y();
		QVERIFY(level == 20.0 || level == 300.0);
		QCOMPARE(route.points.at(1), QPointF(100, level));
		QCOMPARE(route.points.at(2), QPointF(300, level));
	}

	void aCornerOfTwoDucts()
	{
			//An L: the vertical duct stands at the end of the horizontal one
		const CabinetRouter router({horizontal(0, 0, 200), vertical(200, 0, 300)});
		const CabinetRouter::Route route = router.route({50, 100}, {300, 250});
		QVERIFY(route.through_ducts);
		const QVector<QPointF> expected{{50, 100}, {50, 20}, {220, 20}, {220, 250}, {300, 250}};
		QCOMPARE(route.points, expected);
		QCOMPARE(route.length, 80.0 + 170.0 + 230.0 + 80.0);
	}

	void ductsOneAfterTheOther()
	{
			//1 mm apart: they still meet
		const CabinetRouter router({horizontal(0, 0, 100), horizontal(101, 0, 100)});
		const CabinetRouter::Route route = router.route({20, 100}, {180, 100});
		QVERIFY(route.through_ducts);
		QCOMPARE(route.length, 80.0 + 160.0 + 80.0);
	}

	void noDuctJoinsThem()
	{
		const CabinetRouter none({});
		CabinetRouter::Route route = none.route({0, 0}, {30, 40});
		QVERIFY(!route.through_ducts);
		QCOMPARE(route.length, 50.0);

			//Two ducts far apart, one near each device: the way of each
			//device into the other duct crosses the other device
		const QRectF a(35, 70, 30, 60);
		const QRectF b(35, 370, 30, 60);
		const CabinetRouter apart({horizontal(0, 0, 100), horizontal(0, 500, 100)}, {a, b});
		route = apart.route(a.center(), b.center(), a, b);
		QVERIFY(!route.through_ducts);
		QCOMPARE(route.points.size(), 2);
	}

	void aRouteIsSimplified()
	{
		const QVector<QPointF> points{{0, 0}, {0, 0}, {0, 10}, {0, 20}, {10, 20}, {20, 20}};
		const QVector<QPointF> expected{{0, 0}, {0, 20}, {20, 20}};
		QCOMPARE(CabinetRouter::simplified(points), expected);
		QCOMPARE(CabinetRouter::lengthOf(expected), 40.0);
			//Where it turns back, the turn stays
		const QVector<QPointF> back{{0, 100}, {0, 20}, {0, 60}};
		QCOMPARE(CabinetRouter::simplified(back), back);
	}

	void aDeviceGoesUpOrDownFirst()
	{
			//Right under a horizontal duct and near a vertical one: up
		const CabinetRouter router({horizontal(0, 0, 400), vertical(360, 0, 400)});
		const CabinetRouter::Route route = router.route({300, 100}, {380, 300});
		QCOMPARE(route.points.at(1), QPointF(300, 20));
	}

		/// Two terminals of one device (a PLC's output to its own input,
		/// Brandon 2026-11-28): out into the duct and back in
	void outAndBackIntoTheSameDevice()
	{
		const QRectF plc(100, 100, 60, 80);
		const CabinetRouter router({horizontal(0, 0, 400)}, {plc});
		const CabinetRouter::Route route = router.route(plc.center(), plc.center(), plc, plc);
		QVERIFY(route.through_ducts);
		QCOMPARE(route.points.first(), plc.center());
		QCOMPARE(route.points.last(), plc.center());
		QVERIFY(route.points.contains(QPointF(plc.center().x(), 20)));
		QCOMPARE(route.length, 2 * (plc.center().y() - 20));
	}
};

QTEST_GUILESS_MAIN(tst_cabinetrouter)
#include "tst_cabinetrouter.moc"
