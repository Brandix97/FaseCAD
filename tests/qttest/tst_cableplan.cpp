/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "qetapptest.h"

#include "cable/cable.h"
#include "cablelist/cablelistmodel.h"
#include "cablelist/cablelistrows.h"
#include "conductorproperties.h"
#include "diagram.h"
#include "properties/elementdata.h"
#include "qetgraphicsitem/element.h"
#include "qetinformation.h"
#include "qetgraphicsitem/conductor.h"
#include "qetproject.h"

#include <QDomDocument>

/**
	The cable plan: one row per core of every cable, used or spare.

	 - a cable of three cores with one wired gives three rows, in core
	   order: the wired one names its wire, both ends and its sheet, the
	   two others are spare and name nothing;
	 - a plan of one cable lists that cable only;
	 - a table keeps being a plan of that cable once saved and read back;
	 - the CSV writes the same rows, only with known columns.

	fixtures/every_feature.qet has the conductor 8bd16d7a, joining the
	strip X1 to a contact; no conductor of the fixture belongs to a
	cable yet.
*/
class tst_cableplan : public QObject
{
	Q_OBJECT

	QETProject *m_project = nullptr;
	Cable *m_cable = nullptr;

	Conductor *conductor(const QString &uuid) const
	{
		for (Diagram *diagram : m_project->diagrams()) {
			for (Conductor *conductor : diagram->conductors()) {
				if (conductor && conductor->uuid().toString() == uuid) {
					return conductor;
				}
			}
		}
		return nullptr;
	}

private slots:
	void init();
	void cleanup();

	void oneRowPerCore();
	void planOfOneCable();
	void tableKeepsBeingAPlan();
	void csvWritesTheRows();
};

void tst_cableplan::init()
{
	const QString fixture = QFINDTESTDATA("fixtures/every_feature.qet");
	QVERIFY(!fixture.isEmpty());
	m_project = new QETProject(fixture);
	QCOMPARE(m_project->state(), QETProject::Ok);

	Conductor *wired = conductor(
		QStringLiteral("{8bd16d7a-18c4-4fa0-9495-8dab65fd6408}"));
	QVERIFY(wired);

		//The contact the wire lands on gets a label: the fixture's own
		//block has none
	for (Diagram *diagram : m_project->diagrams()) {
		for (Element *element : diagram->elements()) {
			if (element->uuid().toString()
					== QLatin1String("{fc9cc920-5822-421a-916e-a17100acdcf6}")) {
				ElementData data{element->elementData()};
				data.m_informations.addValue(QETInformation::ELMT_LABEL,
											 QStringLiteral("-S7"));
				element->setElementData(data);
			}
		}
	}

	m_cable = m_project->newCable();
	QVERIFY(m_cable);
	m_cable->setDesignation(QStringLiteral("W7"));
	m_cable->setType(QStringLiteral("H05VV-F 3G1,5"));
	m_cable->setCoreCount(3);
	m_cable->setCoreColors({QStringLiteral("BN"), QStringLiteral("BU"),
							QStringLiteral("GNYE")});
	CableCore core;
	core.core = 0;
	core.conductor = wired->uuid();
	m_cable->setCore(core);
}

void tst_cableplan::cleanup()
{
	delete m_project;
	m_project = nullptr;
	m_cable = nullptr;
}

void tst_cableplan::oneRowPerCore()
{
	const auto rows = CableList::coreRows(m_project);
	QCOMPARE(rows.size(), 3);

	const auto &used = rows.at(0);
	QCOMPARE(used.value(QStringLiteral("plan_cable")), QStringLiteral("W7"));
	QCOMPARE(used.value(QStringLiteral("plan_core")), QStringLiteral("1"));
	QCOMPARE(used.value(QStringLiteral("plan_color")), QStringLiteral("BN"));
	QCOMPARE(used.value(QStringLiteral("used")), QStringLiteral("1"));
		//One end is the contact, whichever way the wire was drawn
	QVERIFY2(used.value(QStringLiteral("plan_from")).startsWith(QLatin1String("-S7"))
			 || used.value(QStringLiteral("plan_to")).startsWith(QLatin1String("-S7")),
			 qPrintable(used.value(QStringLiteral("plan_from")) + QStringLiteral(" / ")
						+ used.value(QStringLiteral("plan_to"))));
	QVERIFY(!used.value(QStringLiteral("plan_folio")).isEmpty());
	QCOMPARE(used.value(QStringLiteral("plan_wire")),
			 conductor(QStringLiteral("{8bd16d7a-18c4-4fa0-9495-8dab65fd6408}"))
			 ->properties().text);

	for (int i = 1; i < 3; ++i) {
		const auto &spare = rows.at(i);
		QCOMPARE(spare.value(QStringLiteral("plan_core")), QString::number(i + 1));
		QVERIFY(spare.value(QStringLiteral("used")).isEmpty());
		QVERIFY(spare.value(QStringLiteral("plan_from")).isEmpty());
		QVERIFY(spare.value(QStringLiteral("plan_wire")).isEmpty());
	}
	QCOMPARE(rows.at(2).value(QStringLiteral("plan_color")), QStringLiteral("GNYE"));
}

void tst_cableplan::planOfOneCable()
{
	Cable *other = m_project->newCable();
	QVERIFY(other);
	other->setDesignation(QStringLiteral("W8"));
	other->setCoreCount(2);

	QCOMPARE(CableList::coreRows(m_project).size(), 5);
	QCOMPARE(CableList::coreRows(m_project, m_cable->uuid()).size(), 3);
	const auto rows = CableList::coreRows(m_project, other->uuid());
	QCOMPARE(rows.size(), 2);
	QCOMPARE(rows.first().value(QStringLiteral("plan_cable")), QStringLiteral("W8"));
}

void tst_cableplan::tableKeepsBeingAPlan()
{
	CableListModel model(m_project);
	model.setIdentifier(CableListModel::cablePlanIdentifier());
	model.setOnlyCable(m_cable->uuid());
	model.setFields(CableList::defaultCoreKeys());
	QVERIFY(model.isCablePlan());
	QCOMPARE(model.rowCount(), 3);
	QCOMPARE(model.columnCount(), CableList::defaultCoreKeys().size());

	QDomDocument document;
	const QDomElement saved = model.toXml(document);

	CableListModel read(m_project);
	read.fromXml(saved);
	QVERIFY(read.isCablePlan());
	QCOMPARE(read.onlyCable(), m_cable->uuid());
	QCOMPARE(read.fields(), CableList::defaultCoreKeys());
	QCOMPARE(read.rowCount(), 3);

		//A cable list of the same project is still one row per cable
	CableListModel list(m_project);
	QVERIFY(!list.isCablePlan());
	QCOMPARE(list.rowCount(), CableList::rows(m_project).size());
}

void tst_cableplan::csvWritesTheRows()
{
	const QByteArray csv = CableList::coresToCsv(
				m_project,
				{QStringLiteral("plan_cable"), QStringLiteral("nr"),
				 QStringLiteral("plan_core")},
				true);
	QVERIFY(csv.startsWith("\xEF\xBB\xBF"));
	const QList<QByteArray> lines = csv.mid(3).trimmed().split('\n');
		//The header and one line per core
	QCOMPARE(lines.size(), 4);
		//"nr" belongs to the cable list, not to the plan: dropped
	QCOMPARE(lines.first().count(';'), 1);
	QVERIFY(lines.at(3).contains("\"W7\""));
	QVERIFY(lines.at(3).contains("\"3\""));
}

QET_APP_TEST_MAIN(tst_cableplan)
#include "tst_cableplan.moc"
