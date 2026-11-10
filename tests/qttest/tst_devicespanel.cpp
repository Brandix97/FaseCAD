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

#include "diagram.h"
#include "properties/elementdata.h"
#include "qetgraphicsitem/element.h"
#include "qetproject.h"
#include "ui/devicespanel.h"

#include <QSet>

/**
	The devices panel lists the symbols of a project by label:

	 - the labels come in natural order, -K2 before -K10, and the symbols
	   without a label last;
	 - folio reports and slave contacts are left out, every other symbol
	   of the project is there;
	 - each line names its sheet, counted from 1.

	fixtures/every_feature.qet is the project of the other app tests.
*/
class tst_devicespanel : public QObject
{
	Q_OBJECT

private slots:
	void naturalOrder();
	void listsTheProject();
};

void tst_devicespanel::naturalOrder()
{
	QVERIFY(DevicesPanel::labelLessThan(QStringLiteral("-K2"), QStringLiteral("-K10")));
	QVERIFY(!DevicesPanel::labelLessThan(QStringLiteral("-K10"), QStringLiteral("-K2")));
	QVERIFY(DevicesPanel::labelLessThan(QStringLiteral("-f1"), QStringLiteral("-F2")));
}

void tst_devicespanel::listsTheProject()
{
	const QString fixture = QFINDTESTDATA("fixtures/every_feature.qet");
	QVERIFY(!fixture.isEmpty());
	QETProject project(fixture);
	QCOMPARE(project.state(), QETProject::Ok);

	const QList<DevicesPanel::Device> devices = DevicesPanel::devices(&project);
	QVERIFY(!devices.isEmpty());

	QSet<QString> listed;
	bool label_seen_after_empty = false;
	bool empty_seen = false;
	for (int i = 0; i < devices.size(); ++i) {
		const DevicesPanel::Device &d = devices.at(i);
		listed.insert(d.uuid);
		QVERIFY(d.folio >= 1);
		if (d.label.isEmpty()) {
			empty_seen = true;
		} else if (empty_seen) {
			label_seen_after_empty = true;
		}
		if (i > 0 && !d.label.isEmpty() && !devices.at(i - 1).label.isEmpty()) {
			QVERIFY2(!DevicesPanel::labelLessThan(d.label, devices.at(i - 1).label),
					 qPrintable(devices.at(i - 1).label + QStringLiteral(" / ") + d.label));
		}
	}
	QVERIFY(!label_seen_after_empty);

		//Every symbol of the project but reports and slaves
	int expected = 0;
	for (Diagram *diagram : project.diagrams()) {
		for (Element *element : diagram->elements()) {
			const ElementData::Type type = element->elementData().m_type;
			const bool left_out = type == ElementData::Slave
					|| type == ElementData::NextReport
					|| type == ElementData::PreviousReport
					|| type == ElementData::Thumbnail
					|| type == ElementData::ConductorDefinition;
			QCOMPARE(listed.contains(element->uuid().toString()), !left_out);
			if (!left_out) {
				++expected;
					//Its sheet, counted from 1
				for (const DevicesPanel::Device &d : devices) {
					if (d.uuid == element->uuid().toString()) {
						QCOMPARE(d.folio, project.folioIndex(diagram) + 1);
					}
				}
			}
		}
	}
	QCOMPARE(devices.size(), expected);
}

QET_APP_TEST_MAIN(tst_devicespanel)
#include "tst_devicespanel.moc"
