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
#include "partsdb/partsdbsymbols.h"
#include "partsdb/partsmanagerdialog.h"
#include "partsdb/partthumbnails.h"

#include <QFile>
#include <QApplication>
#include <QBuffer>
#include <QHeaderView>
#include <QImage>
#include <QStandardPaths>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QTimer>
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

	void theFunctionsAreKeptButNotShown()
	{
			//Not shown (Brandon, 2026-11-27): the parts manager kept
			//simple. Their tables stay hidden, with what the part has
		PartsManagerDialog::showManager(nullptr);
		QVERIFY(manager());
		auto *parts = child<QTableWidget>("parts_table");
		QVERIFY(parts);
		QCOMPARE(parts->rowCount(), 1);
		parts->selectRow(0);

		auto *tabs = manager()->findChild<QTabWidget *>();
		QVERIFY(tabs);
		QStringList titles;
		for (int index = 0; index < tabs->count(); ++index)
			titles << tabs->tabText(index);
		QVERIFY2(!titles.contains(QStringLiteral("Functions"))
				 && !titles.contains(QStringLiteral("Connection points")),
				 qPrintable(titles.join(QStringLiteral(", "))));

		auto *functions = child<QTableWidget>("functions_table");
		QVERIFY(functions);
		QVERIFY(!functions->isVisibleTo(manager()));
		QCOMPARE(functions->rowCount(), 3);
		auto *connections = child<QTableWidget>("connections_table");
		QVERIFY(connections);
		QVERIFY(!connections->isVisibleTo(manager()));
		QCOMPARE(connections->rowCount(), 1);
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

	void aSymbolFileIsKeptInTheDatabase()
	{
			//A symbol not in the collection (Brandon, 2026-11-20)
		const QString path = m_dir.filePath(QStringLiteral("button.elmt"));
		{
			QFile file(path);
			QVERIFY(file.open(QIODevice::WriteOnly));
			file.write("<definition type=\"element\" link_type=\"simple\" width=\"20\" height=\"20\" "
					   "hotspot_x=\"10\" hotspot_y=\"10\"><names><name lang=\"en\">Button</name></names>"
					   "<description/></definition>");
		}
		QVERIFY(child<QPushButton>("import_symbol_pb"));
		auto *symbols = child<QListWidget>("symbols_list");
		const int before = symbols->count();
		QVERIFY(manager()->importSymbolFile(path));
		QCOMPARE(symbols->count(), before + 1);
		const QString location = PartsDbSymbols::location(m_part_id, QStringLiteral("button.elmt"));
		QCOMPARE(symbols->item(before)->data(Qt::UserRole).toString(), location);
		PartsDatabase db;
		QVERIFY(db.open(PartsDatabase::path()));
		int kept = 0;
		for (const PartFile &file : db.files(m_part_id))
			kept += file.kind == PartFileKind::Symbol && file.file_name == QLatin1String("button.elmt");
		QCOMPARE(kept, 1);
			//The same file again: neither another row nor another copy
		QVERIFY(manager()->importSymbolFile(path));
		QCOMPARE(symbols->count(), before + 1);
		kept = 0;
		for (const PartFile &file : db.files(m_part_id))
			kept += file.kind == PartFileKind::Symbol;
		QCOMPARE(kept, 1);

			//Taken off again: the test after this one expects two
		symbols->setCurrentRow(before);
		child<QPushButton>("remove_symbol_pb")->click();
		QCOMPARE(symbols->count(), before);
	}

	void saveWritesEverything()
	{
		child<QPushButton>("save_part_pb")->click();

		PartsDatabase db;
		QString error;
		QVERIFY2(db.open(PartsDatabase::path(), &error), qPrintable(error));
		const Part part = *db.part(m_part_id);
			//The functions and connection points it had, kept as they were
		QCOMPARE(part.functions.size(), 3);
		QVERIFY(part.functions.at(1).kind == PartFunctionKind::DigitalInput);
		QCOMPARE(part.functions.at(1).description, QStringLiteral("DI a.0"));
		QCOMPARE(part.connections.size(), 1);
		QCOMPARE(part.connections.at(0).direction, QStringLiteral("0 1 0"));
		QVERIFY(part.connections.at(0).max_wires == std::optional<int>(2));
		QCOMPARE(part.symbols, (QStringList{QStringLiteral("custom://contact.elmt"),
											QStringLiteral("custom://relay.elmt")}));

		manager()->close();
	}

	void openedOnThePartRightClicked()
	{
			//From the parts collection (Brandon, 2026-11-20): every part
			//listed, the one right clicked picked
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		PartsDatabase db;
		QString error;
		QVERIFY2(db.open(PartsDatabase::path(), &error), qPrintable(error));
		Part other;
		other.manufacturer = QStringLiteral("ACME");
		other.order_number = QStringLiteral("PSU-24");
		const qint64 other_id = db.addPart(other, &error);
		QVERIFY2(other_id, qPrintable(error));

		PartsManagerDialog::showManager(nullptr);
		QVERIFY(manager());
		auto *table = child<QTableWidget>("parts_table");
		auto *search = child<QLineEdit>("search_le");
		QVERIFY(table && search);
		search->setText(QStringLiteral("nothing like this"));

		const auto picked = [table]() {
			const QList<QTableWidgetItem *> selected = table->selectedItems();
			return selected.isEmpty() ? QString()
									  : table->item(selected.first()->row(), 1)->text();
		};
		PartsManagerDialog::showManager(nullptr, other_id);
		QVERIFY(search->text().isEmpty());
		QCOMPARE(table->rowCount(), 2);
		QCOMPARE(picked(), QStringLiteral("PSU-24"));
		PartsManagerDialog::showManager(nullptr, m_part_id);
		QCOMPARE(picked(), QStringLiteral("PLC-1214"));
		manager()->close();
	}

	void technicalRowsAreCopiedAndPasted()
	{
			//Whole rows of the technical data (Brandon, 2026-11-21): to
			//another part or a spreadsheet, as tab separated text
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		PartsManagerDialog::showManager(nullptr, m_part_id);
		QVERIFY(manager());
		auto *table = child<QTableWidget>("technical_table");
		QVERIFY(table);
		table->setRowCount(0);
		const QList<QPair<QString, QString>> rows{
			{QStringLiteral("Coil voltage"), QStringLiteral("24 V DC")},
			{QStringLiteral("Contacts"), QStringLiteral("2 CO")}};
		for (const auto &row : rows) {
			const int r = table->rowCount();
			table->insertRow(r);
			table->setItem(r, 0, new QTableWidgetItem(row.first));
			table->setItem(r, 1, new QTableWidgetItem(row.second));
		}
		table->selectAll();
		child<QPushButton>("copy_technical_pb")->click();
		QCOMPARE(QApplication::clipboard()->text(),
				 QStringLiteral("Coil voltage\t24 V DC\nContacts\t2 CO\n"));

			//From a spreadsheet: a name there takes the value, a new one
			//is added, and both are selected
		QApplication::clipboard()->setText(QStringLiteral("Contacts\t4 CO\r\nWeight\t0,05 kg\r\n"));
		table->clearSelection();
		child<QPushButton>("paste_technical_pb")->click();
		QCOMPARE(table->rowCount(), 3);
		QCOMPARE(table->item(1, 1)->text(), QStringLiteral("4 CO"));
		QCOMPARE(table->item(2, 0)->text(), QStringLiteral("Weight"));
		QCOMPARE(table->item(2, 1)->text(), QStringLiteral("0,05 kg"));
		QCOMPARE(table->selectionModel()->selectedRows().size(), 2);

			//Several removed at once
		table->selectAll();
		child<QPushButton>("remove_technical_pb")->click();
		QCOMPARE(table->rowCount(), 0);

		child<QPushButton>("revert_part_pb")->click();
		manager()->close();
	}

	void aCategoryIsPickedFromTheList()
	{
			//A list instead of a text (Brandon, 2026-11-21): the categories
			//of the database, "Manage categories..." at its end
		{
			PartsDatabase db;
			QVERIFY(db.open(PartsDatabase::path()));
			QVERIFY(db.addCategory(QStringLiteral("PLCs")));
		}
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		PartsManagerDialog::showManager(nullptr, m_part_id);
		QVERIFY(manager());
		auto *combo = child<QComboBox>("category_cb");
		QVERIFY(combo);
		QCOMPARE(combo->itemText(0), QStringLiteral("(No category)"));
		QCOMPARE(combo->currentIndex(), 0);
		const int plcs = combo->findData(QStringLiteral("PLCs"));
		QVERIFY(plcs > 0);
		QVERIFY(combo->itemData(combo->count() - 1, Qt::UserRole + 3).toBool());

			//Chosen as the user does: the part changed, saved with it
		combo->setCurrentIndex(plcs);
		emit combo->activated(plcs);
		child<QPushButton>("save_part_pb")->click();
		PartsDatabase db;
		QVERIFY(db.open(PartsDatabase::path()));
		QCOMPARE(db.part(m_part_id)->category, QStringLiteral("PLCs"));
		manager()->close();
	}

	void severalPartsAreDeletedAtOnce()
	{
			//Selected with Ctrl or Shift, deleted in one go (Brandon,
			//2026-11-21)
		QList<qint64> ids;
		{
			PartsDatabase db;
			QVERIFY(db.open(PartsDatabase::path()));
			for (const QString &order : {QStringLiteral("DEL-1"), QStringLiteral("DEL-2")}) {
				Part part;
				part.manufacturer = QStringLiteral("ACME");
				part.order_number = order;
				ids << db.addPart(part);
				QVERIFY(ids.last() > 0);
			}
		}
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		PartsManagerDialog::showManager(nullptr, m_part_id);
		QVERIFY(manager());
		auto *table = child<QTableWidget>("parts_table");
		QVERIFY(table);
		const int before = table->rowCount();
		table->clearSelection();
		for (int row = 0; row < table->rowCount(); ++row) {
			if (ids.contains(table->item(row, 0)->data(Qt::UserRole).toLongLong()))
				table->selectionModel()->select(table->model()->index(row, 0),
												QItemSelectionModel::Select | QItemSelectionModel::Rows);
		}
		QCOMPARE(table->selectionModel()->selectedRows().size(), 2);

			//"Delete the 2 selected parts...?" answered yes
		QTimer yes;
		connect(&yes, &QTimer::timeout, []() {
			auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
			if (QAbstractButton *button = box ? box->button(QMessageBox::Yes) : nullptr)
				button->click();
		});
		yes.start(20);
		child<QPushButton>("delete_parts_pb")->click();
		yes.stop();

		QCOMPARE(table->rowCount(), before - 2);
		PartsDatabase db;
		QVERIFY(db.open(PartsDatabase::path()));
		QVERIFY(!db.part(ids.at(0)));
		QVERIFY(!db.part(ids.at(1)));
		QVERIFY(db.part(m_part_id));
		manager()->close();
	}

	void thePictureOfEachPartComesFirst()
	{
			//The first column (Brandon, 2026-11-21): the first image of the
			//part, small, read in the background; "No Picture" without
		QStandardPaths::setTestModeEnabled(true);
		QImage photo(400, 200, QImage::Format_ARGB32);
		photo.fill(Qt::red);
		QByteArray png;
		QBuffer buffer(&png);
		buffer.open(QIODevice::WriteOnly);
		photo.save(&buffer, "PNG");
		QCOMPARE(PartThumbnails::scaled(png, 96).size(), QSize(96, 48));

		qint64 file = 0;
		{
			PartsDatabase db;
			QVERIFY(db.open(PartsDatabase::path()));
			file = db.addFile(m_part_id, PartFileKind::Image, QStringLiteral("photo.png"), png);
			QVERIFY(file > 0);
			QCOMPARE(db.firstImages().value(m_part_id), file);
		}
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		PartsManagerDialog::showManager(nullptr, m_part_id);
		QVERIFY(manager());
		auto *table = child<QTableWidget>("parts_table");
		QVERIFY(table);
		QCOMPARE(table->horizontalHeader()->logicalIndex(0), 6);
		int with_photo = -1;
		for (int row = 0; row < table->rowCount(); ++row) {
			const qint64 id = table->item(row, 0)->data(Qt::UserRole).toLongLong();
			const qint64 shown = table->item(row, 6)->data(PartPictureDelegate::fileRole()).toLongLong();
			if (id == m_part_id) {
				QCOMPARE(shown, file);
				with_photo = row;
			} else {
				QCOMPARE(shown, qint64(0));
			}
		}
		QVERIFY(with_photo >= 0);

		PartThumbnails thumbnails;
		thumbnails.setDatabase(PartsDatabase::path());
		QVERIFY(thumbnails.picture(file).isNull());
		QTRY_VERIFY(thumbnails.isRead(file));
		QCOMPARE(thumbnails.picture(file).size(), QSize(96, 48));
		QVERIFY(!PartThumbnails::noPicture(Qt::gray, Qt::black).isNull());
		manager()->close();
	}
};

QTEST_MAIN(tst_partsmanager)
#include "tst_partsmanager.moc"
