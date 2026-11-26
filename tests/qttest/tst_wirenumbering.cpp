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
#include "wirenumbering.h"

#include <QDomDocument>
#include <QUndoCommand>
#include <QUndoStack>

#include <memory>

/**
	Numbering the wires at once (Brandon, 2026-11-19): -K1, -K2 and -K3,
	the wires -K1:A2 to -K2:A2 and -K1:A2 to -K3:A1 on one potential, the
	wire -K2:A1 to -K3:A2 on another one, higher on the sheet.
*/
class tst_wirenumbering : public QObject
{
	Q_OBJECT

	int m_made = 0;

	struct Sheet
	{
		Diagram *folio = nullptr;
		Conductor *w1 = nullptr;	///< -K1:A2 to -K2:A2
		Conductor *w2 = nullptr;	///< -K1:A2 to -K3:A1, same potential
		Conductor *w3 = nullptr;	///< -K2:A1 to -K3:A2
	};

	Element *place(Diagram *diagram, const QString &label, const QPointF &pos)
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

	static Conductor *wire(Diagram *folio, Terminal *a, Terminal *b)
	{
		auto *conductor = new Conductor(a, b);
		folio->addItem(conductor);
		return conductor;
	}

	Sheet build(QETProject &project)
	{
		Sheet sheet;
		sheet.folio = project.addNewDiagram();
		Element *k1 = place(sheet.folio, QStringLiteral("-K1"), {100, 100});
		Element *k2 = place(sheet.folio, QStringLiteral("-K2"), {300, 100});
		Element *k3 = place(sheet.folio, QStringLiteral("-K3"), {100, 300});
		if (!k1 || !k2 || !k3) return sheet;
		sheet.w1 = wire(sheet.folio, terminal(k1, QStringLiteral("A2")), terminal(k2, QStringLiteral("A2")));
		sheet.w2 = wire(sheet.folio, terminal(k1, QStringLiteral("A2")), terminal(k3, QStringLiteral("A1")));
		sheet.w3 = wire(sheet.folio, terminal(k2, QStringLiteral("A1")), terminal(k3, QStringLiteral("A2")));
		return sheet;
	}

	static void apply(QETProject &project, const WireNumbering::Options &options)
	{
		QUndoCommand *undo = WireNumbering::command(WireNumbering::plan(&project, options));
		if (undo) project.undoStack()->push(undo);
	}

	static QString text(Conductor *conductor) {return conductor->properties().text;}

private slots:
	void initTestCase()
	{
		QETProject::setBackupEnabled(false);
	}

	void variablesAreReplaced()
	{
		const QHash<QChar, QString> values{{QLatin1Char('F'), QStringLiteral("3")},
										   {QLatin1Char('C'), QStringLiteral("7")}};
		QCOMPARE(WireNumbering::evaluate(QStringLiteral("%F-%3N"), values, 7, 1), QStringLiteral("3-007"));
		QCOMPARE(WireNumbering::evaluate(QStringLiteral("%F.%n"), values, 7, 2), QStringLiteral("3.2"));
		QCOMPARE(WireNumbering::evaluate(QStringLiteral("%F%2C"), values, 1, 1), QStringLiteral("307"));
		QCOMPARE(WireNumbering::evaluate(QStringLiteral("100%% %Q"), values, 1, 1), QStringLiteral("100% %Q"));
		QCOMPARE(WireNumbering::examples().size(), 6);
	}

	void onePotentialOneNumber()
	{
		QETProject project;
		Sheet sheet = build(project);
		QVERIFY(sheet.w3);
		const QString w1_before = text(sheet.w1);
		const QString w3_before = text(sheet.w3);
		WireNumbering::Options options;
		options.formula = QStringLiteral("%N");
		apply(project, options);
			//The higher potential first
		QCOMPARE(text(sheet.w3), QStringLiteral("1"));
		QCOMPARE(text(sheet.w1), QStringLiteral("2"));
		QCOMPARE(text(sheet.w2), QStringLiteral("2"));

			//One step of undo
		project.undoStack()->undo();
		QCOMPARE(text(sheet.w1), w1_before);
		QCOMPARE(text(sheet.w3), w3_before);
	}

	void fromAndToForEachWire()
	{
		QETProject project;
		Sheet sheet = build(project);
		QVERIFY(sheet.w3);
		WireNumbering::Options options;
		options.formula = QStringLiteral("%S:%T/%D:%E");
		options.per_wire = true;
		apply(project, options);
		QCOMPARE(text(sheet.w1), QStringLiteral("-K1:A2/-K2:A2"));
		QCOMPARE(text(sheet.w2), QStringLiteral("-K1:A2/-K3:A1"));
		QCOMPARE(text(sheet.w3), QStringLiteral("-K2:A1/-K3:A2"));
	}

	void twoPotentialsNeverShareAName()
	{
		QETProject project;
		Sheet sheet = build(project);
		QVERIFY(sheet.w3);
		WireNumbering::Options options;
		options.formula = QStringLiteral("X");
		apply(project, options);
		QCOMPARE(text(sheet.w3), QStringLiteral("X"));
		QCOMPARE(text(sheet.w1), QStringLiteral("X.2"));
	}

	void aNumberFixedByHandStays()
	{
		QETProject project;
		Sheet sheet = build(project);
		QVERIFY(sheet.w3);
		ConductorProperties properties = sheet.w3->properties();
		properties.text = QStringLiteral("1");
		sheet.w3->setProperties(properties);
		sheet.w3->setFreezeLabel(true);

		WireNumbering::Options options;
		options.formula = QStringLiteral("%N");
		const QList<WireNumbering::Change> changes = WireNumbering::plan(&project, options);
		QCOMPARE(changes.size(), 1);
			//The first number goes to the other potential: "1" is taken
		QCOMPARE(changes.first().after, QStringLiteral("1.2"));
		QCOMPARE(changes.first().conductors.size(), 2);

		options.keep_fixed = false;
		QCOMPARE(WireNumbering::plan(&project, options).size(), 1);
	}

	void potentialTypesHaveTheirNames()
	{
		QETProject project;
		Sheet sheet = build(project);
		QVERIFY(sheet.w3);
		for (Conductor *conductor : {sheet.w1, sheet.w2}) {
			ConductorProperties properties = conductor->properties();
			properties.m_potential_type = QStringLiteral("L");
			conductor->setProperties(properties);
		}
		ConductorProperties properties = sheet.w3->properties();
		properties.m_potential_type = QStringLiteral("N");
		sheet.w3->setProperties(properties);

		WireNumbering::Options options;
		options.formula = QStringLiteral("%P");
		apply(project, options);
		QCOMPARE(text(sheet.w3), QStringLiteral("N"));
		QCOMPARE(text(sheet.w1), QStringLiteral("L1"));
	}

	void theSheetAndTheSelectionAlone()
	{
		QETProject project;
		Sheet sheet = build(project);
		QVERIFY(sheet.w3);
		WireNumbering::Options options;
		options.formula = QStringLiteral("S%N");
		options.scope = WireNumbering::Scope::Selection;
		options.selection = {sheet.w3};
		apply(project, options);
		QCOMPARE(text(sheet.w3), QStringLiteral("S1"));
		QVERIFY(text(sheet.w1) != QStringLiteral("S2"));
	}

	void ownFormulasAreKept()
	{
		WireNumbering::setOwnSchemes({{QStringLiteral("Mine"), QStringLiteral("W%3N"), true}});
		const QList<WireNumbering::Scheme> own = WireNumbering::ownSchemes();
		QCOMPARE(own.size(), 1);
		QCOMPARE(own.first().name, QStringLiteral("Mine"));
		QCOMPARE(own.first().formula, QStringLiteral("W%3N"));
		QVERIFY(own.first().per_wire);
		WireNumbering::setOwnSchemes({});
		QVERIFY(WireNumbering::ownSchemes().isEmpty());
	}
};

QET_APP_TEST_MAIN(tst_wirenumbering)
#include "tst_wirenumbering.moc"
