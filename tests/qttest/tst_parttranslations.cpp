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
#include "partsdb/partsdatabase.h"
#include "partsdb/partsmanagerdialog.h"
#include "properties/projectlanguages.h"
#include "ui/projectlanguagesdialog.h"

#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>

/**
	The parts manager of 2026-11-27 (Brandon): the maximum current with
	its ?, the description and the model in other languages, which fill in
	the texts of a project in its languages, the Cable tab only for the
	Cables category, and no Accessories tab.
*/
class tst_parttranslations : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;
	bool m_was_enabled = false;
	QString m_was_path;

	static PartsManagerDialog *manager()
	{
		for (QWidget *widget : QApplication::topLevelWidgets()) {
			if (auto *dialog = qobject_cast<PartsManagerDialog *>(widget)) return dialog;
		}
		return nullptr;
	}

	template <typename T> static T *child(const char *name)
	{
		return manager() ? manager()->findChild<T *>(QLatin1String(name)) : nullptr;
	}

private slots:
	void initTestCase()
	{
		QVERIFY(m_dir.isValid());
		const QString path = m_dir.filePath(QStringLiteral("parts.sqlite"));
		PartsDatabase database;
		QString error;
		QVERIFY2(database.open(path, &error), qPrintable(error));
		Part contactor;
		contactor.manufacturer = QStringLiteral("Acme");
		contactor.order_number = QStringLiteral("MC-1");
		contactor.description = QStringLiteral("Contactor tripolar");
		contactor.model = QStringLiteral("Serie D");
		contactor.max_current = 25;
		PartTranslations::setTranslation(contactor, QStringLiteral("description"), QStringLiteral("en"),
										 QStringLiteral("Three-pole contactor"));
		PartTranslations::setTranslation(contactor, QStringLiteral("description"), QStringLiteral("de"),
										 QStringLiteral("Dreipoliger Schütz"));
		PartTranslations::setTranslation(contactor, QStringLiteral("model"), QStringLiteral("de"),
										 QStringLiteral("Baureihe D"));
		QVERIFY2(database.addPart(contactor, &error) > 0, qPrintable(error));
		Part cable;
		cable.manufacturer = QStringLiteral("Acme");
		cable.order_number = QStringLiteral("CB-3G15");
		cable.description = QStringLiteral("Manguera");
		cable.category = PartsDatabase::cablesCategory();
		cable.core_count = 3;
		QVERIFY2(database.addPart(cable, &error) > 0, qPrintable(error));
		database.close();
		m_was_enabled = PartsDatabase::isEnabled();
		m_was_path = PartsDatabase::configuredPath();
		PartsDatabase::setConfiguredPath(path);
		PartsDatabase::setEnabled(true);
	}

	void cleanupTestCase()
	{
		if (manager()) manager()->close();
		PartsDatabase::setConfiguredPath(m_was_path);
		PartsDatabase::setEnabled(m_was_enabled);
	}

	void theTranslationsFillTheProjectLanguages()
	{
		ProjectLanguages languages;
		languages.setSourceLanguage(QStringLiteral("es"));
		QVERIFY(languages.addLanguage(QStringLiteral("en-GB")));
		QVERIFY(languages.addLanguage(QStringLiteral("de")));
		QVERIFY(languages.addLanguage(QStringLiteral("fr")));
		languages.addPhrase(QStringLiteral("Contactor tripolar"));
		languages.addPhrase(QStringLiteral("Serie D"));
		languages.addPhrase(QStringLiteral("Parada de emergencia"));
			//Words already typed are kept
		languages.setTranslation(QStringLiteral("Contactor tripolar"), QStringLiteral("de"),
								 QStringLiteral("Schütz"));
		QCOMPARE(ProjectLanguagesDialog::fillFromParts(languages), 2);
		QCOMPARE(languages.translation(QStringLiteral("Contactor tripolar"), QStringLiteral("en-GB")),
				 QStringLiteral("Three-pole contactor"));
		QCOMPARE(languages.translation(QStringLiteral("Contactor tripolar"), QStringLiteral("de")),
				 QStringLiteral("Schütz"));
		QCOMPARE(languages.translation(QStringLiteral("Serie D"), QStringLiteral("de")),
				 QStringLiteral("Baureihe D"));
		QVERIFY(languages.translation(QStringLiteral("Serie D"), QStringLiteral("fr")).isEmpty());
		QVERIFY(languages.translation(QStringLiteral("Parada de emergencia"), QStringLiteral("en-GB")).isEmpty());
			//The folios shown in German read the part in German
		languages.setActiveLanguage(QStringLiteral("de"));
		QCOMPARE(languages.translate(QStringLiteral("Serie D")), QStringLiteral("Baureihe D"));
			//Nothing more to fill
		QCOMPARE(ProjectLanguagesDialog::fillFromParts(languages), 0);
	}

	void theManagerShowsWhatIsKept()
	{
		PartsManagerDialog::showManager(nullptr);
		QVERIFY(manager());
		auto *tabs = manager()->findChild<QTabWidget *>();
		QVERIFY(tabs);
		QStringList titles;
		for (int index = 0; index < tabs->count(); ++index) titles << tabs->tabText(index);
		QVERIFY2(!titles.contains(QStringLiteral("Accessories")), qPrintable(titles.join(QStringLiteral(", "))));
		QVERIFY(titles.contains(QStringLiteral("Translations")));
		const int cable_tab = titles.indexOf(QStringLiteral("Cable"));
		QVERIFY(cable_tab >= 0);

		auto *help = child<QLabel>("max_current_help");
		QVERIFY(help);
		QVERIFY(help->toolTip().contains(QStringLiteral("Downstream")));
		QVERIFY(child<QLineEdit>("max_current_le"));

		auto *parts = child<QTableWidget>("parts_table");
		QVERIFY(parts);
		QCOMPARE(parts->rowCount(), 2);
		int contactor_row = -1, cable_row = -1;
		for (int row = 0; row < parts->rowCount(); ++row) {
			for (int column = 0; column < parts->columnCount(); ++column) {
				const QTableWidgetItem *item = parts->item(row, column);
				if (item && item->text() == QLatin1String("MC-1")) contactor_row = row;
				if (item && item->text() == QLatin1String("CB-3G15")) cable_row = row;
			}
		}
		QVERIFY(contactor_row >= 0 && cable_row >= 0);

			//The contactor: no Cable tab; its maximum current and its
			//translations
		parts->selectRow(contactor_row);
		QVERIFY(!tabs->isTabVisible(cable_tab));
		QCOMPARE(child<QLineEdit>("max_current_le")->text().toDouble(), 25.0);
		auto *translations = child<QTableWidget>("translations_table");
		QVERIFY(translations);
		bool english = false;
		for (int row = 0; row < translations->rowCount(); ++row) {
			if (translations->item(row, 0)->data(Qt::UserRole).toString() == QLatin1String("en")) {
				english = translations->item(row, 1)->text() == QLatin1String("Three-pole contactor");
			}
		}
		QVERIFY(english);

			//The cable: its Cable tab
		parts->selectRow(cable_row);
		QVERIFY(tabs->isTabVisible(cable_tab));
	}
};

QET_APP_TEST_MAIN(tst_parttranslations)
#include "tst_parttranslations.moc"
