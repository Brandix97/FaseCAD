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
#include "partsdb/partsdatabase.h"
#include "partsdb/partsmanagerdialog.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTranslator>

/**
	The parts manager shows and saves the functions and the connection
	points of a part:

	 - a part read from the database shows its functions (kind in a list)
	   and its connection points;
	 - Add copies the selected function with the next pin, description and
	   PLC address, so the channels of a card are quick to enter;
	 - "From the functions" adds the connection points the functions name
	   and do not have yet, without doubles;
	 - Save writes all of it back, in the order of the tables.

	FASECAD_PARTS_SHOTS=<folder> saves a picture of every tab, and
	FASECAD_PARTS_QM=<file.qm> shows the window in that language.
*/
class tst_partsmanager : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;
	qint64 m_part_id = 0;

	PartsManagerDialog *manager()
	{
		for (QWidget *widget : QApplication::topLevelWidgets()) {
			if (auto *dialog = qobject_cast<PartsManagerDialog *>(widget)) {
				return dialog;
			}
		}
		return nullptr;
	}

	template <typename T> T *child(const char *name)
	{
		return manager()->findChild<T *>(QLatin1String(name));
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
		QCoreApplication::setApplicationName(QStringLiteral("partsmanager"));
		const QString qm = qEnvironmentVariable("FASECAD_PARTS_QM");
		if (!qm.isEmpty()) {
			auto *translator = new QTranslator(qApp);
			QVERIFY2(translator->load(qm), qPrintable(qm));
			QCoreApplication::installTranslator(translator);
		}
			//Numbers typed with a dot whatever the language of the computer
		QLocale::setDefault(QLocale::c());

		const QString path = m_dir.filePath(QStringLiteral("parts.sqlite"));
		PartsDatabase::setConfiguredPath(path);

		PartsDatabase db;
		QString error;
		QVERIFY2(db.open(path, &error), qPrintable(error));
		Part part;
		part.manufacturer = QStringLiteral("ACME");
		part.order_number = QStringLiteral("PLC-1214");
		part.description = QStringLiteral("Compact PLC");
		part.functions = {
			{PartFunctionKind::SupplyPlus, QStringLiteral("X10"),
			 QStringLiteral("1"), QStringLiteral("L+"), QString()},
			{PartFunctionKind::DigitalInput, QStringLiteral("X10"),
			 QStringLiteral("7"), QStringLiteral("DI a.0"),
			 QStringLiteral("I0.0")},
			{PartFunctionKind::BusPort, QStringLiteral("X1"),
			 QStringLiteral("P1"), QStringLiteral("PROFINET"), QString()}};
		PartConnection l_plus;
		l_plus.plug = QStringLiteral("X10");
		l_plus.pin = QStringLiteral("1");
		l_plus.section_max = 2.5;
		l_plus.max_wires = 2;
		l_plus.direction = QStringLiteral("0 1 0");
		part.connections = {l_plus};
		m_part_id = db.addPart(part, &error);
		QVERIFY2(m_part_id, qPrintable(error));
	}

	void showsTheFunctionsOfThePart()
	{
		PartsManagerDialog::showManager(nullptr);
		QVERIFY(manager());
		auto *parts = child<QTableWidget>("parts_table");
		QVERIFY(parts);
		QCOMPARE(parts->rowCount(), 1);
		parts->selectRow(0);

		auto *functions = child<QTableWidget>("functions_table");
		QVERIFY(functions);
		QCOMPARE(functions->rowCount(), 3);
		auto *kind = qobject_cast<QComboBox *>(functions->cellWidget(1, 0));
		QVERIFY(kind);
		QCOMPARE(kind->currentData().toString(),
				 QStringLiteral("digital_input"));
		QCOMPARE(functions->item(1, 3)->text(), QStringLiteral("DI a.0"));

		auto *connections = child<QTableWidget>("connections_table");
		QVERIFY(connections);
		QCOMPARE(connections->rowCount(), 1);
		QCOMPARE(connections->item(0, 3)->text(), QStringLiteral("2.5"));
	}

	void addCopiesWithTheNextNumber()
	{
		auto *functions = child<QTableWidget>("functions_table");
		functions->setCurrentCell(1, 2);
		child<QPushButton>("add_function_pb")->click();
		QCOMPARE(functions->rowCount(), 4);
		QCOMPARE(functions->item(2, 1)->text(), QStringLiteral("X10"));
		QCOMPARE(functions->item(2, 2)->text(), QStringLiteral("8"));
		QCOMPARE(functions->item(2, 3)->text(), QStringLiteral("DI a.1"));
		QCOMPARE(functions->item(2, 4)->text(), QStringLiteral("I0.1"));
		auto *kind = qobject_cast<QComboBox *>(functions->cellWidget(2, 0));
		QCOMPARE(kind->currentData().toString(),
				 QStringLiteral("digital_input"));
			//The bus port stays after the inputs
		QCOMPARE(functions->item(3, 2)->text(), QStringLiteral("P1"));
	}

	void connectionPointsFromTheFunctions()
	{
		auto *connections = child<QTableWidget>("connections_table");
		child<QPushButton>("from_functions_pb")->click();
			//X10:1 was there; X10:7, X10:8 and X1:P1 are added once
		QCOMPARE(connections->rowCount(), 4);
		QCOMPARE(connections->item(1, 1)->text(), QStringLiteral("7"));
		QCOMPARE(connections->item(3, 0)->text(), QStringLiteral("X1"));

			//Add copies the wire data with the next pin
		connections->setCurrentCell(0, 1);
		child<QPushButton>("add_connection_pb")->click();
		QCOMPARE(connections->rowCount(), 5);
		QCOMPARE(connections->item(1, 1)->text(), QStringLiteral("2"));
		QCOMPARE(connections->item(1, 3)->text(), QStringLiteral("2.5"));
		connections->item(1, 6)->setText(QStringLiteral("0.6"));
	}

	void picturesOfTheTabs()
	{
		const QString folder = qEnvironmentVariable("FASECAD_PARTS_SHOTS");
		if (folder.isEmpty()) {
			QSKIP("FASECAD_PARTS_SHOTS does not name a folder for pictures");
		}
		auto *tabs = manager()->findChild<QTabWidget *>();
		QVERIFY(tabs);
		manager()->resize(1100, 650);
		for (int index = 0; index < tabs->count(); ++index) {
			tabs->setCurrentIndex(index);
			const QString file = QDir(folder).filePath(
						QStringLiteral("parts-tab-%1.png").arg(index));
			QVERIFY2(manager()->grab().save(file), qPrintable(file));
		}
		tabs->setCurrentIndex(0);
	}

	void symbolsAreChosenAndOrdered()
	{
			//The .faspart files are written and read from here
		QVERIFY(child<QPushButton>("export_parts_pb"));
		QVERIFY(child<QPushButton>("import_parts_pb"));

		auto *symbols = child<QListWidget>("symbols_list");
		QVERIFY(symbols);
		QCOMPARE(symbols->count(), 0);
		QStringList given{QStringLiteral("custom://relay.elmt"),
						  QStringLiteral("custom://contact.elmt")};
		PartsManagerDialog::setSymbolChooser([&given](QWidget *) {
			return given.takeFirst();
		});
		child<QPushButton>("add_symbol_pb")->click();
		child<QPushButton>("add_symbol_pb")->click();
		QCOMPARE(symbols->count(), 2);
			//The second one first: Tab starts from it
		symbols->setCurrentRow(1);
		child<QPushButton>("symbol_up_pb")->click();
		QCOMPARE(symbols->item(0)->data(Qt::UserRole).toString(),
				 QStringLiteral("custom://contact.elmt"));
		PartsManagerDialog::setSymbolChooser(nullptr);
	}

	void saveWritesEverything()
	{
		child<QPushButton>("save_part_pb")->click();

		PartsDatabase db;
		QString error;
		QVERIFY2(db.open(PartsDatabase::path(), &error), qPrintable(error));
		const Part part = *db.part(m_part_id);
		QCOMPARE(part.functions.size(), 4);
		QVERIFY(part.functions.at(2).kind == PartFunctionKind::DigitalInput);
		QCOMPARE(part.functions.at(2).description, QStringLiteral("DI a.1"));
		QCOMPARE(part.functions.at(3).plug, QStringLiteral("X1"));

		QCOMPARE(part.connections.size(), 5);
			//The direction has no column, and is kept
		QCOMPARE(part.connections.at(0).direction, QStringLiteral("0 1 0"));
		QCOMPARE(part.connections.at(1).pin, QStringLiteral("2"));
		QVERIFY(part.connections.at(1).max_wires == std::optional<int>(2));
		QVERIFY(part.connections.at(1).torque == std::optional<double>(0.6));
		QVERIFY(!part.connections.at(2).section_max);
		QCOMPARE(part.symbols, (QStringList{QStringLiteral("custom://contact.elmt"),
											QStringLiteral("custom://relay.elmt")}));

		manager()->close();
	}
};

QTEST_MAIN(tst_partsmanager)
#include "tst_partsmanager.moc"
