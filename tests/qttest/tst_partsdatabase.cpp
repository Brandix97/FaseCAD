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
#include "partsdb/partscabletypes.h"
#include "partsdb/partslinks.h"
#include "partsdb/partsmaterial.h"
#include "partsdb/partsdatabase.h"

#include <QFile>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

/**
	The parts database keeps what is written in it, and refuses what would
	break the link between an element and its part:

	 - a part, its technical data, accessories and empty numbers read back
	   as they were written, also after the file is closed and opened again;
	 - two parts cannot share a manufacturer and an order number, whatever
	   the case, but any number of parts may have no order number yet;
	 - an image or a DXF is copied in the database or only linked, and both
	   can be saved back to disk; a STEP model is a file of its own kind;
	 - the functions and the connection points of a part (what each pin
	   does, what wire it takes, where it is) read back in their order,
	   and an older database is upgraded without losing anything;
	 - a database made by a newer program, or kept busy by someone else on
	   a shared folder, gives a clear error instead of being damaged;
	 - the setting is off by default and the default file is a local one.
*/
class tst_partsdatabase : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;

	Part contactor()
	{
		Part part;
		part.manufacturer = QStringLiteral("Acme Electric");
		part.order_number = QStringLiteral("AC-C09P7");
		part.internal_reference = QStringLiteral("INT-0042");
		part.supplier = QStringLiteral("Distrib");
		part.supplier_reference = QStringLiteral("RX-778812");
		part.description = QStringLiteral("Contactor 9 A, coil 230 V AC");
		part.designation = QStringLiteral("Series D");
		part.category = QStringLiteral("Contactors");
		part.width = 45;
		part.height = 77;
		part.depth = 86;
		part.weight = 0.33;
		part.technical_data = {{QStringLiteral("Coil voltage"),
								QStringLiteral("230 V AC")},
							   {QStringLiteral("Poles"), QStringLiteral("3")}};
		return part;
	}

	QString newPath(const QString &name)
	{
		const QString path = m_dir.filePath(name);
		QFile::remove(path);
		return path;
	}

		/// The categories of @p db but Cables, which every database has
	static QList<PartCategory> userCategories(const PartsDatabase &db)
	{
		QList<PartCategory> list;
		int cables = 0;
		for (const PartCategory &category : db.categoryList()) {
			if (PartsDatabase::isDefaultCategory(category.name))
				++cables;
			else
				list << category;
		}
		return cables == 1 ? list : QList<PartCategory>();
	}

private slots:
	void initTestCase()
	{
		QVERIFY(m_dir.isValid());
			//Never touch the settings of the person running the tests
		QSettings::setDefaultFormat(QSettings::IniFormat);
		QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
						   m_dir.filePath(QStringLiteral("settings")));
		QCoreApplication::setOrganizationName(QStringLiteral("QETTest"));
		QCoreApplication::setApplicationName(QStringLiteral("partsdb"));
	}

	void settingIsOffAndLocalByDefault()
	{
		QVERIFY(!PartsDatabase::isEnabled());
		QVERIFY(PartsDatabase::configuredPath().isEmpty());
		QCOMPARE(PartsDatabase::path(), PartsDatabase::defaultPath());

		const QString shared = m_dir.filePath(QStringLiteral("share/parts.sqlite"));
		PartsDatabase::setEnabled(true);
		PartsDatabase::setConfiguredPath(shared);
		QVERIFY(PartsDatabase::isEnabled());
		QCOMPARE(PartsDatabase::path(), shared);

		PartsDatabase::setConfiguredPath(QString());
		PartsDatabase::setEnabled(false);
		QCOMPARE(PartsDatabase::path(), PartsDatabase::defaultPath());
	}

	void partReadsBackAfterReopening()
	{
		const QString path = newPath(QStringLiteral("reopen.sqlite"));
		qint64 id = 0;
		qint64 cover_id = 0;
		{
			PartsDatabase db;
			QString error;
			QVERIFY2(db.open(path, &error), qPrintable(error));
			Part cover;
			cover.manufacturer = QStringLiteral("Acme Electric");
			cover.order_number = QStringLiteral("LAD9ET1");
			cover_id = db.addPart(cover, &error);
			QVERIFY2(cover_id, qPrintable(error));

			Part part = contactor();
			part.accessories = {{cover_id, 2}};
			id = db.addPart(part, &error);
			QVERIFY2(id, qPrintable(error));
		}

		PartsDatabase db;
		QString error;
		QVERIFY2(db.open(path, &error), qPrintable(error));
		const std::optional<Part> part = db.part(id);
		QVERIFY(part);
		const Part expected = contactor();
		QCOMPARE(part->manufacturer, expected.manufacturer);
		QCOMPARE(part->order_number, expected.order_number);
		QCOMPARE(part->internal_reference, expected.internal_reference);
		QCOMPARE(part->supplier_reference, expected.supplier_reference);
		QVERIFY(part->weight == expected.weight);
		QVERIFY(part->depth == expected.depth);
		QVERIFY(part->technical_data == expected.technical_data);
		QVERIFY(part->accessories
				== (QList<PartAccessory>{{cover_id, 2}}));
			//Never filled in: stays empty, not zero
		QVERIFY(!part->price);
		QCOMPARE(db.categories(), QStringList{QStringLiteral("Contactors")});
		QCOMPARE(db.manufacturers(),
				 QStringList{QStringLiteral("Acme Electric")});
	}

	void sameIdentityIsRefused()
	{
		PartsDatabase db;
		QString error;
		QVERIFY2(db.open(newPath(QStringLiteral("identity.sqlite")), &error),
				 qPrintable(error));
		const qint64 id = db.addPart(contactor(), &error);
		QVERIFY2(id, qPrintable(error));

		Part twin = contactor();
		twin.manufacturer = twin.manufacturer.toUpper();
		twin.order_number = QStringLiteral(" ac-c09p7 ");
		QCOMPARE(db.addPart(twin, &error), qint64(0));
		QVERIFY(error.contains(QLatin1String("already exists")));
		QCOMPARE(db.findPart(twin.manufacturer, twin.order_number), id);

			//Parts without an order number yet never collide
		Part draft;
		draft.description = QStringLiteral("To be chosen");
		QVERIFY(db.addPart(draft, &error));
		QVERIFY(db.addPart(draft, &error));
		QCOMPARE(db.findPart(QString(), QString()), qint64(0));

			//Nor can an update take the identity of another part
		Part other;
		other.manufacturer = QStringLiteral("Bolt");
		other.order_number = QStringLiteral("BT-AF09");
		other.id = db.addPart(other, &error);
		QVERIFY(other.id);
		other.manufacturer = contactor().manufacturer;
		other.order_number = contactor().order_number;
		QVERIFY(!db.updatePart(other, &error));
	}

	void updateReplacesDetails()
	{
		PartsDatabase db;
		QString error;
		QVERIFY2(db.open(newPath(QStringLiteral("update.sqlite")), &error),
				 qPrintable(error));
		Part part = contactor();
		part.id = db.addPart(part, &error);
		QVERIFY2(part.id, qPrintable(error));

		part.description = QStringLiteral("Contactor 9 A, coil 24 V DC");
		part.technical_data = {{QStringLiteral("Coil voltage"),
								QStringLiteral("24 V DC")}};
		part.weight.reset();
		QVERIFY2(db.updatePart(part, &error), qPrintable(error));

		const std::optional<Part> read = db.part(part.id);
		QVERIFY(read);
		QCOMPARE(read->description, part.description);
		QVERIFY(read->technical_data == part.technical_data);
		QVERIFY(!read->weight);

		part.accessories = {{part.id, 1}};
		QVERIFY(!db.updatePart(part, &error));
		QVERIFY(error.contains(QLatin1String("own accessory")));
			//The failed update changed nothing
		QVERIFY(db.part(part.id)->technical_data == read->technical_data);
	}

	void removeCleansEverything()
	{
		PartsDatabase db;
		QString error;
		QVERIFY2(db.open(newPath(QStringLiteral("remove.sqlite")), &error),
				 qPrintable(error));
		Part cover;
		cover.order_number = QStringLiteral("COVER");
		const qint64 cover_id = db.addPart(cover, &error);
		Part part = contactor();
		part.accessories = {{cover_id, 1}};
		part.id = db.addPart(part, &error);
		QVERIFY2(part.id && cover_id, qPrintable(error));
		QVERIFY(db.addFile(part.id, PartFileKind::Image,
						   QStringLiteral("front.png"), "png", &error));

		QVERIFY2(db.removePart(cover_id, &error), qPrintable(error));
		QVERIFY(db.part(part.id)->accessories.isEmpty());

		QVERIFY2(db.removePart(part.id, &error), qPrintable(error));
		QVERIFY(!db.part(part.id));
		QVERIFY(db.files(part.id).isEmpty());
	}

	void searchNeedsEveryWord()
	{
		PartsDatabase db;
		QString error;
		QVERIFY2(db.open(newPath(QStringLiteral("search.sqlite")), &error),
				 qPrintable(error));
		QVERIFY(db.addPart(contactor(), &error));
		Part breaker;
		breaker.manufacturer = QStringLiteral("Bolt");
		breaker.order_number = QStringLiteral("BT-C10");
		breaker.description = QStringLiteral("Circuit breaker 10 A");
		QVERIFY(db.addPart(breaker, &error));
		Part percent;
		percent.order_number = QStringLiteral("X100%");
		QVERIFY(db.addPart(percent, &error));

		QCOMPARE(db.parts().size(), 3);
		QCOMPARE(db.parts(QStringLiteral("contactor 230")).size(), 1);
		QCOMPARE(db.parts(QStringLiteral("rx-778812")).size(), 1);
		QCOMPARE(db.parts(QStringLiteral("contactor Bolt")).size(), 0);
			//A % typed by the user is a character, not a wildcard
		QCOMPARE(db.parts(QStringLiteral("%")).size(), 1);
		QCOMPARE(db.parts().first().manufacturer, QString());
	}

	void filesAreCopiedOrLinked()
	{
		PartsDatabase db;
		QString error;
		QVERIFY2(db.open(newPath(QStringLiteral("files.sqlite")), &error),
				 qPrintable(error));
		const qint64 id = db.addPart(contactor(), &error);
		QVERIFY2(id, qPrintable(error));

		const QString dxf_path = m_dir.filePath(QStringLiteral("front.dxf"));
		QFile dxf(dxf_path);
		QVERIFY(dxf.open(QIODevice::WriteOnly));
		dxf.write("0\nSECTION\n2\nENTITIES\n0\nENDSEC\n0\nEOF\n");
		dxf.close();

		const qint64 copied = db.addFileFromDisk(id, PartFileKind::Dxf,
												 dxf_path, &error);
		QVERIFY2(copied, qPrintable(error));
		const qint64 linked = db.linkFile(id, PartFileKind::Dxf, dxf_path,
										  &error);
		QVERIFY2(linked, qPrintable(error));
		const qint64 image = db.addFile(id, PartFileKind::Image,
										QStringLiteral("photo.png"),
										QByteArray("\x89PNG", 4), &error);
		QVERIFY2(image, qPrintable(error));

		const QList<PartFile> files = db.files(id);
		QCOMPARE(files.size(), 3);
		QVERIFY(files.first().kind == PartFileKind::Image);
		QVERIFY(!files.at(1).isLinked());
		QVERIFY(files.at(2).isLinked());
		QCOMPARE(files.at(1).file_name, QStringLiteral("front.dxf"));

			//The copy no longer needs the original file
		QFile::remove(dxf_path);
		QCOMPARE(db.fileContent(copied),
				 QByteArray("0\nSECTION\n2\nENTITIES\n0\nENDSEC\n0\nEOF\n"));
		QVERIFY(db.fileContent(linked, &error).isEmpty());
		QVERIFY(error.contains(QLatin1String("cannot be found")));

		const QString out = m_dir.filePath(QStringLiteral("saved.dxf"));
		QVERIFY2(db.saveFileAs(copied, out, &error), qPrintable(error));
		QFile saved(out);
		QVERIFY(saved.open(QIODevice::ReadOnly));
		QCOMPARE(saved.readAll(), db.fileContent(copied));

		QVERIFY(db.removeFile(image, &error));
		QCOMPARE(db.files(id).size(), 2);

		QVERIFY(!db.addFile(id + 1000, PartFileKind::Image,
							QStringLiteral("orphan.png"), "x", &error));
		QVERIFY(error.contains(QLatin1String("no longer exists")));
	}

	void newerDatabaseIsRefused()
	{
		const QString path = newPath(QStringLiteral("newer.sqlite"));
		{
			PartsDatabase db;
			QVERIFY(db.open(path));
		}
		{
			QSqlDatabase raw = QSqlDatabase::addDatabase(
						QStringLiteral("QSQLITE"), QStringLiteral("raw"));
			raw.setDatabaseName(path);
			QVERIFY(raw.open());
			QSqlQuery query(raw);
			QVERIFY(query.exec(QStringLiteral(
					"UPDATE meta SET value = '99' "
					"WHERE key = 'schema_version'")));
			raw.close();
		}
		QSqlDatabase::removeDatabase(QStringLiteral("raw"));

		PartsDatabase db;
		QString error;
		QVERIFY(!db.open(path, &error));
		QVERIFY(error.contains(QLatin1String("newer version")));
		QVERIFY(!db.isOpen());
	}

	void cableFieldsReadBack()
	{
		PartsDatabase db;
		QString error;
		QVERIFY2(db.open(newPath(QStringLiteral("cable.sqlite")), &error),
				 qPrintable(error));
		Part cable;
		cable.manufacturer = QStringLiteral("Wireco");
		cable.order_number = QStringLiteral("0012345");
		cable.designation = QStringLiteral("H07RN-F 5G2,5");
		cable.core_count = 5;
		cable.core_colors = QStringLiteral("BK|BN|GY|BU|GNYE");
		cable.section = QStringLiteral("2,5");
		cable.sheath_color = QStringLiteral("Black");
		const qint64 id = db.addPart(cable, &error);
		QVERIFY2(id, qPrintable(error));

		const std::optional<Part> read = db.part(id);
		QVERIFY(read);
		QCOMPARE(read->core_count, 5);
		QCOMPARE(read->core_colors, cable.core_colors);
		QCOMPARE(read->section, cable.section);
		QCOMPARE(read->sheath_color, cable.sheath_color);
			//The list of parts carries them too: the cable types are
			//made from it
		QCOMPARE(db.parts().first().core_count, 5);
	}

	void versionOneIsUpgraded()
	{
		const QString path = newPath(QStringLiteral("v1.sqlite"));
		{
			QSqlDatabase raw = QSqlDatabase::addDatabase(
						QStringLiteral("QSQLITE"), QStringLiteral("v1"));
			raw.setDatabaseName(path);
			QVERIFY(raw.open());
			QSqlQuery query(raw);
				//The tables as version 1 wrote them
			QVERIFY(query.exec(QStringLiteral(
				"CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT)")));
			QVERIFY(query.exec(QStringLiteral(
				"CREATE TABLE part (id INTEGER PRIMARY KEY, "
				"manufacturer TEXT NOT NULL DEFAULT '', "
				"order_number TEXT NOT NULL DEFAULT '', "
				"internal_reference TEXT NOT NULL DEFAULT '', "
				"supplier TEXT NOT NULL DEFAULT '', "
				"supplier_reference TEXT NOT NULL DEFAULT '', "
				"description TEXT NOT NULL DEFAULT '', "
				"designation TEXT NOT NULL DEFAULT '', "
				"category TEXT NOT NULL DEFAULT '', "
				"model TEXT NOT NULL DEFAULT '', "
				"voltage TEXT NOT NULL DEFAULT '', "
				"current TEXT NOT NULL DEFAULT '', "
				"unit TEXT NOT NULL DEFAULT '', "
				"datasheet_url TEXT NOT NULL DEFAULT '', "
				"notes TEXT NOT NULL DEFAULT '', "
				"width REAL, height REAL, depth REAL, weight REAL, price REAL, "
				"created TEXT NOT NULL, modified TEXT NOT NULL)")));
			QVERIFY(query.exec(QStringLiteral(
				"CREATE TABLE part_data (part_id INTEGER NOT NULL, "
				"position INTEGER NOT NULL, key TEXT NOT NULL, "
				"value TEXT NOT NULL DEFAULT '', PRIMARY KEY (part_id, position))")));
			QVERIFY(query.exec(QStringLiteral(
				"CREATE TABLE part_accessory (part_id INTEGER NOT NULL, "
				"position INTEGER NOT NULL, accessory_id INTEGER NOT NULL, "
				"quantity REAL NOT NULL DEFAULT 1, PRIMARY KEY (part_id, position))")));
			QVERIFY(query.exec(QStringLiteral(
				"CREATE TABLE part_file (id INTEGER PRIMARY KEY, "
				"part_id INTEGER NOT NULL, kind TEXT NOT NULL, "
				"file_name TEXT NOT NULL, linked_path TEXT NOT NULL DEFAULT '', "
				"content BLOB, size INTEGER NOT NULL DEFAULT 0, added TEXT NOT NULL)")));
			QVERIFY(query.exec(QStringLiteral(
				"INSERT INTO meta VALUES ('schema_version', '1')")));
			QVERIFY(query.exec(QStringLiteral(
				"INSERT INTO part (manufacturer, order_number, weight, created, modified) "
				"VALUES ('Bolt', 'BT-C10', 0.12, '2026-11-07', '2026-11-07')")));
			raw.close();
		}
		QSqlDatabase::removeDatabase(QStringLiteral("v1"));

		PartsDatabase db;
		QString error;
		QVERIFY2(db.open(path, &error), qPrintable(error));
		const QList<Part> parts = db.parts();
		QCOMPARE(parts.size(), 1);
		QCOMPARE(parts.first().order_number, QStringLiteral("BT-C10"));
		QVERIFY(parts.first().weight == std::optional<double>(0.12));
		QCOMPARE(parts.first().core_count, 0);

		Part cable = *db.part(parts.first().id);
		cable.core_count = 3;
		QVERIFY2(db.updatePart(cable, &error), qPrintable(error));
		QCOMPARE(db.part(cable.id)->core_count, 3);
		db.close();

			//Upgraded once and for all
		PartsDatabase again;
		QVERIFY2(again.open(path, &error), qPrintable(error));
		QCOMPARE(again.parts().first().core_count, 3);
	}

	void partsWithCoresAreCableTypes()
	{
		Part cable;
		cable.manufacturer = QStringLiteral("Wireco");
		cable.order_number = QStringLiteral("0012345");
		cable.designation = QStringLiteral("H07RN-F 5G2,5");
		cable.core_count = 5;
		cable.core_colors = QStringLiteral(" BK; BN,GY |BU|GNYE ");
		Part breaker;
		breaker.order_number = QStringLiteral("BT-C10");
		Part unnamed;
		unnamed.core_count = 2;
		Part by_order;
		by_order.order_number = QStringLiteral("CBL-7");
		by_order.core_count = 7;

		const QList<CableTypeRecord> records = PartsCableTypes::records(
					{cable, breaker, unnamed, by_order});
			//Only parts with cores and a name
		QCOMPARE(records.size(), 2);
		const CableTypeRecord &first = records.first();
		QCOMPARE(first.value(QStringLiteral("designation")),
				 QStringLiteral("H07RN-F 5G2,5"));
		QCOMPARE(first.value(QStringLiteral("cores")), QStringLiteral("5"));
		QCOMPARE(first.value(QStringLiteral("core_colors")),
				 QStringLiteral("BK|BN|GY|BU|GNYE"));
		QCOMPARE(first.value(QStringLiteral("manufacturer_reference")),
				 QStringLiteral("0012345"));
		QVERIFY(PartsCableTypes::isFromPartsDatabase(first));
			//No article number: the order number names the type
		QCOMPARE(records.at(1).value(QStringLiteral("designation")),
				 QStringLiteral("CBL-7"));
		QVERIFY(!PartsCableTypes::isFromPartsDatabase(CableTypeRecord()));
	}

	void partFillsTheElementInformations()
	{
		Part part = contactor();
		part.model = QStringLiteral("D09");
		const MaterialRecord main = PartsMaterial::record(part);
		QCOMPARE(main.value(QStringLiteral("manufacturer")), part.manufacturer);
		QCOMPARE(main.value(QStringLiteral("manufacturer_reference")),
				 part.order_number);
		QCOMPARE(main.value(QStringLiteral("machine_manufacturer_reference")),
				 part.internal_reference);
		QCOMPARE(main.value(QStringLiteral("width")), QStringLiteral("45"));
			//An empty article field is written too: the article picked
			//before must not linger
		QVERIFY(main.values.contains(QStringLiteral("current_rating")));
		QVERIFY(main.value(QStringLiteral("current_rating")).isEmpty());
			//The quantity is the element's own, unless an accessory says it
		QVERIFY(!main.values.contains(QStringLiteral("quantity")));

		const MaterialRecord accessory = PartsMaterial::record(part, 2);
		QCOMPARE(accessory.value(QStringLiteral("quantity")), QStringLiteral("2"));
			//The same entry lands in an auxiliary block under its own keys
		QCOMPARE(MaterialList::elementInfoKey(QStringLiteral("manufacturer"), 2),
				 QStringLiteral("manufacturer_auxiliary2"));
		QVERIFY(MaterialList::columnsForBlock(2).contains(QStringLiteral("quantity")));
	}

	void elementPartsReadBack()
	{
		const QList<PartsLinks::Link> links = {
			{QStringLiteral("Acme Electric"), QStringLiteral("AC-C09P7"), 1},
			{QStringLiteral("Wire\tWorks"), QStringLiteral("0012345"), 12.5}};
		const QString value = PartsLinks::encode(links);
		const QList<PartsLinks::Link> read = PartsLinks::decode(value);
		QCOMPARE(read.size(), 2);
		QVERIFY(read.first() == links.first());
			//A tab typed in a field does not break the line
		QCOMPARE(read.at(1).manufacturer, QStringLiteral("Wire Works"));
		QCOMPARE(read.at(1).units, 12.5);
			//A line without an order number says nothing, units which are
			//no number count one
		const QList<PartsLinks::Link> odd = PartsLinks::decode(
					QStringLiteral("Bolt\t\t3\nBolt\tBT\tmany"));
		QCOMPARE(odd.size(), 1);
		QCOMPARE(odd.first().units, 1.0);
		QVERIFY(PartsLinks::decode(QString()).isEmpty());
	}

		///A PLC as its manufacturer describes it : a few functions and
		///connection points of a compact CPU.
	Part plc()
	{
		Part part;
		part.manufacturer = QStringLiteral("ACME");
		part.order_number = QStringLiteral("PLC-1214");
		part.description = QStringLiteral("Compact PLC, 24 V DC, 14 DI / 10 DQ / 2 AI");
		part.gtin = QStringLiteral("0000000012147");
		part.code_letter = QStringLiteral("K");
		part.width = 100;
		part.height = 90;
		part.depth = 70;
		part.power_loss = 10.5;
		part.clearance_top = 20;
		part.clearance_bottom = 20;
		part.clearance_front = 20;
		part.functions = {
			{PartFunctionKind::SupplyPlus, QStringLiteral("X10"),
			 QStringLiteral("1"), QStringLiteral("L+"), QString()},
			{PartFunctionKind::SupplyMinus, QStringLiteral("X10"),
			 QStringLiteral("2"), QStringLiteral("M"), QString()},
			{PartFunctionKind::DigitalInput, QStringLiteral("X10"),
			 QStringLiteral("7"), QStringLiteral("DI a.0"),
			 QStringLiteral("I0.0")},
			{PartFunctionKind::BusPort, QStringLiteral("X1"),
			 QStringLiteral("P1"), QStringLiteral("PROFINET"), QString()}};
		PartConnection l_plus;
		l_plus.plug = QStringLiteral("X10");
		l_plus.pin = QStringLiteral("1");
		l_plus.section_min = 0.5;
		l_plus.section_max = 2.5;
		l_plus.max_wires = 2;
		l_plus.strip_length = 7;
		l_plus.torque = 0.5;
		l_plus.screwdriver = QStringLiteral("3 mm");
		l_plus.x = 5;
		l_plus.y = 80;
		l_plus.z = 40;
		l_plus.direction = QStringLiteral("0 1 0");
		PartConnection m = l_plus;
		m.pin = QStringLiteral("2");
		m.x = 9;
		m.max_wires.reset();
		part.connections = {l_plus, m};
		return part;
	}

	void functionsAndConnectionsReadBack()
	{
		const QString path = newPath(QStringLiteral("functions.sqlite"));
		qint64 id = 0;
		{
			PartsDatabase db;
			QString error;
			QVERIFY2(db.open(path, &error), qPrintable(error));
			id = db.addPart(plc(), &error);
			QVERIFY2(id, qPrintable(error));
		}

		PartsDatabase db;
		QString error;
		QVERIFY2(db.open(path, &error), qPrintable(error));
		Part read = *db.part(id);
		const Part expected = plc();
		QCOMPARE(read.gtin, expected.gtin);
		QCOMPARE(read.code_letter, expected.code_letter);
		QVERIFY(read.power_loss == expected.power_loss);
		QVERIFY(read.clearance_top == expected.clearance_top);
		QVERIFY(read.clearance_front == expected.clearance_front);
			//Never filled in: stays empty, not zero
		QVERIFY(!read.clearance_left);
		QVERIFY(read.functions == expected.functions);
		QVERIFY(read.connections == expected.connections);
		QVERIFY(!read.connections.at(1).max_wires);
			//The article code finds the part
		QCOMPARE(db.parts(QStringLiteral("0000000012147")).size(), 1);

			//An update replaces them, in the new order
		read.functions = {expected.functions.at(2), expected.functions.at(0)};
		read.connections.removeFirst();
		QVERIFY2(db.updatePart(read, &error), qPrintable(error));
		const Part updated = *db.part(id);
		QVERIFY(updated.functions == read.functions);
		QCOMPARE(updated.connections.size(), 1);
		QCOMPARE(updated.connections.first().pin, QStringLiteral("2"));

			//And they go with the part
		QVERIFY2(db.removePart(id, &error), qPrintable(error));
		QVERIFY(!db.part(id));
		{
			QSqlDatabase raw = QSqlDatabase::addDatabase(
						QStringLiteral("QSQLITE"), QStringLiteral("count"));
			raw.setDatabaseName(path);
			QVERIFY(raw.open());
			QSqlQuery query(raw);
			for (const char *table : {"part_function", "part_connection"}) {
				QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM %1")
								   .arg(QLatin1String(table))));
				QVERIFY(query.next());
				QCOMPARE(query.value(0).toInt(), 0);
			}
			query.finish();
			raw.close();
		}
		QSqlDatabase::removeDatabase(QStringLiteral("count"));
	}

	void functionKindsKeepTheirIds()
	{
			//Written in the databases of the users: never renamed
		QCOMPARE(PartFunction::kindId(PartFunctionKind::DigitalInput),
				 QStringLiteral("digital_input"));
		QCOMPARE(PartFunction::kindId(PartFunctionKind::SupplyPlus),
				 QStringLiteral("supply_plus"));
		for (const PartFunctionKind kind : PartFunction::kinds()) {
			QCOMPARE(PartFunction::kindFromId(PartFunction::kindId(kind)),
					 kind);
			QVERIFY(!PartFunction::kindName(kind).isEmpty());
		}
			//An id of a newer version is read as Other
		QCOMPARE(PartFunction::kindFromId(QStringLiteral("hydraulic_valve")),
				 PartFunctionKind::Other);
	}

	void stepModelIsAFileOfItsOwn()
	{
		PartsDatabase db;
		QString error;
		QVERIFY2(db.open(newPath(QStringLiteral("step.sqlite")), &error),
				 qPrintable(error));
		const qint64 id = db.addPart(plc(), &error);
		QVERIFY2(id, qPrintable(error));
		QVERIFY(db.addFile(id, PartFileKind::Step, QStringLiteral("cpu.stp"),
						   "ISO-10303-21;", &error));
		QVERIFY(db.addFile(id, PartFileKind::Dxf, QStringLiteral("cpu.dxf"),
						   "0\nEOF\n", &error));
		QVERIFY(db.addFile(id, PartFileKind::Image, QStringLiteral("cpu.png"),
						   "png", &error));
			//Image first, then the drawing, then the 3D model
		const QList<PartFile> files = db.files(id);
		QCOMPARE(files.size(), 3);
		QVERIFY(files.at(0).kind == PartFileKind::Image);
		QVERIFY(files.at(1).kind == PartFileKind::Dxf);
		QVERIFY(files.at(2).kind == PartFileKind::Step);
		QCOMPARE(db.fileContent(files.at(2).id), QByteArray("ISO-10303-21;"));
	}

	void maximumCurrentTranslationsAndCables()
	{
			//Brandon, 2026-11-27
		const QString path = m_dir.filePath(QStringLiteral("v6.sqlite"));
		PartsDatabase db;
		QString error;
		QVERIFY2(db.open(path, &error), qPrintable(error));
		Part part;
		part.manufacturer = QStringLiteral("Acme");
		part.order_number = QStringLiteral("MC-1");
		part.description = QStringLiteral("Contactor tripolar");
		part.model = QStringLiteral("Serie D");
		part.max_current = 25;
		PartTranslations::setTranslation(part, QStringLiteral("description"), QStringLiteral("en"),
										 QStringLiteral("Three-pole contactor"));
		PartTranslations::setTranslation(part, QStringLiteral("model"), QStringLiteral("de"),
										 QStringLiteral("Baureihe D"));
		const qint64 id = db.addPart(part, &error);
		QVERIFY2(id > 0, qPrintable(error));
		const Part read = *db.part(id);
		QVERIFY(read.max_current == std::optional<double>(25));
		QCOMPARE(PartTranslations::text(read, QStringLiteral("description"), QStringLiteral("en")),
				 QStringLiteral("Three-pole contactor"));
		QCOMPARE(PartTranslations::text(read, QStringLiteral("description"), QStringLiteral("fr")),
				 QStringLiteral("Contactor tripolar"));
		QCOMPARE(PartTranslations::text(read, QStringLiteral("model"), QStringLiteral("de")),
				 QStringLiteral("Baureihe D"));
			//Taken off again: nothing left
		Part cleared = read;
		PartTranslations::setTranslation(cleared, QStringLiteral("description"), QStringLiteral("en"), QString());
		PartTranslations::setTranslation(cleared, QStringLiteral("model"), QStringLiteral("de"), QString());
		QVERIFY(cleared.translations.isEmpty());

			//Cables is there, and stays
		bool cables = false;
		for (const PartCategory &category : db.categoryList())
			cables = cables || category.name == PartsDatabase::cablesCategory();
		QVERIFY(cables);
		QVERIFY(!db.removeCategory(QStringLiteral("cables"), &error));
		QVERIFY(!db.renameCategory(QStringLiteral("Cables"), QStringLiteral("Wires"), &error));
		QVERIFY(db.addCategory(QStringLiteral("Cables › Power"), &error));
		QVERIFY2(db.removeCategory(QStringLiteral("Cables › Power"), &error), qPrintable(error));
		db.close();
		QVERIFY(db.open(path, &error));
		int count = 0;
		for (const PartCategory &category : db.categoryList())
			count += PartsDatabase::isDefaultCategory(category.name) ? 1 : 0;
		QCOMPARE(count, 1);
	}

	void categoriesHaveIconsAndTakeTheirParts()
	{
			//The categories of the parts manager (Brandon, 2026-11-21): made
			//by the user or had by a part, with an icon; renamed or removed
			//with their parts
		const QString path = newPath(QStringLiteral("categories.sqlite"));
		PartsDatabase db;
		QString error;
		QVERIFY2(db.open(path, &error), qPrintable(error));
		const qint64 id = db.addPart(contactor(), &error);
		QVERIFY2(id > 0, qPrintable(error));
		QVERIFY2(db.addCategory(QStringLiteral("Relays"), &error), qPrintable(error));
		QVERIFY(!db.addCategory(QStringLiteral("relays"), &error));
		QVERIFY(!db.addCategory(QStringLiteral("  "), &error));
		QList<PartCategory> list = userCategories(db);
		QCOMPARE(list.size(), 2);
		QCOMPARE(list.at(0).name, QStringLiteral("Contactors"));
		QCOMPARE(list.at(1).name, QStringLiteral("Relays"));
		QVERIFY(list.at(0).icon.isEmpty());

		const QByteArray svg("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"8\" height=\"8\"/>");
		QVERIFY2(db.setCategoryIcon(QStringLiteral("Contactors"), svg, &error), qPrintable(error));
		QCOMPARE(userCategories(db).at(0).icon, svg);
		QCOMPARE(db.partsInCategory(QStringLiteral("contactors")), 1);

			//Renamed: its icon and its part go with it
		QVERIFY2(db.renameCategory(QStringLiteral("Contactors"),
								   QStringLiteral("Power contactors"), &error), qPrintable(error));
		QCOMPARE(db.part(id)->category, QStringLiteral("Power contactors"));
		list = userCategories(db);
		QCOMPARE(list.size(), 2);
		QCOMPARE(list.at(0).name, QStringLiteral("Power contactors"));
		QCOMPARE(list.at(0).icon, svg);
		QVERIFY(!db.renameCategory(QStringLiteral("Power contactors"),
								   QStringLiteral("RELAYS"), &error));

			//Removed: its part has no category any more
		QVERIFY2(db.removeCategory(QStringLiteral("Power contactors"), &error), qPrintable(error));
		QVERIFY(db.part(id)->category.isEmpty());
		QCOMPARE(userCategories(db).size(), 1);
		db.close();

			//Still version 5: the FaseCAD before opens it too
		{
			QSqlDatabase raw = QSqlDatabase::addDatabase(
						QStringLiteral("QSQLITE"), QStringLiteral("categories"));
			raw.setDatabaseName(path);
			QVERIFY(raw.open());
			QSqlQuery query(raw);
			QVERIFY(query.exec(QStringLiteral(
					"SELECT value FROM meta WHERE key = 'schema_version'")));
			QVERIFY(query.next());
			QCOMPARE(query.value(0).toInt(), PartsDatabase::schemaVersion());
			query.finish();
			raw.close();
		}
		QSqlDatabase::removeDatabase(QStringLiteral("categories"));
	}

	void subcategoriesAreKeptAsPaths()
	{
			//A category inside another (Brandon, 2026-11-21): its path
		const QString sep = PartCategoryPath::separator();
		const QString relays = QStringLiteral("Relays");
		const QString interface = relays + sep + QStringLiteral("Interface");
		QCOMPARE(PartCategoryPath::normalized(QStringLiteral(" Relays ") + sep.trimmed()
											  + QStringLiteral("Interface ")), interface);
		QVERIFY(PartCategoryPath::isWithin(interface, QStringLiteral("relays")));
		QVERIFY(PartCategoryPath::isWithin(relays, relays));
		QVERIFY(!PartCategoryPath::isWithin(QStringLiteral("Relays2"), relays));
		QCOMPARE(PartCategoryPath::parent(interface), relays);
		QCOMPARE(PartCategoryPath::leaf(interface), QStringLiteral("Interface"));
		QVERIFY(PartCategoryPath::less(relays, interface));
		QVERIFY(PartCategoryPath::less(interface, QStringLiteral("Relays2")));

		PartsDatabase db;
		QString error;
		QVERIFY2(db.open(newPath(QStringLiteral("subcategories.sqlite")), &error), qPrintable(error));
		QVERIFY2(db.addCategory(interface, &error), qPrintable(error));
		QList<PartCategory> list = userCategories(db);
		QCOMPARE(list.size(), 2);
		QCOMPARE(list.at(0).name, relays);
		QCOMPARE(list.at(1).name, interface);

		Part inner = contactor();
		inner.category = interface;
		const qint64 inner_id = db.addPart(inner, &error);
		QVERIFY2(inner_id > 0, qPrintable(error));
		Part outer = contactor();
		outer.order_number = QStringLiteral("ACME-C12");
		outer.category = relays;
		const qint64 outer_id = db.addPart(outer, &error);
		QVERIFY2(outer_id > 0, qPrintable(error));
		QCOMPARE(db.partsInCategory(relays), 1);
		QCOMPARE(db.partsInCategory(relays, true), 2);
		const QByteArray svg("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"8\" height=\"8\"/>");
		QVERIFY(db.setCategoryIcon(interface, svg));

			//Renamed: its subcategories, their parts and icons with it
		QHash<QString, QString> moved;
		const QString relais = QStringLiteral("Relais");
		const QString relais_interface = relais + sep + QStringLiteral("Interface");
		QVERIFY2(db.renameCategory(relays, relais, &error, &moved), qPrintable(error));
		QCOMPARE(db.part(inner_id)->category, relais_interface);
		QCOMPARE(db.part(outer_id)->category, relais);
		QCOMPARE(moved.value(interface), relais_interface);
		list = userCategories(db);
		QCOMPARE(list.size(), 2);
		QCOMPARE(list.at(1).name, relais_interface);
		QCOMPARE(list.at(1).icon, svg);

			//Removed: its parts to the category it is in (none at the
			//top), its subcategories up one level with their parts
		QVERIFY2(db.removeCategory(relais, &error, &moved), qPrintable(error));
		QCOMPARE(db.part(inner_id)->category, QStringLiteral("Interface"));
		QVERIFY(db.part(outer_id)->category.isEmpty());
		QCOMPARE(moved.value(relais_interface), QStringLiteral("Interface"));
		list = userCategories(db);
		QCOMPARE(list.size(), 1);
		QCOMPARE(list.at(0).name, QStringLiteral("Interface"));
		QCOMPARE(list.at(0).icon, svg);

			//Never inside itself
		QVERIFY(db.addCategory(QStringLiteral("Interface") + sep + QStringLiteral("Small")));
		QVERIFY(!db.renameCategory(QStringLiteral("Interface"),
								   QStringLiteral("Interface") + sep + QStringLiteral("Small")
								   + sep + QStringLiteral("X"), &error));
	}

	void versionTwoIsUpgraded()
	{
		const QString path = newPath(QStringLiteral("v2.sqlite"));
		qint64 cable_id = 0;
		{
			PartsDatabase db;
			QString error;
			QVERIFY2(db.open(path, &error), qPrintable(error));
			Part cable;
			cable.manufacturer = QStringLiteral("Wireco");
			cable.order_number = QStringLiteral("0012345");
			cable.core_count = 5;
			cable.technical_data = {{QStringLiteral("Sheath"),
									 QStringLiteral("PVC")}};
			cable_id = db.addPart(cable, &error);
			QVERIFY2(cable_id, qPrintable(error));
		}
		{
				//Take the file back to what version 2 wrote
			QSqlDatabase raw = QSqlDatabase::addDatabase(
						QStringLiteral("QSQLITE"), QStringLiteral("v2"));
			raw.setDatabaseName(path);
			QVERIFY(raw.open());
			QSqlQuery query(raw);
			QVERIFY(query.exec(QStringLiteral("DROP TABLE part_function")));
			QVERIFY(query.exec(QStringLiteral("DROP TABLE part_connection")));
			QVERIFY(query.exec(QStringLiteral("DROP TABLE part_symbol")));
			for (const char *column : {"gtin", "code_letter",
									   "power_loss", "max_current", "translations",
									   "clearance_top", "clearance_bottom",
									   "clearance_left", "clearance_right",
									   "clearance_front", "clearance_rear"}) {
				QVERIFY2(query.exec(QStringLiteral(
						"ALTER TABLE part DROP COLUMN %1")
						.arg(QLatin1String(column))),
						 qPrintable(query.lastError().text()));
			}
			QVERIFY(query.exec(QStringLiteral(
					"UPDATE meta SET value = '2' "
					"WHERE key = 'schema_version'")));
			raw.close();
		}
		QSqlDatabase::removeDatabase(QStringLiteral("v2"));

		PartsDatabase db;
		QString error;
		QVERIFY2(db.open(path, &error), qPrintable(error));
		Part cable = *db.part(cable_id);
		QCOMPARE(cable.core_count, 5);
		QCOMPARE(cable.technical_data.size(), 1);
		QVERIFY(cable.gtin.isEmpty());
		QVERIFY(!cable.power_loss);
		QVERIFY(cable.functions.isEmpty());

		cable.code_letter = QStringLiteral("W");
		cable.functions = {{PartFunctionKind::Terminal, QString(),
							QStringLiteral("1"), QStringLiteral("BK"),
							QString()}};
		QVERIFY2(db.updatePart(cable, &error), qPrintable(error));
		QCOMPARE(db.part(cable_id)->code_letter, QStringLiteral("W"));
		QCOMPARE(db.part(cable_id)->functions.size(), 1);
	}

		//The symbols a part is placed with (version 5): kept in their
		//order; the one symbol of a version 4 becomes the first
	void symbolsAreKeptAndVersionFourUpgraded()
	{
		const QString path = newPath(QStringLiteral("v4.sqlite"));
		const QString changeover = QStringLiteral(
				"common://10_electric/05_iec_60617_cuadros/01_contactos/"
				"contacto_conmutado.elmt");
		const QString coil = QStringLiteral(
				"common://10_electric/05_iec_60617_cuadros/02_bobinas/bobina.elmt");
		qint64 id = 0;
		{
			PartsDatabase db;
			QString error;
			QVERIFY2(db.open(path, &error), qPrintable(error));
			Part part = plc();
			part.symbols = {coil, changeover};
			id = db.addPart(part, &error);
			QVERIFY2(id, qPrintable(error));
			QCOMPARE(db.part(id)->symbols, (QStringList{coil, changeover}));
			part = *db.part(id);
			part.symbols = {changeover};
			QVERIFY2(db.updatePart(part, &error), qPrintable(error));
			QCOMPARE(db.part(id)->symbols, QStringList{changeover});
		}
		{
				//Take the file back to what version 4 wrote: one symbol
				//in a column of the part
			QSqlDatabase raw = QSqlDatabase::addDatabase(
						QStringLiteral("QSQLITE"), QStringLiteral("v4"));
			raw.setDatabaseName(path);
			QVERIFY(raw.open());
			QSqlQuery query(raw);
			QVERIFY(query.exec(QStringLiteral("DROP TABLE part_symbol")));
			QVERIFY(query.exec(QStringLiteral("ALTER TABLE part DROP COLUMN max_current")));
			QVERIFY(query.exec(QStringLiteral("ALTER TABLE part DROP COLUMN translations")));
			QVERIFY2(query.exec(QStringLiteral(
					"ALTER TABLE part ADD COLUMN symbol TEXT NOT NULL DEFAULT ''")),
					 qPrintable(query.lastError().text()));
			QVERIFY(query.exec(QStringLiteral("UPDATE part SET symbol = '%1'")
							   .arg(coil)));
			QVERIFY(query.exec(QStringLiteral(
					"UPDATE meta SET value = '4' WHERE key = 'schema_version'")));
			raw.close();
		}
		QSqlDatabase::removeDatabase(QStringLiteral("v4"));

		PartsDatabase db;
		QString error;
		QVERIFY2(db.open(path, &error), qPrintable(error));
		Part part = *db.part(id);
		QCOMPARE(part.symbols, QStringList{coil});
		QCOMPARE(part.functions.size(), plc().functions.size());
		part.symbols << changeover;
		QVERIFY2(db.updatePart(part, &error), qPrintable(error));
		QCOMPARE(db.part(id)->symbols, (QStringList{coil, changeover}));
	}

	void busyDatabaseSaysSo()
	{
		const QString path = newPath(QStringLiteral("busy.sqlite"));
		PartsDatabase db;
		QString error;
		QVERIFY2(db.open(path, &error), qPrintable(error));

			//Someone else on the shared folder is writing
		{
			QSqlDatabase other = QSqlDatabase::addDatabase(
						QStringLiteral("QSQLITE"), QStringLiteral("other"));
			other.setDatabaseName(path);
			QVERIFY(other.open());
			QSqlQuery query(other);
			QVERIFY(query.exec(QStringLiteral("BEGIN IMMEDIATE")));

			QCOMPARE(db.addPart(contactor(), &error), qint64(0));
			QVERIFY2(error.contains(QLatin1String("someone else")),
					 qPrintable(error));

			QVERIFY(query.exec(QStringLiteral("ROLLBACK")));
			other.close();
		}
		QSqlDatabase::removeDatabase(QStringLiteral("other"));

		error.clear();
		QVERIFY2(db.addPart(contactor(), &error), qPrintable(error));
	}

	void aServerHoldsThePartsToo()
	{
			//A PostgreSQL server (Brandon, 2026-11-25), when one is given
			//for the tests: FASECAD_TEST_PGSERVER=postgresql://user@host:port/db,
			//a database the tests may make others in
		const QString given = qEnvironmentVariable("FASECAD_TEST_PGSERVER");
		if (given.isEmpty()) QSKIP("No PostgreSQL server given (FASECAD_TEST_PGSERVER)");
		const PartsDatabase::Server admin = PartsDatabase::Server::fromUrl(given);
		const QString name = QStringLiteral("fasecad_test_")
				+ QUuid::createUuid().toString(QUuid::Id128).left(10);
		auto run = [&admin](const QString &statement) {
			bool ok = false;
			{
				QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QPSQL"), QStringLiteral("admin"));
				db.setHostName(admin.host);
				db.setPort(admin.port);
				db.setDatabaseName(admin.database);
				db.setUserName(admin.user);
				db.setPassword(admin.password);
				ok = db.open() && QSqlQuery(db).exec(statement);
				db.close();
			}
			QSqlDatabase::removeDatabase(QStringLiteral("admin"));
			return ok;
		};
		QVERIFY(run(QStringLiteral("CREATE DATABASE %1").arg(name)));
		PartsDatabase::Server server = admin;
		server.database = name;
		{
			PartsDatabase db;
			QString error;
			QVERIFY2(db.openServer(server, &error), qPrintable(error));
			QVERIFY(db.isServer());
			const qint64 id = db.addPart(contactor(), &error);
			QVERIFY2(id > 0, qPrintable(error));
				//The case is not minded, as in the file
			QCOMPARE(db.findPart(contactor().manufacturer.toUpper(),
								 contactor().order_number.toLower()), id);
			QCOMPARE(db.parts(QStringLiteral("CONTACTOR 230")).size(), 1);
			Part twin = contactor();
			twin.manufacturer = twin.manufacturer.toLower();
			QCOMPARE(db.addPart(twin, &error), qint64(0));
			const std::optional<Part> read = db.part(id);
			QVERIFY(read);
			QCOMPARE(read->width, contactor().width);
			QCOMPARE(read->weight, contactor().weight);
				//A category is one whatever its case, as in the file
			QVERIFY2(db.addCategory(QStringLiteral("Relays"), &error), qPrintable(error));
			QVERIFY(!db.addCategory(QStringLiteral("relays"), &error));
			QVERIFY(error.contains(QStringLiteral("already")));
			QVERIFY2(db.setCategoryIcon(QStringLiteral("Relays"), QByteArray("\x89PNG\0", 5), &error),
					 qPrintable(error));
			int relays = 0;
			for (const PartCategory &category : db.categoryList())
				relays += category.name.compare(QStringLiteral("relays"), Qt::CaseInsensitive) == 0;
			QCOMPARE(relays, 1);
				//A file as it was written, its bytes untouched
			const QByteArray content("\0\1\2binary\xff", 10);
			const qint64 file = db.addFile(id, PartFileKind::Image, QStringLiteral("photo.png"), content, &error);
			QVERIFY2(file > 0, qPrintable(error));
			QCOMPARE(db.fileContent(file), content);
			QCOMPARE(db.categories(), QStringList{QStringLiteral("Contactors")});
		}

			//From the settings: the server, its password kept apart
		PartsDatabase::setConfiguredServer(server);
		PartsDatabase::setUseServer(true);
		QVERIFY(PartsDatabase::isServerPath(PartsDatabase::path()));
		QVERIFY(!PartsDatabase::path().contains(QStringLiteral("@@")));
		{
			PartsDatabase db;
			QString error;
			QVERIFY2(db.open(PartsDatabase::path(), &error), qPrintable(error));
			QCOMPARE(db.parts().size(), 1);
		}
		PartsDatabase::setUseServer(false);
		QVERIFY(!PartsDatabase::isServerPath(PartsDatabase::path()));

			//The parts of a file copied to it, accessories and files with them
		{
			PartsDatabase file_db;
			QString error;
			QVERIFY2(file_db.open(newPath(QStringLiteral("to-server.sqlite")), &error), qPrintable(error));
			QVERIFY(file_db.addPart(contactor(), &error));
			Part cover;
			cover.manufacturer = QStringLiteral("Acme");
			cover.order_number = QStringLiteral("COVER-1");
			const qint64 cover_id = file_db.addPart(cover, &error);
			QVERIFY(cover_id > 0);
			Part box;
			box.manufacturer = QStringLiteral("Acme");
			box.order_number = QStringLiteral("BOX-1");
			box.category = QStringLiteral("Enclosures");
			box.accessories << PartAccessory{cover_id, 2};
			const qint64 box_id = file_db.addPart(box, &error);
			QVERIFY2(box_id > 0, qPrintable(error));
			QVERIFY(file_db.addFile(box_id, PartFileKind::Image, QStringLiteral("box.png"), QByteArray("box"), &error) > 0);

			PartsDatabase db;
			QVERIFY2(db.openServer(server, &error), qPrintable(error));
				//The contactor is there already: left as it is
			const int count = db.copyFrom(file_db, &error);
			QVERIFY2(count >= 0, qPrintable(error));
			QCOMPARE(count, 2);
			const qint64 on_server = db.findPart(QStringLiteral("acme"), QStringLiteral("box-1"));
			QVERIFY(on_server > 0);
			const std::optional<Part> copied = db.part(on_server);
			QVERIFY(copied);
			QCOMPARE(copied->accessories.size(), 1);
			QCOMPARE(copied->accessories.first().accessory_id,
					 db.findPart(QStringLiteral("Acme"), QStringLiteral("COVER-1")));
			QCOMPARE(db.files(on_server).size(), 1);
			QCOMPARE(db.copyFrom(file_db, &error), 0);
		}
		QVERIFY(run(QStringLiteral("DROP DATABASE %1").arg(name)));
	}
};

QTEST_GUILESS_MAIN(tst_partsdatabase)
#include "tst_partsdatabase.moc"
