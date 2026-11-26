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
#include "cable/cable.h"
#include "cable/cablemanager.h"
#include "cable/cablepart.h"
#include "cable/cablerenumber.h"
#include "conductorproperties.h"
#include "qetproject.h"

#include <QDomDocument>
#include <QPointer>
#include <QTimer>

/**
	Names and texts of cables and wires as FaseCAD sets them by default
	(Brandon, 2026-11-15):

	 - a cable takes -W and the first number free in the project, with
	   no numbering rule and no question when the cable tool is picked;
	 - renumbering names every cable again, -W1, -W2... in the order
	   they stand, around the names typed in by hand;
	 - the texts of a cable start where its line begins (left aligned)
	   and are lifted above the number of a core they would cover,
	   unless they were moved by hand;
	 - the text of a wire stands above a horizontal wire and left of a
	   vertical one, while a file which does not say keeps below and
	   right, the way it was drawn.
*/
class tst_cablenaming : public QObject
{
	Q_OBJECT

	QList<QPointer<Cable>> m_made;

	Cable *cable(const QString &name, qreal x, bool by_hand = false)
	{
		Diagram *diagram = QetAppTest::diagram();
		Cable *made = diagram->project()->newCable();
		made->setDesignation(name);
		made->setDesignationByHand(by_hand);
		CablePartData part;
		part.uuid = QUuid::createUuid();
		part.diagram = diagram->uuid();
		part.p1 = QPointF(x, 100);
		part.p2 = QPointF(x + 50, 100);
		made->addPart(part);
		m_made << made;
		return made;
	}

	/**
		The top of what a line of "-W2" draws, from x 0 to 300 at y
		100, with the number "3" of its one core @p first_wire from
		where it begins, its texts moved by hand by @p nudge.
	*/
	qreal topOfALineWithACoreAt(qreal first_wire, const QPointF &nudge = QPointF())
	{
		Diagram *diagram = QetAppTest::diagram();
		Cable *made = diagram->project()->newCable();
		m_made << made;
		made->setDesignation(QStringLiteral("-W2"));
		made->setCoreCount(1);
		made->setCoreColors({QStringLiteral("3")});
		CableProperties properties = made->properties();
		properties.text_offset = nudge;
		made->setProperties(properties);

		CablePartData data;
		data.uuid = QUuid::createUuid();
		data.diagram = diagram->uuid();
		data.p1 = QPointF(0, 100);
		data.p2 = QPointF(300, 100);
		made->addPart(data);
		CableCore core;
		core.core = 0;
		core.part = data.uuid;
		core.position = QPointF(first_wire, 100);
		made->setCore(core);

		CablePart item(data);
		item.setCable(made);
		return item.boundingRect().top();
	}

private slots:
	void init()
	{
		QVERIFY(QetAppTest::diagram());
	}

	void cleanup()
	{
		QETProject *project = QetAppTest::diagram()->project();
		for (const QPointer<Cable> &made : std::as_const(m_made)) {
			if (made) {
				project->removeCable(made);
			}
		}
		m_made.clear();
		QSettings().remove(QStringLiteral("cable-management/cable-align"));
	}

	void aCableTakesTheFirstFreeNumber()
	{
		Diagram *diagram = QetAppTest::diagram();
		QETProject *project = diagram->project();
		QVERIFY(!project->hasCableAutoNum());
		QCOMPARE(CableManager::nextDesignation(project, diagram),
				 QStringLiteral("-W1"));

		cable(QStringLiteral("-W1"), 0);
		cable(QStringLiteral("-W3"), 100);
		cable(QStringLiteral("W"), 200);
		QCOMPARE(CableManager::nextDesignation(project, diagram),
				 QStringLiteral("-W2"));
		cable(QStringLiteral("-W2"), 300);
		QCOMPARE(CableManager::nextDesignation(project, diagram),
				 QStringLiteral("-W4"));
	}

	void theCableToolAsksNothing()
	{
		QWidget *asked = nullptr;
		QTimer watch;
		connect(&watch, &QTimer::timeout, this, [&asked]() {
			if (QWidget *modal = QApplication::activeModalWidget()) {
				asked = modal;
				modal->close();
			}
		});
		watch.start(50);

		QAction *tool = QetAppTest::action(QStringLiteral("cable"));
		QVERIFY(tool);
		tool->trigger();
		QTest::qWait(200);
		watch.stop();

		QVERIFY2(!asked, "a window came up when the cable tool was picked");
		QVERIFY(QetAppTest::diagram()->eventInterfaceIsRunning());
		QetAppTest::diagram()->clearEventInterface();
		tool->setChecked(false);
	}

	void renumberingFollowsTheOrderOfTheCables()
	{
		QETProject *project = QetAppTest::diagram()->project();
		Cable *right = cable(QStringLiteral("-W7"), 400);
		Cable *left = cable(QStringLiteral("-W9"), 0);
		Cable *middle = cable(QStringLiteral("X"), 200);

		const CableRenumberPlan plan = CableRenumber::plan(project, true, true);
		QVERIFY2(plan.ok(), qPrintable(plan.error));
		QCOMPARE(plan.cables, (QList<Cable *>{left, middle, right}));
		QCOMPARE(plan.designations,
				 (QStringList{QStringLiteral("-W1"), QStringLiteral("-W2"),
							  QStringLiteral("-W3")}));
	}

	void renumberingGoesAroundTheNamesTypedByHand()
	{
		QETProject *project = QetAppTest::diagram()->project();
		Cable *first = cable(QStringLiteral("-W5"), 0);
		cable(QStringLiteral("-W2"), 100, true);
		Cable *third = cable(QStringLiteral("-W6"), 200);

		const CableRenumberPlan plan = CableRenumber::plan(project, true, false);
		QVERIFY2(plan.ok(), qPrintable(plan.error));
		QCOMPARE(plan.cables, (QList<Cable *>{first, third}));
		QCOMPARE(plan.designations,
				 (QStringList{QStringLiteral("-W1"), QStringLiteral("-W3")}));
	}

	/**
		Every line across the folios of a real project, drawn from one
		end and then from the other: the crossings, which number the
		cores, come from the left (from the top for an upright line)
		both times.
	*/
	void coresAreNumberedFromTheLeftWhicheverEndIsDrawnFirst()
	{
		const QString file = QFINDTESTDATA("fixtures/every_feature.qet");
		QVERIFY(!file.isEmpty());
		QETProject project(file);
		QCOMPARE(project.state(), QETProject::Ok);

		int lines = 0;
		for (Diagram *diagram : project.diagrams())
		{
			const QRectF area = diagram->itemsBoundingRect();
			for (int upright = 0 ; upright < 2 ; ++upright)
			{
				const qreal from = upright ? area.left() : area.top();
				const qreal to = upright ? area.right() : area.bottom();
				for (qreal at = from + 1.5 ; at < to ; at += 10.0)
				{
					const QLineF forward = upright
							? QLineF(at, area.top() - 1, at, area.bottom() + 1)
							: QLineF(area.left() - 1, at, area.right() + 1, at);
					const QLineF backward(forward.p2(), forward.p1());
					const QList<CableCrossing> one =
							CableManager::crossedConductors(diagram, forward);
					const QList<CableCrossing> other =
							CableManager::crossedConductors(diagram, backward);
					if (one.size() < 2) continue;
					++lines;

					QCOMPARE(other.size(), one.size());
					for (int i = 0 ; i < one.size() ; ++i)
					{
						QCOMPARE(other.at(i).conductor, one.at(i).conductor);
						if (i) {
							const qreal before = upright
									? one.at(i - 1).position.y()
									: one.at(i - 1).position.x();
							const qreal here = upright
									? one.at(i).position.y()
									: one.at(i).position.x();
							QVERIFY(before <= here);
						}
					}
				}
			}
		}
		QVERIFY2(lines > 0, "no line across two wires: the fixture changed");
	}

	void theNameStandsClearOfTheFirstCore()
	{
			//Far along the line, the number is nowhere near the name
		const qreal far = topOfALineWithACoreAt(250);
			//Close to where the line begins, under the name
		const qreal near = topOfALineWithACoreAt(20);
		QVERIFY2(near < far - 5, qPrintable(QStringLiteral("%1 / %2")
											.arg(near).arg(far)));
			//Moved by hand (half a unit down), it stays where he put it:
			//not lifted
		const qreal nudged = topOfALineWithACoreAt(20, QPointF(0, 0.5));
		QVERIFY2(nudged >= far, qPrintable(QStringLiteral("%1 / %2")
										   .arg(nudged).arg(far)));
	}

	void cableTextsAreLeftAlignedByDefault()
	{
		QCOMPARE(QETApp::cableTextAlignment(), Qt::Alignment(Qt::AlignLeft));
		QETApp::setCableTextAlignment(Qt::AlignRight);
		QCOMPARE(QETApp::cableTextAlignment(), Qt::Alignment(Qt::AlignRight));
	}

	void wireTextsStandAboveAndLeftByDefault()
	{
		const ConductorProperties made;
		QCOMPARE(made.m_horizontal_alignment, Qt::Alignment(Qt::AlignTop));
		QCOMPARE(made.m_vertical_alignment, Qt::Alignment(Qt::AlignLeft));

		const ConductorProperties defaults = ConductorProperties::defaultProperties();
		QCOMPARE(defaults.m_horizontal_alignment, Qt::Alignment(Qt::AlignTop));
		QCOMPARE(defaults.m_vertical_alignment, Qt::Alignment(Qt::AlignLeft));
	}

	void aFileWhichDoesNotSayKeepsBelowAndRight()
	{
		QDomDocument document;
		QDomElement element = document.createElement(QStringLiteral("conductor"));
		ConductorProperties read;
		read.fromXml(element);
		QCOMPARE(read.m_horizontal_alignment, Qt::Alignment(Qt::AlignBottom));
		QCOMPARE(read.m_vertical_alignment, Qt::Alignment(Qt::AlignRight));

		ConductorProperties written;
		QDomElement saved = document.createElement(QStringLiteral("conductor"));
		written.toXml(saved);
		ConductorProperties again;
		again.fromXml(saved);
		QCOMPARE(again.m_horizontal_alignment, Qt::Alignment(Qt::AlignTop));
		QCOMPARE(again.m_vertical_alignment, Qt::Alignment(Qt::AlignLeft));
	}
};

QET_APP_TEST_MAIN(tst_cablenaming)
#include "tst_cablenaming.moc"
