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
#include "partsdb/partsbom.h"
#include "partsdb/partsdatabase.h"
#include "partsdb/partslinks.h"
#include "qetgraphicsitem/element.h"
#include "qetproject.h"

#include <QTemporaryDir>

/**
	The bill of materials of the parts:

	 - a device with a Parts list counts those parts with their units,
	   one without counts the articles of its information;
	 - the lines are grouped by manufacturer and order number, their
	   units added up, with the labels of the devices;
	 - the parts database describes the parts it knows;
	 - a device excluded from the bill of materials counts nothing;
	 - a project is read through its database.
*/
class tst_partsbom : public QObject
{
	Q_OBJECT

	static PartsBom::Device device(const QString &label,
								   const QList<QPair<QString, QString>> &values)
	{
		PartsBom::Device result;
		result.label = label;
		for (const auto &value : values) {
			result.information.addValue(value.first, value.second);
		}
		return result;
	}

private slots:
	void linesAreGroupedAndAddedUp()
	{
		const QList<PartsBom::Device> devices = {
			device(QStringLiteral("-Q2"), {
				{PartsLinks::infoKey(), PartsLinks::encode(
					{{QStringLiteral("ABB"), QStringLiteral("S201-C10"), 2}})}}),
			device(QStringLiteral("-Q1"), {
				{QStringLiteral("manufacturer"), QStringLiteral("ABB")},
				{QStringLiteral("manufacturer_reference"),
				 QStringLiteral("S201-C10")},
				{QStringLiteral("manufacturer_auxiliary1"),
				 QStringLiteral("Schneider")},
				{QStringLiteral("manufacturer_reference_auxiliary1"),
				 QStringLiteral("LC1D09")},
				{QStringLiteral("quantity_auxiliary1"), QStringLiteral("2,5")}}),
			device(QStringLiteral("-K1"), {
				{QStringLiteral("comment"), QStringLiteral("no article")}})};

		const auto rows = PartsBom::rows(devices, nullptr);
		QCOMPARE(rows.size(), 2);
		QCOMPARE(rows.at(0).value("pos"), QStringLiteral("1"));
		QCOMPARE(rows.at(0).value("manufacturer"), QStringLiteral("ABB"));
		QCOMPARE(rows.at(0).value("quantity"), QStringLiteral("3"));
		QCOMPARE(rows.at(0).value("devices"), QStringLiteral("-Q1, -Q2"));
		QCOMPARE(rows.at(1).value("pos"), QStringLiteral("2"));
		QCOMPARE(rows.at(1).value("order_number"), QStringLiteral("LC1D09"));
		QCOMPARE(rows.at(1).value("quantity"), QStringLiteral("2.5"));
		QCOMPARE(rows.at(1).value("devices"), QStringLiteral("-Q1"));
	}

	void databaseDescribesTheParts()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		PartsDatabase database;
		QString error;
		QVERIFY2(database.open(dir.filePath("parts.sqlite"), &error),
				 qPrintable(error));
		Part part;
		part.manufacturer = QStringLiteral("ABB");
		part.order_number = QStringLiteral("S201-C10");
		part.description = QStringLiteral("Circuit breaker 1P C10");
		part.supplier = QStringLiteral("Wholesaler");
		part.unit = QStringLiteral("pcs");
		QVERIFY2(database.addPart(part, &error) > 0, qPrintable(error));

		const auto rows = PartsBom::rows(
			{device(QStringLiteral("-Q1"), {
				{PartsLinks::infoKey(), PartsLinks::encode(
					{{QStringLiteral("abb"), QStringLiteral("S201-C10"), 1}})}})},
			&database);
		QCOMPARE(rows.size(), 1);
		QCOMPARE(rows.at(0).value("description"),
				 QStringLiteral("Circuit breaker 1P C10"));
		QCOMPARE(rows.at(0).value("supplier"), QStringLiteral("Wholesaler"));
		QCOMPARE(rows.at(0).value("unit"), QStringLiteral("pcs"));
	}

	void projectIsReadAndExclusionHonoured()
	{
		const QString fixture = QFINDTESTDATA("fixtures/every_feature.qet");
		QVERIFY(!fixture.isEmpty());
		QETProject project(fixture);
		QCOMPARE(project.state(), QETProject::Ok);
		Element *element = nullptr;
		for (Diagram *diagram : project.diagrams()) {
			for (Element *candidate : diagram->elements()) {
				if (candidate->linkType() == Element::Simple) {
					element = candidate;
					break;
				}
			}
			if (element) {
				break;
			}
		}
		QVERIFY(element);

		const QString order = QStringLiteral("BOM-TEST-0001");
		DiagramContext information = element->elementInformations();
		information.addValue(PartsLinks::infoKey(), PartsLinks::encode(
			{{QStringLiteral("Test"), order, 4}}));
		element->setElementInformations(information);

		auto find = [&]() -> QMap<QString, QString> {
			for (const auto &row : PartsBom::rows(
					 PartsBom::devices(&project), nullptr)) {
				if (row.value("order_number") == order) {
					return row;
				}
			}
			return {};
		};
		QCOMPARE(find().value("quantity"), QStringLiteral("4"));

		information.addValue(QStringLiteral("exclude_from_bom"),
							 QStringLiteral("true"));
		element->setElementInformations(information);
		QVERIFY(find().isEmpty());
	}

		//The parts of a contact count, under the label of its master
		//when the contact has none of its own
	void contactPartsGoByTheMaster()
	{
		const QString fixture = QFINDTESTDATA("fixtures/every_feature.qet");
		QVERIFY(!fixture.isEmpty());
		QETProject project(fixture);
		QCOMPARE(project.state(), QETProject::Ok);
		Element *contact = nullptr;
		for (Diagram *diagram : project.diagrams()) {
			for (Element *candidate : diagram->elements()) {
				if (!contact && candidate->linkType() == Element::Slave
					&& !candidate->linkedElements().isEmpty()
					&& !candidate->linkedElements().first()
						->actualLabel().isEmpty()) {
					contact = candidate;
				}
			}
		}
		if (!contact) {
			QSKIP("the fixture has no contact linked to a labelled master");
		}
		const QString master = contact->linkedElements().first()->actualLabel();

		const QString order = QStringLiteral("BOM-CONTACT-0001");
		DiagramContext information = contact->elementInformations();
		information.addValue(QStringLiteral("label"), QString());
		information.addValue(PartsLinks::infoKey(), PartsLinks::encode(
			{{QStringLiteral("Test"), order, 1}}));
		contact->setElementInformations(information);

		QMap<QString, QString> found;
		for (const auto &row : PartsBom::rows(
				 PartsBom::devices(&project), nullptr)) {
			if (row.value("order_number") == order) {
				found = row;
			}
		}
		QCOMPARE(found.value("quantity"), QStringLiteral("1"));
		QVERIFY2(found.value("devices").contains(master),
				 qPrintable(found.value("devices") + " / " + master));
	}
};

QET_APP_TEST_MAIN(tst_partsbom)
#include "tst_partsbom.moc"
