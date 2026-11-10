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
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

/**
	The parts database keeps what is written in it, and refuses what would
	break the link between an element and its part:

	 - a part, its technical data, accessories and empty numbers read back
	   as they were written, also after the file is closed and opened again;
	 - two parts cannot share a manufacturer and an order number, whatever
	   the case, but any number of parts may have no order number yet;
	 - an image or a DXF is copied in the database or only linked, and both
	   can be saved back to disk;
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
		part.manufacturer = QStringLiteral("Schneider Electric");
		part.order_number = QStringLiteral("LC1D09P7");
		part.internal_reference = QStringLiteral("INT-0042");
		part.supplier = QStringLiteral("Rexel");
		part.supplier_reference = QStringLiteral("RX-778812");
		part.description = QStringLiteral("Contactor 9 A, coil 230 V AC");
		part.designation = QStringLiteral("TeSys D");
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
			cover.manufacturer = QStringLiteral("Schneider Electric");
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
				 QStringList{QStringLiteral("Schneider Electric")});
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
		twin.order_number = QStringLiteral(" lc1d09p7 ");
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
		other.manufacturer = QStringLiteral("ABB");
		other.order_number = QStringLiteral("AF09");
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
		breaker.manufacturer = QStringLiteral("ABB");
		breaker.order_number = QStringLiteral("S201-C10");
		breaker.description = QStringLiteral("Circuit breaker 10 A");
		QVERIFY(db.addPart(breaker, &error));
		Part percent;
		percent.order_number = QStringLiteral("X100%");
		QVERIFY(db.addPart(percent, &error));

		QCOMPARE(db.parts().size(), 3);
		QCOMPARE(db.parts(QStringLiteral("contactor 230")).size(), 1);
		QCOMPARE(db.parts(QStringLiteral("rx-778812")).size(), 1);
		QCOMPARE(db.parts(QStringLiteral("contactor ABB")).size(), 0);
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
		cable.manufacturer = QStringLiteral("Lapp");
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
				"VALUES ('ABB', 'S201-C10', 0.12, '2026-11-07', '2026-11-07')")));
			raw.close();
		}
		QSqlDatabase::removeDatabase(QStringLiteral("v1"));

		PartsDatabase db;
		QString error;
		QVERIFY2(db.open(path, &error), qPrintable(error));
		const QList<Part> parts = db.parts();
		QCOMPARE(parts.size(), 1);
		QCOMPARE(parts.first().order_number, QStringLiteral("S201-C10"));
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
		cable.manufacturer = QStringLiteral("Lapp");
		cable.order_number = QStringLiteral("0012345");
		cable.designation = QStringLiteral("H07RN-F 5G2,5");
		cable.core_count = 5;
		cable.core_colors = QStringLiteral(" BK; BN,GY |BU|GNYE ");
		Part breaker;
		breaker.order_number = QStringLiteral("S201-C10");
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
			{QStringLiteral("Schneider Electric"), QStringLiteral("LC1D09P7"), 1},
			{QStringLiteral("Lapp\tCable"), QStringLiteral("0012345"), 12.5}};
		const QString value = PartsLinks::encode(links);
		const QList<PartsLinks::Link> read = PartsLinks::decode(value);
		QCOMPARE(read.size(), 2);
		QVERIFY(read.first() == links.first());
			//A tab typed in a field does not break the line
		QCOMPARE(read.at(1).manufacturer, QStringLiteral("Lapp Cable"));
		QCOMPARE(read.at(1).units, 12.5);
			//A line without an order number says nothing, units which are
			//no number count one
		const QList<PartsLinks::Link> odd = PartsLinks::decode(
					QStringLiteral("ABB\t\t3\nABB\tS201\tmany"));
		QCOMPARE(odd.size(), 1);
		QCOMPARE(odd.first().units, 1.0);
		QVERIFY(PartsLinks::decode(QString()).isEmpty());
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
};

QTEST_GUILESS_MAIN(tst_partsdatabase)
#include "tst_partsdatabase.moc"
