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
#include "qetapp.h"
#include "qetproject.h"
#include "ElementsCollection/elementscollectionwidget.h"
#include "ElementsCollection/elementslocation.h"
#include "ElementsCollection/xmlelementcollection.h"

#include <QDomDocument>
#include <QUuid>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

/**
	The copy made when a project is opened (Brandon, 2026-11-21): one copy
	beside it, "<name>_backup", written over at each opening, instead of
	one more file each time.
*/
class tst_openingcopy : public QObject
{
	Q_OBJECT

	static bool write(const QString &path, const QByteArray &content)
	{
		QFile file(path);
		return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
	}

	static QByteArray read(const QString &path)
	{
		QFile file(path);
		return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
	}

private slots:
	void oneCopyWrittenOver()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString project = dir.filePath(QStringLiteral("XARC200.qet"));
		const QString copy = dir.filePath(QStringLiteral("XARC200_backup.qet"));
		QCOMPARE(QETProject::openingCopyOf(project), copy);
			//A copy is not copied again
		QVERIFY(QETProject::openingCopyOf(copy).isEmpty());

		QVERIFY(write(project, "first"));
		QVERIFY(QETProject::makeOpeningCopy(project));
		QCOMPARE(read(copy), QByteArray("first"));
		QVERIFY(write(project, "second"));
		QVERIFY(QETProject::makeOpeningCopy(project));
		QCOMPARE(read(copy), QByteArray("second"));
			//The project and its one copy, nothing more
		QCOMPARE(QDir(dir.path()).entryList(QDir::Files).size(), 2);
	}

	void aSymbolIsDownloaded()
	{
			//From the collection panel (Brandon, 2026-11-21): a symbol of
			//a folder as its file is, one of a project as it holds it
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const ElementsLocation common(QStringLiteral(
				"common://10_electric/05_iec_60617_cuadros/04_proteccion/fusible.elmt"));
		QVERIFY(common.exist());
		const QString path = dir.filePath(QStringLiteral("fusible.elmt"));
		QString error;
		QVERIFY2(ElementsCollectionWidget::saveSymbol(common, path, &error), qPrintable(error));
		QFile original(common.fileSystemPath());
		QVERIFY(original.open(QIODevice::ReadOnly));
		QCOMPARE(read(path), original.readAll());

		QETProject project;
		project.addNewDiagram();
		const QString xml = QStringLiteral(
				"<definition type=\"element\" link_type=\"simple\" width=\"20\" height=\"40\" "
				"hotspot_x=\"10\" hotspot_y=\"20\"><uuid uuid=\"%1\"/>"
				"<names><name lang=\"en\">Lamp</name></names><description>"
				"<terminal x=\"0\" y=\"-20\" orientation=\"n\" uuid=\"%2\"/></description></definition>")
				.arg(QUuid::createUuid().toString(), QUuid::createUuid().toString());
		QDomDocument definition;
		QVERIFY(definition.setContent(xml));
		QVERIFY(project.embeddedElementCollection()->addElementDefinition(
					QStringLiteral("import"), QStringLiteral("lamp"), definition.documentElement()));
		const ElementsLocation embedded(QStringLiteral("embed://import/lamp.elmt"), &project);
		const QString written = dir.filePath(QStringLiteral("lamp.elmt"));
		QVERIFY2(ElementsCollectionWidget::saveSymbol(embedded, written, &error), qPrintable(error));
		QDomDocument back;
		QVERIFY(back.setContent(read(written)));
		QCOMPARE(back.documentElement().tagName(), QStringLiteral("definition"));
		QVERIFY(!back.documentElement().firstChildElement(QStringLiteral("description")).isNull());

		QVERIFY(!ElementsCollectionWidget::saveSymbol(
					ElementsLocation(QStringLiteral("common://nowhere/missing.elmt")),
					dir.filePath(QStringLiteral("missing.elmt")), &error));
	}
};

	/// The symbols of the repository's collection, set before the
	/// application starts: it reads the folder once
static void useTheCollection()
{
	QETApp::overrideCommonElementsDir(QStringLiteral(QET_ELEMENTS_DIR));
}

QET_APP_TEST_MAIN_PREPARED(tst_openingcopy, useTheCollection)
#include "tst_openingcopy.moc"
