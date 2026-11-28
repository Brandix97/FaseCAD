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
#include "ElementsCollection/xmlelementcollection.h"
#include "qet.h"
#include "qetapp.h"
#include "qetproject.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

/**
	The symbol of a part of the parts database placed in a project
	(Brandon, 2026-11-28): it goes into a folder of "import" named after
	the part, not into the folders of the collection it comes from, and a
	symbol of the same name there is replaced without asking.
*/
class tst_partimportfolder : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;

	QString writeSymbol(const QString &name)
	{
		const QString folder = QETApp::customElementsDirN() + QStringLiteral("/tst_partimportfolder/10_electric/relays");
		QDir().mkpath(folder);
		const QString path = folder + QStringLiteral("/coil.elmt");
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return QString();
		file.write(QStringLiteral(
			"<definition type=\"element\" link_type=\"simple\" width=\"20\" height=\"20\""
			" hotspot_x=\"10\" hotspot_y=\"10\"><names><name lang=\"en\">%1</name></names>"
			"<description><terminal x=\"0\" y=\"0\" orientation=\"n\"/></description></definition>")
				   .arg(name).toUtf8());
		return path;
	}

	static QString nameOf(const ElementsLocation &location)
	{
		return location.xml().firstChildElement(QStringLiteral("names"))
				.firstChildElement(QStringLiteral("name")).text();
	}

private slots:
	void initTestCase()
	{
		QVERIFY(m_dir.isValid());
		QETProject::setBackupEnabled(false);
	}

	void cleanupTestCase()
	{
		QDir(QETApp::customElementsDirN() + QStringLiteral("/tst_partimportfolder")).removeRecursively();
	}

	void aPartSymbolGoesIntoTheFolderOfThePart()
	{
		QETProject project;
		project.addNewDiagram();
		ElementsLocation source(writeSymbol(QStringLiteral("First")));
		QVERIFY(source.exist());

		const QString part = QStringLiteral("6ES7214-1AG40-0XB0");
		ElementsLocation placed = project.importPartElement(source, part);
		QVERIFY(placed.exist());
		const QString folder = QStringLiteral("import/") + QET::stringToFileName(part);
		QCOMPARE(placed.collectionPath(false), folder + QStringLiteral("/coil.elmt"));
		QVERIFY(project.embeddedElementCollection()->exist(folder));
		QCOMPARE(nameOf(placed), QStringLiteral("First"));

			//The same again: the one there
		QCOMPARE(project.importPartElement(source, part).collectionPath(false),
				 placed.collectionPath(false));

			//Changed: replaced, without a question
		ElementsLocation changed(writeSymbol(QStringLiteral("Second")));
		ElementsLocation replaced = project.importPartElement(changed, part);
		QCOMPARE(replaced.collectionPath(false), placed.collectionPath(false));
		QCOMPARE(nameOf(ElementsLocation(folder + QStringLiteral("/coil.elmt"), &project)),
				 QStringLiteral("Second"));

	}
};

QET_APP_TEST_MAIN(tst_partimportfolder)
#include "tst_partimportfolder.moc"
