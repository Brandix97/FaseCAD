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
#include "container/qetzip.h"
#include "partsdb/partsarchive.h"
#include "partsdb/partsdatabase.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

/**
	A part written to a .faspart and read back into another parts
	database (PartsArchive):

	 - every field comes back, functions and connection points included;
	 - an accessory comes back when the other database has it, by
	   manufacturer and order number;
	 - the files come back with their content, the symbols through the
	   writer that puts them in the collection;
	 - storing again over the same part replaces it, its files too;
	 - a zip which is no part file, or one of a newer version, is refused.
*/
class tst_partsarchive : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;

	static Part cpu()
	{
		Part part;
		part.manufacturer = QStringLiteral("ACME");
		part.order_number = QStringLiteral("PLC-1214");
		part.description = QStringLiteral("Compact PLC");
		part.category = QStringLiteral("PLC");
		part.code_letter = QStringLiteral("K");
		part.gtin = QStringLiteral("0000000012147");
		part.width = 100;
		part.height = 90;
		part.depth = 70;
		part.weight = 0.4;
		part.power_loss = 10;
		part.clearance_top = 20;
		part.technical_data = {{QStringLiteral("Supply"), QStringLiteral("24 V DC")}};
		part.functions = {
			{PartFunctionKind::SupplyPlus, QStringLiteral("X10"),
			 QStringLiteral("1"), QStringLiteral("L+"), QString()},
			{PartFunctionKind::DigitalInput, QStringLiteral("X10"),
			 QStringLiteral("7"), QStringLiteral("DI a.0"), QStringLiteral("I0.0")}};
		PartConnection point;
		point.plug = QStringLiteral("X10");
		point.pin = QStringLiteral("1");
		point.section_max = 2.5;
		point.max_wires = 2;
		point.torque = 0.5;
		point.direction = QStringLiteral("0 -1 0");
		part.connections = {point};
		return part;
	}

	static Part signalBoard()
	{
		Part part;
		part.manufacturer = QStringLiteral("ACME");
		part.order_number = QStringLiteral("IO-0423");
		part.description = QStringLiteral("Input board");
		return part;
	}

	static PartsArchive::SymbolFile readSymbol(const QString &location)
	{
		return {location.section(QLatin1Char('/'), -1),
				QStringLiteral("<definition>%1</definition>").arg(location).toUtf8()};
	}

	private slots:
		void initTestCase()
		{
			QVERIFY(m_dir.isValid());
		}

		void partGoesToAnotherDatabase()
		{
			PartsDatabase source;
			QString error;
			QVERIFY2(source.open(m_dir.filePath(QStringLiteral("source.sqlite")), &error),
					 qPrintable(error));
			const qint64 board = source.addPart(signalBoard(), &error);
			QVERIFY2(board, qPrintable(error));
			Part part = cpu();
			part.accessories = {{board, 2}};
			part.symbols = {QStringLiteral("custom://plc/cpu_power.elmt"),
							QStringLiteral("custom://plc/cpu_inputs.elmt")};
			const qint64 id = source.addPart(part, &error);
			QVERIFY2(id, qPrintable(error));
			QVERIFY(source.addFile(id, PartFileKind::Image, QStringLiteral("cpu.png"),
								   "PNG", &error));
			QVERIFY(source.addFile(id, PartFileKind::Dxf, QStringLiteral("cpu.dxf"),
								   "0\nEOF\n", &error));

			const auto content = PartsArchive::contentOf(source, id, &readSymbol, &error);
			QVERIFY2(content, qPrintable(error));
			QCOMPARE(content->accessories.size(), 1);
			QCOMPARE(content->files.size(), 2);
			QCOMPARE(content->symbols.size(), 2);

			const QString path = m_dir.filePath(PartsArchive::fileNameFor(content->part));
			QCOMPARE(QFileInfo(path).fileName(),
					 QStringLiteral("ACME_PLC-1214.faspart"));
			QVERIFY2(PartsArchive::write(path, *content, &error), qPrintable(error));

			const auto read = PartsArchive::read(path, &error);
			QVERIFY2(read, qPrintable(error));
			QCOMPARE(PartsDatabase::toJson(read->part), PartsDatabase::toJson(cpu()));
			QCOMPARE(read->files.at(1).content, QByteArray("0\nEOF\n"));
			QCOMPARE(read->symbols.at(0).name, QStringLiteral("cpu_power.elmt"));

				//Another database, which has the accessory
			PartsDatabase target;
			QVERIFY2(target.open(m_dir.filePath(QStringLiteral("target.sqlite")), &error),
					 qPrintable(error));
			const qint64 target_board = target.addPart(signalBoard(), &error);
			QStringList written;
			const auto writer = [&written](const PartsArchive::SymbolFile &symbol, const Part &) {
				written << QString::fromUtf8(symbol.definition);
				return QStringLiteral("custom://imported_parts/ACME/") + symbol.name;
			};
			const qint64 stored = PartsArchive::store(target, *read, 0, writer, &error);
			QVERIFY2(stored, qPrintable(error));
			const Part back = *target.part(stored);
			QCOMPARE(PartsDatabase::toJson(back), PartsDatabase::toJson(cpu()));
			QCOMPARE(back.accessories.size(), 1);
			QCOMPARE(back.accessories.first().accessory_id, target_board);
			QCOMPARE(back.accessories.first().quantity, 2.0);
			QCOMPARE(back.symbols,
					 (QStringList{QStringLiteral("custom://imported_parts/ACME/cpu_power.elmt"),
								  QStringLiteral("custom://imported_parts/ACME/cpu_inputs.elmt")}));
			QCOMPARE(written.first(),
					 QStringLiteral("<definition>custom://plc/cpu_power.elmt</definition>"));
			QCOMPARE(target.files(stored).size(), 2);

				//Again over the same part: replaced, its files too
			PartsArchive::Content again = *read;
			again.part.description = QStringLiteral("Compact PLC, new description");
			QCOMPARE(PartsArchive::store(target, again, stored, writer, &error), stored);
			QCOMPARE(target.part(stored)->description, again.part.description);
			QCOMPARE(target.files(stored).size(), 2);
			QCOMPARE(target.parts().size(), 2);
		}

		void foreignOrNewerFileIsRefused()
		{
			QString error;
			const QString foreign = m_dir.filePath(QStringLiteral("foreign.faspart"));
			QVERIFY(QetZip::write(foreign, {{QStringLiteral("readme.txt"), "hello", true}}));
			QVERIFY(!PartsArchive::read(foreign, &error));
			QVERIFY(!error.isEmpty());

			const QString newer = m_dir.filePath(QStringLiteral("newer.faspart"));
			const QJsonObject root{{QStringLiteral("format"), QStringLiteral("faspart")},
								   {QStringLiteral("version"), 99}};
			QVERIFY(QetZip::write(newer, {{QStringLiteral("faspart.json"),
										   QJsonDocument(root).toJson(), true}}));
			error.clear();
			QVERIFY(!PartsArchive::read(newer, &error));
			QVERIFY(!error.isEmpty());
		}
};

QTEST_GUILESS_MAIN(tst_partsarchive)
#include "tst_partsarchive.moc"
