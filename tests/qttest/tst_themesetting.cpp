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
/*
	QetSettings::theme() -- the light or dark colors of the whole
	interface, Settings > General > Theme.

	* Nobody gets a theme without choosing one: the default, and any value
	  this version does not know (written by a newer one, or by hand),
	  is AsBefore, where "Use system colors" and the application color
	  keep working as they always did.
	* Each theme reads back as it was stored.
	* Dark is dark whatever the system does, Light is light, and Follow
	  the system is what the system is.

	This test owns its own QSettings scope (organization + application
	name), so it cannot read or write the configuration of whoever runs
	it.
*/

#include "utils/qetsettings.h"

#include <QSettings>
#include <QtTest>

class TstThemeSetting : public QObject
{
		Q_OBJECT

	private slots:
		void initTestCase();
		void init();

		void defaultsToAsBefore();
		void everyThemeReadsBack();
		void unknownValueFallsBackToAsBefore();
		void darkFollowsTheChoice();
};

void TstThemeSetting::initTestCase()
{
	QCoreApplication::setOrganizationName(
				QStringLiteral("QElectroTech-tst_themesetting"));
	QCoreApplication::setApplicationName(
				QStringLiteral("tst_themesetting"));
}

void TstThemeSetting::init()
{
	QSettings settings;
	settings.clear();
}

void TstThemeSetting::defaultsToAsBefore()
{
	QVERIFY(QetSettings::theme() == QetSettings::Theme::AsBefore);
}

void TstThemeSetting::everyThemeReadsBack()
{
	for (QetSettings::Theme theme : {QetSettings::Theme::System,
									 QetSettings::Theme::Light,
									 QetSettings::Theme::Dark,
									 QetSettings::Theme::AsBefore}) {
		QetSettings::setTheme(theme);
		QVERIFY(QetSettings::theme() == theme);
	}
}

void TstThemeSetting::unknownValueFallsBackToAsBefore()
{
	QSettings settings;
	for (const QVariant &value : {QVariant(7), QVariant(-1),
								  QVariant(QStringLiteral("dark"))}) {
		settings.setValue(QStringLiteral("theme"), value);
		QVERIFY2(QetSettings::theme() == QetSettings::Theme::AsBefore,
				 qPrintable(value.toString()));
	}
}

void TstThemeSetting::darkFollowsTheChoice()
{
	using QetSettings::Theme;
	QVERIFY(QetSettings::themeIsDark(Theme::Dark, false));
	QVERIFY(QetSettings::themeIsDark(Theme::Dark, true));
	QVERIFY(!QetSettings::themeIsDark(Theme::Light, true));
	QVERIFY(!QetSettings::themeIsDark(Theme::Light, false));
	QVERIFY(QetSettings::themeIsDark(Theme::System, true));
	QVERIFY(!QetSettings::themeIsDark(Theme::System, false));
}

QTEST_GUILESS_MAIN(TstThemeSetting)
#include "tst_themesetting.moc"
