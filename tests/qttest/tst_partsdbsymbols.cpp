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
#include "partsdb/partsarchive.h"
#include "partsdb/partsdatabase.h"
#include "partsdb/partsdbsymbols.h"
#include "partsdb/partssymbolfiles.h"

#include <QTemporaryDir>

/**
	A symbol file kept in the parts database for a part (Brandon,
	2026-11-20): it need not be in the collection; where one of the
	collection is needed, it is put there, once.
*/
class tst_partsdbsymbols : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;

	static QByteArray definition()
	{
		return QByteArrayLiteral(
				"<definition type=\"element\" link_type=\"simple\" width=\"20\" height=\"40\" "
				"hotspot_x=\"10\" hotspot_y=\"20\"><uuid uuid=\"{3b0e8a62-6a52-4b8e-9b2c-5f0c3c1d2e41}\"/>"
				"<names><name lang=\"en\">Push button</name></names>"
				"<description><rect x=\"-5\" y=\"-10\" width=\"10\" height=\"20\" "
				"style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
				"</description></definition>");
	}

private slots:
	void initTestCase()
	{
		QVERIFY(m_dir.isValid());
	}

	void aLocationNamesThePartAndTheFile()
	{
		const QString location = PartsDbSymbols::location(42, QStringLiteral("push button/1.elmt"));
		QVERIFY(PartsDbSymbols::isStored(location));
		QVERIFY(!PartsDbSymbols::isStored(QStringLiteral("common://10_electric/relay.elmt")));
		qint64 id = 0;
		QString name;
		QVERIFY(PartsDbSymbols::decode(location, &id, &name));
		QCOMPARE(id, qint64(42));
		QCOMPARE(name, QStringLiteral("push button/1.elmt"));
		QCOMPARE(PartsDbSymbols::fileName(location), name);
		QVERIFY(PartsDbSymbols::isSymbol(definition()));
		QVERIFY(!PartsDbSymbols::isSymbol(QByteArrayLiteral("<definition type=\"folio\"/>")));
		QVERIFY(!PartsDbSymbols::isSymbol(QByteArrayLiteral("not xml")));
	}

	void aProjectSymbolTakesItsTwinOfTheCollection()
	{
			//A part naming a symbol embedded in a project: lost once the
			//project is closed; the same of the collection is used
		PartsArchive::SymbolFile symbol;
		symbol.name = QStringLiteral("coil.elmt");
		symbol.definition = definition();
		const QString custom = PartsSymbolFiles::write(symbol, Part());
		QVERIFY(!custom.isEmpty());
		const QString embedded = QStringLiteral("project0+embed://import/")
				+ ElementsLocation(custom).collectionPath(false);
		QVERIFY(PartsSymbolFiles::isProjectSymbol(embedded));
		QVERIFY(!PartsSymbolFiles::isProjectSymbol(custom));
		QCOMPARE(PartsSymbolFiles::collectionTwin(embedded), custom);
		PartsDatabase database;
		QVERIFY(database.open(m_dir.filePath(QStringLiteral("twins.sqlite"))));
		QCOMPARE(PartsDbSymbols::resolve(database, embedded), custom);
			//None in the collection, its project closed: nothing
		QVERIFY(PartsDbSymbols::resolve(database, QStringLiteral(
				"project0+embed://import/nowhere/missing.elmt")).isEmpty());
	}

	void aStoredSymbolIsPutInTheCollectionOnce()
	{
		PartsDatabase database;
		QString error;
		QVERIFY2(database.open(m_dir.filePath(QStringLiteral("parts.sqlite")), &error), qPrintable(error));
		Part part;
		part.manufacturer = QStringLiteral("Acme");
		part.order_number = QStringLiteral("PB-22");
		const qint64 id = database.addPart(part, &error);
		QVERIFY2(id > 0, qPrintable(error));
		QVERIFY(database.addFile(id, PartFileKind::Symbol, QStringLiteral("button.elmt"), definition(), &error) > 0);
		QCOMPARE(database.files(id).first().kind, PartFileKind::Symbol);

		const QString location = PartsDbSymbols::location(id, QStringLiteral("button.elmt"));
		const PartsArchive::SymbolFile read = PartsDbSymbols::read(database, location);
		QCOMPARE(read.name, QStringLiteral("button.elmt"));
		QCOMPARE(read.definition, definition());
		QVERIFY(PartsDbSymbols::read(database, PartsDbSymbols::location(id, QStringLiteral("none.elmt")))
				.definition.isEmpty());

			//In the collection, under Imported parts, as a symbol
		const QString resolved = PartsDbSymbols::resolve(database, location);
		QVERIFY(!resolved.isEmpty());
		QVERIFY(!PartsDbSymbols::isStored(resolved));
		const ElementsLocation symbol(resolved);
		QVERIFY(symbol.exist());
		QVERIFY(symbol.isElement());
			//Once: the same one the next time
		QCOMPARE(PartsDbSymbols::resolve(database, location), resolved);
			//A symbol of the collection stays as it is
		QCOMPARE(PartsDbSymbols::resolve(database, resolved), resolved);

			//It travels in the .faspart of the part
		Part with_symbol = *database.part(id);
		with_symbol.symbols = QStringList{location};
		QVERIFY(database.updatePart(with_symbol, &error));
		const std::optional<PartsArchive::Content> content =
				PartsArchive::contentOf(database, id, PartsArchive::SymbolReader(), &error);
		QVERIFY2(content.has_value(), qPrintable(error));
		QCOMPARE(content->symbols.size(), 1);
		QCOMPARE(content->symbols.first().definition, definition());
	}
};

QET_APP_TEST_MAIN(tst_partsdbsymbols)
#include "tst_partsdbsymbols.moc"
