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
#include "../../sources/editor/partslayout.h"

#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QtTest>
#include <QUndoStack>

using Mode = PartsLayout::Mode;

/**
	Align and distribute of the element editor.
*/
class TestPartsLayout : public QObject
{
	Q_OBJECT

private slots:
	void distributingEvensTheGaps()
	{
			//Widths 10, 20, 10: from 0 to 100, the gaps become 30 each
		const QList<QRectF> rects{{0, 0, 10, 5}, {15, 0, 20, 5}, {90, 0, 10, 5}};
		const QList<QPointF> moves = PartsLayout::offsets(rects, Mode::DistributeHorizontally);
		QCOMPARE(moves.at(0), QPointF());
		QCOMPARE(moves.at(2), QPointF());
		QCOMPARE(moves.at(1), QPointF(25, 0));
	}

	void distributingGoesByWhereThePartsStand()
	{
			//The middle one listed last still stays between the others
		const QList<QRectF> rects{{0, 0, 4, 4}, {0, 100, 4, 4}, {0, 10, 4, 4}};
		const QList<QPointF> moves = PartsLayout::offsets(rects, Mode::DistributeVertically);
		QCOMPARE(moves.at(0), QPointF());
		QCOMPARE(moves.at(1), QPointF());
		QCOMPARE(moves.at(2), QPointF(0, 40));
	}

	void twoPartsCannotBeDistributed()
	{
		const QList<QRectF> rects{{0, 0, 4, 4}, {50, 0, 4, 4}};
		for (const QPointF &p : PartsLayout::offsets(rects, Mode::DistributeHorizontally))
			QCOMPARE(p, QPointF());
		QCOMPARE(PartsLayout::minimumCount(Mode::DistributeVertically), 3);
		QCOMPARE(PartsLayout::minimumCount(Mode::AlignLeft), 2);
	}

	void aligning()
	{
		const QList<QRectF> rects{{10, 0, 20, 10}, {40, 30, 10, 20}};
		QCOMPARE(PartsLayout::offsets(rects, Mode::AlignLeft).at(1), QPointF(-30, 0));
		QCOMPARE(PartsLayout::offsets(rects, Mode::AlignRight).at(0), QPointF(20, 0));
		QCOMPARE(PartsLayout::offsets(rects, Mode::AlignTop).at(1), QPointF(0, -30));
		QCOMPARE(PartsLayout::offsets(rects, Mode::AlignBottom).at(0), QPointF(0, 40));
			//Both centred on the centre of the two together (x 30, y 25)
		QCOMPARE(PartsLayout::offsets(rects, Mode::AlignHCenter).at(0), QPointF(10, 0));
		QCOMPARE(PartsLayout::offsets(rects, Mode::AlignVCenter).at(1), QPointF(0, -15));
	}

	void theCommandMovesAndUndoes()
	{
		QGraphicsScene scene;
		auto *a = scene.addRect(0, 0, 10, 10);
		auto *b = scene.addRect(0, 0, 10, 10);
		auto *c = scene.addRect(0, 0, 10, 10);
		b->setPos(12, 0);
		c->setPos(100, 0);
		QUndoStack stack;
		auto *command = new PartsLayoutCommand({a, b, c}, Mode::DistributeHorizontally);
		QVERIFY(command->isValid());
		stack.push(command);
		QCOMPARE(b->pos(), QPointF(50, 0));
		stack.undo();
		QCOMPARE(b->pos(), QPointF(12, 0));

			//Nothing to move: not a valid command
		PartsLayoutCommand none({a, c}, Mode::AlignTop);
		QVERIFY(!none.isValid());
	}
};

QTEST_MAIN(TestPartsLayout)
#include "tst_partslayout.moc"
