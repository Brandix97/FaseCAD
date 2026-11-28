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
#include "devicexrefs.h"
#include "qetproject.h"
#include "qetresult.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

/**
	The cross references between the parts of one device (Brandon,
	2026-11-28): the main part, the one made of parts, lists where the
	others are, each other part where the main one is; the places are
	written with the project's format; the setting is saved only when
	used, so a project saved before reads it off.
*/
class tst_devicexrefs : public QObject
{
	Q_OBJECT

	using Part = DeviceXRefs::Part;

	static Part part(int folio, const QString &cell, bool has_parts = false)
	{
		Part p;
		p.element = QUuid::createUuid();
		p.folio_index = folio - 1;
		p.cell = cell;
		p.has_parts = has_parts;
		return p;
	}

private slots:
	void initTestCase()
	{
		QETProject::setBackupEnabled(false);
	}

	void placesAreWrittenWithTheFormat()
	{
		Part p = part(3, QStringLiteral("B4"));
		QCOMPARE(DeviceXRefs::placeText(QStringLiteral("%f-%l%c"), p), QStringLiteral("3-B4"));
		QCOMPARE(DeviceXRefs::placeText(QStringLiteral("%f.%c"), p), QStringLiteral("3.4"));
		p.folio_label = QStringLiteral("=A1/12");
		QCOMPARE(DeviceXRefs::placeText(QStringLiteral("/%f.%c"), p), QStringLiteral("/=A1/12.4"));
	}

	void theMainPartListsTheOthers()
	{
		const QList<Part> parts{part(1, "A1"), part(2, "C3", true), part(5, "B7")};
		QCOMPARE(DeviceXRefs::mainOf(parts), 1);
		const QHash<QString, QList<Part>> devices{{QStringLiteral("-K1"), parts}};
		DeviceXRefSettings settings;
		settings.enabled = true;
		const QString formula = QStringLiteral("%f-%l%c");

		QList<QUuid> targets;
		QCOMPARE(DeviceXRefs::placesFor(parts.at(1).element, devices, "-K1", settings, formula, &targets),
				 (QStringList{"1-A1", "5-B7"}));
		QCOMPARE(targets, (QList<QUuid>{parts.at(0).element, parts.at(2).element}));
		QCOMPARE(DeviceXRefs::placesFor(parts.at(2).element, devices, "-K1", settings, formula),
				 (QStringList{"2-C3"}));

		settings.secondary = DeviceXRefSettings::Secondary::All;
		QCOMPARE(DeviceXRefs::placesFor(parts.at(2).element, devices, "-K1", settings, formula),
				 (QStringList{"1-A1", "2-C3"}));
		settings.secondary = DeviceXRefSettings::Secondary::None;
		QVERIFY(DeviceXRefs::placesFor(parts.at(2).element, devices, "-K1", settings, formula).isEmpty());

			//Off, or no other part: nothing
		settings.enabled = false;
		QVERIFY(DeviceXRefs::placesFor(parts.at(1).element, devices, "-K1", settings, formula).isEmpty());
		settings.enabled = true;
		QVERIFY(DeviceXRefs::placesFor(QUuid::createUuid(), devices, "-K2", settings, formula).isEmpty());
	}

	void withoutPartsTheFirstIsTheMain()
	{
		QCOMPARE(DeviceXRefs::mainOf({part(4, "A1"), part(6, "A1")}), 0);
	}

	void theSettingIsSavedOnlyWhenUsed()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		QETProject project;
		project.addNewDiagram();
		QVERIFY(!project.deviceXRefs().enabled);
		project.setFilePath(dir.filePath(QStringLiteral("plain.qet")));
		QVERIFY(project.write().isOk());
		{
			QFile file(project.filePath());
			QVERIFY(file.open(QIODevice::ReadOnly));
			QVERIFY(!file.readAll().contains("device_xrefs"));
		}

		DeviceXRefSettings settings;
		settings.enabled = true;
		settings.place = DeviceXRefSettings::Place::BelowSymbol;
		project.setDeviceXRefs(settings);
		QVERIFY(project.write().isOk());
		QETProject reopened(project.filePath());
		QCOMPARE(reopened.state(), QETProject::Ok);
		QVERIFY(reopened.deviceXRefs() == settings);
	}
};

QET_APP_TEST_MAIN(tst_devicexrefs)
#include "tst_devicexrefs.moc"
