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
#include "conductorproperties.h"
#include "diagram.h"
#include "factory/elementfactory.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminal.h"
#include "qetinformation.h"
#include "qetproject.h"
#include "wiresections.h"

#include <QDomDocument>
#include <QUndoCommand>
#include <QUndoStack>

/**
	The section of the wires a protection feeds, IEC 60204-1 table 6
	(Brandon, 2026-11-19)
*/
class tst_wiresections : public QObject
{
	Q_OBJECT

	int m_made = 0;

	Element *place(Diagram *diagram, const QString &label, const QString &link_type,
				   const QString &kind, const QPointF &pos)
	{
		const QString name = QStringLiteral("device%1").arg(++m_made);
		const QString xml = QStringLiteral(
				"<definition type=\"element\" link_type=\"%1\" "
				"width=\"40\" height=\"40\" hotspot_x=\"20\" hotspot_y=\"20\">"
				"<uuid uuid=\"%2\"/>"
				"<names><name lang=\"en\">Test</name></names>"
				"<kindInformations><kindInformation name=\"type\">%3</kindInformation></kindInformations>"
				"<description><rect x=\"-10\" y=\"-10\" width=\"20\" height=\"20\" "
				"style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
				"<terminal x=\"-10\" y=\"-20\" orientation=\"n\" name=\"1\" uuid=\"%4\"/>"
				"<terminal x=\"-10\" y=\"20\" orientation=\"s\" name=\"2\" uuid=\"%5\"/>"
				"</description></definition>")
				.arg(link_type, QUuid::createUuid().toString(), kind,
					 QUuid::createUuid().toString(), QUuid::createUuid().toString());
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
		element->setPos(pos);
		DiagramContext information = element->elementInformations();
		information.addValue(QETInformation::ELMT_LABEL, label);
		element->setElementInformations(information);
		return element;
	}

	static Terminal *terminal(Element *element, const QString &name)
	{
		for (Terminal *terminal : element->terminals()) {
			if (terminal->name() == name) return terminal;
		}
		return nullptr;
	}

private slots:
	void initTestCase()
	{
		QETProject::setBackupEnabled(false);
	}

	void theTableGivesTheSection()
	{
		WireSections::Conditions conditions;
		QCOMPARE(WireSections::minimumSection(10, conditions), 1.0);
		QCOMPARE(WireSections::minimumSection(16, conditions), 2.5);
		QCOMPARE(WireSections::minimumSection(25, conditions), 6.0);
		QCOMPARE(WireSections::minimumSection(500, conditions), 0.0);
			//Two circuits together, or a warmer cabinet: more copper
		conditions.grouped = 2;
		QCOMPARE(WireSections::minimumSection(16, conditions), 4.0);
		conditions.grouped = 1;
		conditions.ambient = 50;
		QCOMPARE(WireSections::minimumSection(16, conditions), 4.0);
		conditions.ambient = 40;
		conditions.method = WireSections::Method::E;
		QCOMPARE(WireSections::minimumSection(16, conditions), 1.5);
	}

	void numbersAreRead()
	{
		QCOMPARE(WireSections::parseSection(QStringLiteral("1,5 mm²")), 1.5);
		QCOMPARE(WireSections::parseSection(QStringLiteral("2.5")), 2.5);
		QCOMPARE(WireSections::parseSection(QStringLiteral("")), 0.0);
		QCOMPARE(WireSections::parseCurrent(QStringLiteral("C16")), 16.0);
		QCOMPARE(WireSections::parseCurrent(QStringLiteral("In = 10 A")), 10.0);
	}

	void aBreakerGivesItsWiresTheirSection()
	{
		QETProject project;
		Diagram *folio = project.addNewDiagram();
		Element *breaker = place(folio, QStringLiteral("-Q1"), QStringLiteral("master"),
								 QStringLiteral("protection"), {200, 200});
		Element *supply = place(folio, QStringLiteral("-X0"), QStringLiteral("simple"),
								QStringLiteral("coil"), {200, 60});
		Element *load = place(folio, QStringLiteral("-K1"), QStringLiteral("simple"),
							  QStringLiteral("coil"), {200, 400});
		QVERIFY(breaker && supply && load);
		QVERIFY(WireSections::isProtection(breaker));
		QVERIFY(!WireSections::isProtection(load));
		DiagramContext information = breaker->elementInformations();
		information.addValue(QETInformation::ELMT_CURRENT_RATING, QStringLiteral("C16"));
		breaker->setElementInformations(information);
		QCOMPARE(WireSections::ratedCurrent(breaker), 16.0);
		QCOMPARE(WireSections::outgoing(breaker).size(), 1);
		QCOMPARE(WireSections::outgoing(breaker).first()->name(), QStringLiteral("2"));

		auto *in = new Conductor(terminal(supply, QStringLiteral("2")), terminal(breaker, QStringLiteral("1")));
		folio->addItem(in);
		auto *out = new Conductor(terminal(breaker, QStringLiteral("2")), terminal(load, QStringLiteral("1")));
		folio->addItem(out);

		const QList<WireSections::Row> rows = WireSections::check(&project, {});
		QCOMPARE(rows.size(), 1);
		QCOMPARE(rows.first().label, QStringLiteral("-Q1"));
		QCOMPARE(rows.first().current, 16.0);
		QCOMPARE(rows.first().required, 2.5);
			//The wire out of it, not the one into it
		QCOMPARE(rows.first().wires.size(), 1);
		QCOMPARE(rows.first().wires.first().data(), out);
		QCOMPARE(rows.first().without, 1);

		QUndoCommand *undo = WireSections::apply(rows);
		QVERIFY(undo);
		project.undoStack()->push(undo);
		QCOMPARE(WireSections::parseSection(out->properties().m_wire_section), 2.5);
		QVERIFY(in->properties().m_wire_section.isEmpty());
			//Done: nothing more to give
		QVERIFY(!WireSections::apply(WireSections::check(&project, {})));
		project.undoStack()->undo();
		QVERIFY(out->properties().m_wire_section.isEmpty());
	}
};

QET_APP_TEST_MAIN(tst_wiresections)
#include "tst_wiresections.moc"
