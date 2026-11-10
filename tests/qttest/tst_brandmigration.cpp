// SPDX-License-Identifier: GPL-2.0-or-later
#include "utils/brandmigration.h"

#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

/*
	The settings of FaseCAD: copied from those of QElectroTech on the first
	start, then given the new design once (command tabs, parts database,
	the theme of the system when none is chosen) -- whether they are new
	or copied, since QElectroTech had none of these. What is changed after
	that stays.
*/
class tst_brandmigration : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;
	int m_run = 0;

		/// A settings folder of its own for each case
	void freshFolder()
	{
		const QString folder = m_dir.filePath(QString::number(++m_run));
		QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, folder);
		QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, folder);
	}

private slots:
	void initTestCase()
	{
		QVERIFY(m_dir.isValid());
		QStandardPaths::setTestModeEnabled(true);
		QCoreApplication::setOrganizationName(BrandMigration::name());
		QCoreApplication::setApplicationName(BrandMigration::name());
		QSettings::setDefaultFormat(QSettings::IniFormat);
	}

	void copiedSettingsGetTheNewDesign()
	{
		freshFolder();
		{
			QSettings old(QSettings::IniFormat, QSettings::UserScope,
						  BrandMigration::formerName(), BrandMigration::formerName());
			old.setValue(QStringLiteral("diagrameditor/grid"), 20);
			old.sync();
		}
		BrandMigration::run();
		BrandMigration::applyFirstStartDefaults();

		QSettings settings;
		QCOMPARE(settings.value(QStringLiteral("diagrameditor/grid")).toInt(), 20);
		QCOMPARE(settings.value(QStringLiteral("diagrameditor/command_tabs")).toBool(), true);
		QCOMPARE(settings.value(QStringLiteral("parts-database/enabled")).toBool(), true);
		QCOMPARE(settings.value(QStringLiteral("theme")).toInt(), 1);
	}

	void newSettingsGetTheNewDesign()
	{
		freshFolder();
		BrandMigration::run();
		BrandMigration::applyFirstStartDefaults();

		QSettings settings;
		QCOMPARE(settings.value(QStringLiteral("diagrameditor/command_tabs")).toBool(), true);
		QCOMPARE(settings.value(QStringLiteral("parts-database/enabled")).toBool(), true);
	}

	void aChosenThemeStays()
	{
		freshFolder();
		{
			QSettings old(QSettings::IniFormat, QSettings::UserScope,
						  BrandMigration::formerName(), BrandMigration::formerName());
			old.setValue(QStringLiteral("theme"), 3);
			old.sync();
		}
		BrandMigration::run();
		BrandMigration::applyFirstStartDefaults();
		QCOMPARE(QSettings().value(QStringLiteral("theme")).toInt(), 3);
	}

	void whatIsChangedAfterwardsStays()
	{
		freshFolder();
		BrandMigration::run();
		BrandMigration::applyFirstStartDefaults();
		{
			QSettings settings;
			settings.setValue(QStringLiteral("diagrameditor/command_tabs"), false);
			settings.setValue(QStringLiteral("parts-database/enabled"), false);
			settings.sync();
		}
		BrandMigration::run();
		BrandMigration::applyFirstStartDefaults();

		QSettings settings;
		QCOMPARE(settings.value(QStringLiteral("diagrameditor/command_tabs")).toBool(), false);
		QCOMPARE(settings.value(QStringLiteral("parts-database/enabled")).toBool(), false);
	}
};

QTEST_GUILESS_MAIN(tst_brandmigration)
#include "tst_brandmigration.moc"
