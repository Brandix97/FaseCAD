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
#include "ElementsCollection/xmlelementcollection.h"
#include "diagram.h"
#include "factory/elementfactory.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminal.h"
#include "qetproject.h"
#include "qetresult.h"

#include <QDomDocument>
#include <QTemporaryDir>
#include <QUndoStack>

/**
	Wires kept apart (Brandon, 2026-11-22): the middle stretch of an
	automatic wire that would lie along another one is moved, one grid step
	at a time, to where it is free; and a new project hops the horizontal
	wire of a crossing.
*/
class tst_wiresapart : public QObject
{
	Q_OBJECT

	Diagram *m_diagram = nullptr;

		/// A symbol with one terminal facing @p facing, placed at @p pos
	Element *symbol(const QString &name, const QString &facing, const QPointF &pos)
	{
		QDomDocument definition;
		definition.setContent(QStringLiteral(
			R"(<definition type="element" link_type="simple" width="20" height="20" hotspot_x="10" hotspot_y="10">
<names><name lang="en">%1</name></names>
<description><terminal x="0" y="0" orientation="%2"/></description></definition>)")
				.arg(name, facing));
		auto *collection = m_diagram->project()->embeddedElementCollection();
		if (!collection->addElementDefinition(QStringLiteral("import"), name, definition.documentElement()))
			return nullptr;
		int state = 0;
		Element *element = ElementFactory::Instance()->createElement(
			ElementsLocation(QStringLiteral("embed://import/%1.elmt").arg(name), m_diagram->project()),
			nullptr, &state);
		if (state || !element)
			return nullptr;
		m_diagram->addItem(element);
		element->setPos(pos);
		return element;
	}

		/// A wire from a terminal facing down at (@p x1, 100) to one
		/// facing up at (@p x2, 200): its middle stretch is horizontal
	Conductor *wire(const QString &name, qreal x1, qreal x2)
	{
		Element *top = symbol(name + QStringLiteral("_top"), QStringLiteral("s"), QPointF(x1, 100));
		Element *bottom = symbol(name + QStringLiteral("_bottom"), QStringLiteral("n"), QPointF(x2, 200));
		if (!top || !bottom) return nullptr;
		auto *conductor = new Conductor(top->terminals().first(), bottom->terminals().first());
		m_diagram->addItem(conductor);
		return conductor;
	}

		/// The horizontal pieces of @p conductor, in scene coordinates
	static QList<QLineF> horizontals(const Conductor *conductor)
	{
		const QPainterPath path = conductor->mapToScene(conductor->path());
		QList<QLineF> pieces;
		for (int i = 1 ; i < path.elementCount() ; ++i) {
			const QPointF a = path.elementAt(i - 1), b = path.elementAt(i);
			if (qAbs(a.y() - b.y()) < 0.5 && qAbs(a.x() - b.x()) > 0.5)
				pieces << QLineF(a, b);
		}
		return pieces;
	}

		/// How far the horizontal pieces of @p a lie on those of @p b
	static qreal along(const Conductor *a, const Conductor *b)
	{
		qreal on = 0.0;
		for (const QLineF &p : horizontals(a)) {
			for (const QLineF &q : horizontals(b)) {
				if (qAbs(p.y1() - q.y1()) > 0.5) continue;
				const qreal length = qMin(qMax(p.x1(), p.x2()), qMax(q.x1(), q.x2()))
						- qMax(qMin(p.x1(), p.x2()), qMin(q.x1(), q.x2()));
				if (length > 0.5) on += length;
			}
		}
		return on;
	}

	static bool savesItsPath(const Conductor *conductor)
	{
		QDomDocument document;
		QHash<Terminal *, int> table;
		return !conductor->toXml(document, table).firstChildElement(QStringLiteral("segment")).isNull();
	}

private slots:
	void init()
	{
		m_diagram = QetAppTest::diagram();
		QVERIFY(m_diagram);
			//The wires first: an element takes its wires with it
		for (Conductor *conductor : m_diagram->conductors()) {
			m_diagram->removeItem(conductor);
			delete conductor;
		}
		for (Element *element : m_diagram->elements()) {
			m_diagram->removeItem(element);
			delete element;
		}
	}

	void aNewProjectHopsAndKeepsWiresApart()
	{
		QETProject *project = m_diagram->project();
		QVERIFY(project->wireHops() == WireHops::Mode::Horizontal);
		QVERIFY(project->wiresApart());
	}

	/// A wire drawn across another one hops at once, as it does once
	/// moved: the vertical wire is drawn first, the crossing one after
	void aWireDrawnAcrossHops()
	{
		Element *top = symbol(QStringLiteral("v_top"), QStringLiteral("s"), QPointF(300, 50));
		Element *bottom = symbol(QStringLiteral("v_bottom"), QStringLiteral("n"), QPointF(300, 350));
		QVERIFY(top && bottom);
		auto *vertical = new Conductor(top->terminals().first(), bottom->terminals().first());
		m_diagram->addItem(vertical);
			//Painted once before the other wire exists
		QCOMPARE(vertical->paintedPath(), vertical->path());

		Conductor *crossing = wire(QStringLiteral("h"), 150, 450);
		QVERIFY(crossing);
		crossing->keepApart();
		QVERIFY2(crossing->paintedPath() != crossing->path(), "the new wire draws no hop");
		const QPainterPath at_once = crossing->paintedPath();

			//Moved away and back: the same hop
		crossing->setPos(crossing->pos() + QPointF(0, 0.0001));
		crossing->setPos(crossing->pos() - QPointF(0, 0.0001));
		QCOMPARE(crossing->paintedPath(), at_once);
	}

	void aWireIsKeptOffTheOther()
	{
		Conductor *first = wire(QStringLiteral("a"), 100, 200);
		Conductor *second = wire(QStringLiteral("b"), 120, 220);
		QVERIFY(first && second);
			//Both automatic, their middle stretches on the same line
		QVERIFY(along(first, second) > 50.0);
		const QList<QLineF> first_before = horizontals(first);

		QVERIFY(second->keepApart());
		QCOMPARE(along(first, second), 0.0);
		QCOMPARE(horizontals(first), first_before);
			//One grid step away, on the side where it crosses nothing:
			//above, its ends do not cross the first wire
		QCOMPARE(horizontals(second).size(), 1);
		QCOMPARE(horizontals(second).first().y1(), first_before.first().y1() - Diagram::yGrid);
			//Kept as a path moved by hand: saved, and not moved again
		QVERIFY(savesItsPath(second));
		QVERIFY(!savesItsPath(first));
		QVERIFY(!second->keepApart());
			//Nothing to move off: the first one stays automatic
		QVERIFY(!first->keepApart());
		QVERIFY(!savesItsPath(first));
	}

	void undoneWithTheStepItIsPartOf()
	{
		Conductor *first = wire(QStringLiteral("c"), 100, 200);
		Conductor *second = wire(QStringLiteral("d"), 120, 220);
		QVERIFY(first && second);
		QUndoStack stack;
		auto *step = new QUndoCommand(QStringLiteral("move"));
		QVERIFY(second->keepApart(step));
		QCOMPARE(step->childCount(), 1);
		stack.push(step);
		QCOMPARE(along(first, second), 0.0);
		stack.undo();
		QVERIFY(along(first, second) > 50.0);
		QVERIFY(!savesItsPath(second));
		stack.redo();
		QCOMPARE(along(first, second), 0.0);
		QVERIFY(savesItsPath(second));
	}

	void offInAProjectSavedWithoutIt()
	{
		QETProject *project = m_diagram->project();
		project->setWiresApart(false);
		Conductor *first = wire(QStringLiteral("e"), 100, 200);
		Conductor *second = wire(QStringLiteral("f"), 120, 220);
		QVERIFY(first && second);
		QVERIFY(!second->keepApart());
		QVERIFY(along(first, second) > 50.0);
		project->setWiresApart(true);
	}

	void theSettingIsSaved()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		QETProject project;
		project.addNewDiagram();
		QVERIFY(!project.wiresApart());
		QVERIFY(project.wireHops() == WireHops::Mode::None);
		project.setWiresApart(true);
		project.setFilePath(dir.filePath(QStringLiteral("apart.qet")));
		QVERIFY(project.write().isOk());
		{
			QETProject reopened(project.filePath());
			QCOMPARE(reopened.state(), QETProject::Ok);
			QVERIFY(reopened.wiresApart());
			QVERIFY(reopened.wireHops() == WireHops::Mode::None);
		}
		project.setWiresApart(false);
		project.setWireHops(WireHops::Mode::Vertical);
		QVERIFY(project.write().isOk());
		QETProject reopened(project.filePath());
		QCOMPARE(reopened.state(), QETProject::Ok);
		QVERIFY(!reopened.wiresApart());
		QVERIFY(reopened.wireHops() == WireHops::Mode::Vertical);
	}

	/// A project saved before the hops existed shows them; "No hop"
	/// chosen stays so
	void anOldProjectHops()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		QETProject project;
		project.addNewDiagram();
		project.setWireHops(WireHops::Mode::Horizontal);
		project.setFilePath(dir.filePath(QStringLiteral("old.qet")));
		QVERIFY(project.write().isOk());
		{
				//Horizontal and nothing else: saved as a project of before
			QFile file(project.filePath());
			QVERIFY(file.open(QIODevice::ReadOnly));
			QVERIFY(!file.readAll().contains("wire_crossings"));
			QETProject reopened(project.filePath());
			QVERIFY(reopened.wireHops() == WireHops::Mode::Horizontal);
		}
		project.setWireHops(WireHops::Mode::None);
		QVERIFY(project.write().isOk());
		QETProject reopened(project.filePath());
		QVERIFY(reopened.wireHops() == WireHops::Mode::None);
	}
};

QET_APP_TEST_MAIN(tst_wiresapart)
#include "tst_wiresapart.moc"
