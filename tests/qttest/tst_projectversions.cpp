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
#include "diagram.h"
#include "factory/elementfactory.h"
#include "projectversions.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/independenttextitem.h"
#include "qetgraphicsitem/terminal.h"
#include "qetinformation.h"
#include "qetproject.h"
#include "titleblockproperties.h"

#include <QDomDocument>

/**
	Set version (Brandon, 2026-11-27): the first version is where the
	changes start; the next ones add a sheet with the devices added,
	removed or rewired, never the ones only moved.
*/
class tst_projectversions : public QObject
{
	Q_OBJECT

	int m_made = 0;

	Element *place(Diagram *diagram, const QString &label, const QPointF &pos)
	{
		const QString name = QStringLiteral("device%1").arg(++m_made);
		const QString xml = QStringLiteral(
				"<definition type=\"element\" link_type=\"simple\" "
				"width=\"40\" height=\"40\" hotspot_x=\"20\" hotspot_y=\"20\">"
				"<uuid uuid=\"%1\"/>"
				"<names><name lang=\"en\">Test</name></names>"
				"<description><rect x=\"-10\" y=\"-10\" width=\"20\" height=\"20\" "
				"style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
				"<terminal x=\"-10\" y=\"-20\" orientation=\"n\" name=\"1\" uuid=\"%2\"/>"
				"<terminal x=\"-10\" y=\"20\" orientation=\"s\" name=\"2\" uuid=\"%3\"/>"
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
		information.addValue(QETInformation::ELMT_DESCRIPTION, label == QLatin1String("-K1")
							 ? QStringLiteral("Contactor") : QString());
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

	static QString sheetText(Diagram *sheet)
	{
		QString text;
		for (QGraphicsItem *item : sheet->items()) {
			if (auto *independent = dynamic_cast<IndependentTextItem *>(item))
				text += independent->toPlainText();
		}
		return text;
	}

private slots:
	void initTestCase()
	{
		QETProject::setBackupEnabled(false);
	}

	void theChangesOfEachVersion()
	{
		QETProject project;
		Diagram *folio = project.addNewDiagram();
		Element *supply = place(folio, QStringLiteral("-X1"), {200, 60});
		Element *contactor = place(folio, QStringLiteral("-K1"), {200, 200});
		Element *motor = place(folio, QStringLiteral("-M1"), {200, 400});
		QVERIFY(supply && contactor && motor);
		folio->addItem(new Conductor(terminal(supply, QStringLiteral("2")), terminal(contactor, QStringLiteral("1"))));
		folio->addItem(new Conductor(terminal(contactor, QStringLiteral("2")), terminal(motor, QStringLiteral("1"))));
		const int folios = project.diagrams().size();

			//Version 1: no sheet
		QVERIFY(ProjectVersions::setVersion(&project).isEmpty());
		QCOMPARE(project.versionHistory().current(), 1);
		QCOMPARE(project.diagrams().size(), folios);
		QCOMPARE(project.projectProperties().value(QStringLiteral("project_version")).toString(),
				 QStringLiteral("1"));
		QVERIFY(project.versionHistory().last.value(QStringLiteral("-K1")).wiring
				.contains(QStringLiteral("1 → -X1:2")));

			//Nothing but a move: version 2 says so
		contactor->setPos(400, 200);
		motor->setPos(400, 400);
		QList<Diagram *> sheets = ProjectVersions::setVersion(&project);
		QCOMPARE(sheets.size(), 1);
		QCOMPARE(sheets.first()->title(), QStringLiteral("Changes of version 2"));
		QString text = sheetText(sheets.first());
		QVERIFY2(text.contains(QStringLiteral("No device was added, removed or rewired.")), qPrintable(text));

			//-M1 rewired to the supply, -H1 added, the contactor gone
		Element *lamp = place(folio, QStringLiteral("-H1"), {600, 200});
		QVERIFY(lamp);
		folio->addItem(new Conductor(terminal(supply, QStringLiteral("1")), terminal(lamp, QStringLiteral("1"))));
		for (Conductor *wire : contactor->conductors()) delete wire;
		delete contactor;
		folio->addItem(new Conductor(terminal(supply, QStringLiteral("2")), terminal(motor, QStringLiteral("1"))));
		sheets = ProjectVersions::setVersion(&project);
		QCOMPARE(sheets.size(), 1);
		text = sheetText(sheets.first());
		QVERIFY2(text.contains(QStringLiteral("Changes from version 2 to version 3")), qPrintable(text));
		QVERIFY2(text.contains(QStringLiteral("Devices added (1)\n  -H1")), qPrintable(text));
		QVERIFY2(text.contains(QStringLiteral("Devices removed (1)\n  -K1   Contactor")), qPrintable(text));
		QVERIFY2(text.contains(QStringLiteral("Wiring changed")), qPrintable(text));
		QVERIFY2(text.contains(QStringLiteral("− 1 → -K1:2")), qPrintable(text));
		QVERIFY2(text.contains(QStringLiteral("+ 1 → -X1:2")), qPrintable(text));
		QCOMPARE(project.versionHistory().current(), 3);

			//The revision index of the title block is the version: a number
			//typed in goes on from it, on every folio
		TitleBlockProperties defaults = project.defaultTitleBlockProperties();
		QCOMPARE(defaults.indexrev, QStringLiteral("3"));
		QCOMPARE(folio->border_and_titleblock.exportTitleBlock().indexrev, QStringLiteral("3"));
		defaults.indexrev = QStringLiteral("7");
		project.setDefaultTitleBlockProperties(defaults);
		sheets = ProjectVersions::setVersion(&project);
		QCOMPARE(sheets.size(), 1);
		QCOMPARE(sheets.first()->title(), QStringLiteral("Changes of version 8"));
		QCOMPARE(project.versionHistory().current(), 8);
		QCOMPARE(project.defaultTitleBlockProperties().indexrev, QStringLiteral("8"));
		QCOMPARE(sheets.first()->border_and_titleblock.exportTitleBlock().indexrev, QStringLiteral("8"));
			//No number there: 1
		defaults = project.defaultTitleBlockProperties();
		defaults.indexrev = QStringLiteral("A");
		project.setDefaultTitleBlockProperties(defaults);
		ProjectVersions::setVersion(&project);
		QCOMPARE(project.defaultTitleBlockProperties().indexrev, QStringLiteral("1"));

			//Kept with the project
		QDomDocument document;
		const QDomElement saved = ProjectVersions::toXml(project.versionHistory(), document);
		const ProjectVersions::History read = ProjectVersions::fromXml(saved);
		QVERIFY(read == project.versionHistory());
		QCOMPARE(read.versions.size(), 5);
	}
};

QET_APP_TEST_MAIN(tst_projectversions)
#include "tst_projectversions.moc"
