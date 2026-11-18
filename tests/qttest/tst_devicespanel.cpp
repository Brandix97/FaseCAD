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

#include "cabinet/cabinetdevices.h"
#include "cabinet/cabinetitem.h"
#include "diagram.h"
#include "partsdb/partsdatabase.h"
#include "partsdb/partslinks.h"
#include "properties/elementdata.h"
#include "qetgraphicsitem/element.h"
#include "qetproject.h"
#include "qetresult.h"
#include "ui/devicespanel.h"

#include <QSet>
#include <QTemporaryDir>

/**
	The devices panel lists the symbols of a project by label:

	 - the labels come in natural order, -K2 before -K10, and the symbols
	   without a label last;
	 - folio reports and slave contacts are left out, every other symbol
	   of the project is there;
	 - each line names its sheet, counted from 1;
	 - a device drawn on a cabinet sheet is found there, whether the sheet
	   is built or not, and its drawing takes the label the device has;
	 - the drawing of a device is the DXF of its part in the parts
	   database; without one, there is none.

	fixtures/every_feature.qet is the project of the other app tests.
*/
class tst_devicespanel : public QObject
{
	Q_OBJECT

private slots:
	void naturalOrder();
	void listsTheProject();
	void oneLinePerDevice();
	void labelsAreNumbered();
	void devicesAreOrderedByPosition();
	void drawingsOnCabinetSheetsAreFound();
	void drawingComesFromTheParts();
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

void tst_devicespanel::oneLinePerDevice()
{
	auto symbol = [](const QString &label, int folio, const QString &cell,
					 const QString &description, const QString &parts = QString()) {
		DevicesPanel::Device device;
		device.uuid = QUuid::createUuid().toString();
		device.diagram_uuid = QUuid::createUuid().toString();
		device.label = label;
		device.folio = folio;
		device.cell = cell;
		device.description = description;
		device.parts = parts;
		return device;
	};
	const QString cpu = PartsLinks::encode(
		{{QStringLiteral("ACME"), QStringLiteral("PLC-24DC"), 1}});
		//A PLC drawn on three sheets, its article on its second symbol
		//only; a relay; two symbols without a label
	const QList<DevicesPanel::Device> devices {
		symbol(QStringLiteral("-K1"), 1, QStringLiteral("E2"), QStringLiteral("DI a")),
		symbol(QStringLiteral("-K1"), 1, QStringLiteral("E11"), QStringLiteral("PLC 24 V DC"), cpu),
		symbol(QStringLiteral("-K1"), 4, QStringLiteral("C4"), QStringLiteral("DQ")),
		symbol(QStringLiteral("-K2"), 2, QStringLiteral("C3"), QStringLiteral("Relay")),
		symbol(QString(), 1, QStringLiteral("A1"), QString()),
		symbol(QString(), 3, QStringLiteral("B2"), QString())};

	const QList<DevicesPanel::Line> lines = DevicesPanel::lines(devices);
	QCOMPARE(lines.size(), 3);

	const DevicesPanel::Line &plc = lines.at(0);
	QCOMPARE(plc.label, QStringLiteral("-K1"));
	QCOMPARE(plc.symbols.size(), 3);
		//It stands for the symbol made of parts, and says its article once
	QCOMPARE(plc.main, 1);
	QCOMPARE(plc.description, QStringLiteral("PLC 24 V DC"));
	QCOMPARE(plc.article, QStringLiteral("PLC-24DC"));
	QCOMPARE(plc.sheets, QStringLiteral("1, 4"));

	const DevicesPanel::Line &relay = lines.at(1);
	QCOMPARE(relay.label, QStringLiteral("-K2"));
	QCOMPARE(relay.main, 0);
	QVERIFY(relay.article.isEmpty());
	QCOMPARE(relay.sheets, QStringLiteral("2-C3"));

	QVERIFY(lines.at(2).label.isEmpty());
	QCOMPARE(lines.at(2).symbols.size(), 2);
}

void tst_devicespanel::labelsAreNumbered()
{
	using L = QStringList;
	QCOMPARE(DevicesPanel::numberedLabels(QStringLiteral("-K1"), 3),
			 L({QStringLiteral("-K1"), QStringLiteral("-K2"), QStringLiteral("-K3")}));
		//The zeros kept, the number growing past them
	QCOMPARE(DevicesPanel::numberedLabels(QStringLiteral("-K08"), 3),
			 L({QStringLiteral("-K08"), QStringLiteral("-K09"), QStringLiteral("-K10")}));
		//The last number only: a terminal of a strip
	QCOMPARE(DevicesPanel::numberedLabels(QStringLiteral("-X0:1"), 2),
			 L({QStringLiteral("-X0:1"), QStringLiteral("-X0:2")}));
		//Letters after the number stay
	QCOMPARE(DevicesPanel::numberedLabels(QStringLiteral("-K1A"), 2),
			 L({QStringLiteral("-K1A"), QStringLiteral("-K2A")}));
		//No number: one is added, from 1
	QCOMPARE(DevicesPanel::numberedLabels(QStringLiteral("-K"), 2),
			 L({QStringLiteral("-K1"), QStringLiteral("-K2")}));
		//A step
	QCOMPARE(DevicesPanel::numberedLabels(QStringLiteral("-K10"), 3, 10),
			 L({QStringLiteral("-K10"), QStringLiteral("-K20"), QStringLiteral("-K30")}));
}

void tst_devicespanel::devicesAreOrderedByPosition()
{
	auto at = [](int folio, const QString &cell) {
		DevicesPanel::Device device;
		device.folio = folio;
		device.cell = cell;
		return device;
	};
		//Sheet first, then column, then row; column 4 before column 11
	QVERIFY(DevicesPanel::positionLessThan(at(1, QStringLiteral("C4")),
										   at(1, QStringLiteral("E11"))));
	QVERIFY(!DevicesPanel::positionLessThan(at(1, QStringLiteral("E11")),
											at(1, QStringLiteral("C4"))));
	QVERIFY(DevicesPanel::positionLessThan(at(1, QStringLiteral("B4")),
										   at(1, QStringLiteral("C4"))));
	QVERIFY(DevicesPanel::positionLessThan(at(1, QStringLiteral("H17")),
										   at(2, QStringLiteral("A1"))));
}

void tst_devicespanel::drawingsOnCabinetSheetsAreFound()
{
	const QString fixture = QFINDTESTDATA("fixtures/every_feature.qet");
	QVERIFY(!fixture.isEmpty());
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const QString path = dir.filePath(QStringLiteral("cabinet_devices.qet"));
	QUuid device;
	QString label;
	int folio = 0;
	{
		QETProject project(fixture);
		QCOMPARE(project.state(), QETProject::Ok);
		const int before = CabinetDevices::placed(&project).size();
		for (const DevicesPanel::Device &d : DevicesPanel::devices(&project)) {
			if (!d.label.isEmpty()) {
				device = QUuid(d.uuid);
				label = d.label;
				break;
			}
		}
		QVERIFY(!device.isNull());

		Diagram *cabinet = project.addNewCabinetDiagram();
		folio = project.folioIndex(cabinet) + 1;
		CabinetItem::Properties properties;
		properties.kind = CabinetItem::Kind::Drawing;
		properties.drawing.addRect(0, 0, 45, 90);
		properties.length = 45;
		properties.width = 90;
		properties.device = device;
			//An old label: the drawing takes the one its device has now
		properties.device_label = QStringLiteral("-OLD");
		auto *drawing = new CabinetItem(properties);
		cabinet->addItem(drawing);
		drawing->placeAt(QPointF(60, 60));
		QCOMPARE(drawing->properties().device_label, label);

		const QList<CabinetDevices::Placed> placed = CabinetDevices::placed(&project);
		QCOMPARE(placed.size(), before + 1);
		QCOMPARE(placed.last().device, device);
		QCOMPARE(placed.last().label, label);
		QCOMPARE(placed.last().folio, folio);

		project.setFilePath(path);
		QVERIFY(project.write().isOk());
	}

		//Opened again: found on the sheet as it was read, built or not
	QETProject project(path);
	QCOMPARE(project.state(), QETProject::Ok);
	const QList<Diagram *> cabinets = CabinetDevices::cabinetFolios(&project);
	QVERIFY(!cabinets.isEmpty());
	Diagram *cabinet = cabinets.last();
	const bool built = cabinet->isLoaded();
	bool found = false;
	for (const CabinetDevices::Placed &placed : CabinetDevices::placed(&project)) {
		if (placed.device == device) {
			QCOMPARE(placed.label, label);
			QCOMPARE(placed.folio, folio);
			found = true;
		}
	}
	QVERIFY(found);
		//Reading it did not build the sheet
	QCOMPARE(cabinet->isLoaded(), built);
}

void tst_devicespanel::drawingComesFromTheParts()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const QString path = dir.filePath(QStringLiteral("parts.sqlite"));
		//A front view 45 mm wide and 90 mm high
	const QByteArray dxf = QByteArrayLiteral(
		"0\nSECTION\n2\nENTITIES\n"
		"0\nLINE\n8\n0\n10\n0\n20\n0\n11\n45\n21\n0\n"
		"0\nLINE\n8\n0\n10\n45\n20\n0\n11\n45\n21\n90\n"
		"0\nENDSEC\n0\nEOF\n");
	{
		PartsDatabase database;
		QString error;
		QVERIFY2(database.open(path, &error), qPrintable(error));
		Part with_dxf;
		with_dxf.manufacturer = QStringLiteral("ACME");
		with_dxf.order_number = QStringLiteral("CPU-1");
		const qint64 id = database.addPart(with_dxf, &error);
		QVERIFY2(id > 0, qPrintable(error));
		QVERIFY2(database.addFile(id, PartFileKind::Dxf, QStringLiteral("cpu-1.dxf"),
								  dxf, &error) > 0, qPrintable(error));
		Part without = with_dxf;
		without.order_number = QStringLiteral("PSU-2");
		QVERIFY2(database.addPart(without, &error) > 0, qPrintable(error));
	}
	PartsDatabase::setEnabled(true);
	PartsDatabase::setConfiguredPath(path);

	const QString cpu = PartsLinks::encode({{QStringLiteral("ACME"), QStringLiteral("CPU-1"), 1}});
	const QString psu = PartsLinks::encode({{QStringLiteral("ACME"), QStringLiteral("PSU-2"), 1}});
	CabinetItem::Properties properties;
	QString error;
		//The second part has the DXF: it is found
	QVERIFY(CabinetDevices::drawingFromParts(psu + QLatin1Char('\n') + cpu, properties, &error));
	QVERIFY(error.isEmpty());
	QCOMPARE(properties.kind, CabinetItem::Kind::Drawing);
	QCOMPARE(properties.source, QStringLiteral("cpu-1"));
	QVERIFY(qAbs(properties.length - 45.0) < 1e-6);
	QVERIFY(qAbs(properties.width - 90.0) < 1e-6);

		//No DXF, no part, no parts database: no drawing, and no error
	QVERIFY(!CabinetDevices::drawingFromParts(psu, properties, &error));
	QVERIFY(error.isEmpty());
	QVERIFY(!CabinetDevices::drawingFromParts(QString(), properties, &error));
	PartsDatabase::setEnabled(false);
	QVERIFY(!CabinetDevices::drawingFromParts(cpu, properties, &error));
	PartsDatabase::setConfiguredPath(QString());
}

QET_APP_TEST_MAIN(tst_devicespanel)
#include "tst_devicespanel.moc"
