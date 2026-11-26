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
#include "ElementsCollection/elementscollectionmodel.h"
#include "ElementsCollection/elementslocation.h"

#include <QDomDocument>
#include <QFile>
#include <QMimeData>
#include <QSignalSpy>

/**
	The common collection, the one FaseCAD comes with, is not protected
	as in QElectroTech:

	 - a symbol of it is written, and so is a new one in one of its
	   folders;
	 - a symbol can be dropped into one of its folders;
	 - only a file that cannot be written (a read-only installation)
	   stays read only.
*/
class tst_commoncollectionedit : public QObject
{
	Q_OBJECT

	const QString m_symbol = QStringLiteral("common://test/symbol.elmt");

	static QDomDocument definition(const QString &name)
	{
		QDomDocument document;
		document.setContent(QStringLiteral(
			"<definition type=\"element\" width=\"20\" height=\"20\""
			" hotspot_x=\"10\" hotspot_y=\"10\" link_type=\"simple\""
			" version=\"0.100.0\">"
			"<uuid uuid=\"{2b4f7f3e-1c2d-4a5b-9e8f-0a1b2c3d4e5f}\"/>"
			"<names><name lang=\"en\">%1</name></names>"
			"<description><line x1=\"-5\" y1=\"0\" x2=\"5\" y2=\"0\""
			" length1=\"1.5\" length2=\"1.5\" end1=\"none\" end2=\"none\""
			" antialias=\"false\" style=\"line-style:normal;"
			"line-weight:normal;filling:none;color:black\"/>"
			"</description></definition>").arg(name));
		return document;
	}

	static QByteArray contents(const QString &path)
	{
		QFile file(path);
		return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
	}

private slots:
	void cleanup()
	{
		QFile::setPermissions(ElementsLocation(m_symbol).fileSystemPath(),
							  QFile::ReadOwner | QFile::WriteOwner);
	}

	void theSymbolIsInTheCommonCollection()
	{
		const ElementsLocation location(m_symbol);
		QVERIFY(location.isCommonCollection());
		QVERIFY(location.exist());
	}

	void aSymbolIsWritten()
	{
		const ElementsLocation location(m_symbol);
		QVERIFY(location.isWritable());
		QVERIFY(location.setXml(definition(QStringLiteral("Changed"))));
		QVERIFY(contents(location.fileSystemPath()).contains("Changed"));
	}

	void aNewSymbolIsWritten()
	{
		const ElementsLocation added(QStringLiteral("common://test/new.elmt"));
		QVERIFY(!added.exist());
		QVERIFY(added.isWritable());
		QVERIFY(added.setXml(definition(QStringLiteral("Added"))));
		QVERIFY(added.exist());
		QVERIFY(QFile::remove(added.fileSystemPath()));
	}

	void aReadOnlyFileStaysReadOnly()
	{
		const ElementsLocation location(m_symbol);
		const QByteArray before = contents(location.fileSystemPath());
		QVERIFY(QFile::setPermissions(location.fileSystemPath(),
									  QFile::ReadOwner | QFile::ReadUser));
		QVERIFY(!location.isWritable());
		QVERIFY(!location.setXml(definition(QStringLiteral("Refused"))));
		QCOMPARE(contents(location.fileSystemPath()), before);
	}

	void aSymbolIsDroppedIntoAFolder()
	{
		ElementsCollectionModel model;
		QSignalSpy loaded(&model, &ElementsCollectionModel::loadingFinished);
		model.loadCollections(true, false, false, {});
		QVERIFY(loaded.count() || loaded.wait(30000));

		const QModelIndex folder = model.indexFromLocation(
					ElementsLocation(QStringLiteral("common://test")));
		QVERIFY(folder.isValid());

		QMimeData data;
		data.setData(QStringLiteral("application/x-qet-element-uri"),
					 QByteArray("custom://other.elmt"));
		data.setText(QStringLiteral("custom://other.elmt"));
		QVERIFY(model.canDropMimeData(&data, Qt::CopyAction, -1, -1, folder));
	}
};

	/// A common collection of one folder and one symbol
static void useACollectionOfOurOwn()
{
	static QTemporaryDir dir;
	QDir(dir.path()).mkpath(QStringLiteral("test"));
	QFile folder(dir.filePath(QStringLiteral("test/qet_directory")));
	if (folder.open(QIODevice::WriteOnly)) {
		folder.write("<qet-directory><names>"
					 "<name lang=\"en\">Test</name></names></qet-directory>\n");
	}
	QFile symbol(dir.filePath(QStringLiteral("test/symbol.elmt")));
	if (symbol.open(QIODevice::WriteOnly)) {
		symbol.write(
			"<definition type=\"element\" width=\"20\" height=\"20\""
			" hotspot_x=\"10\" hotspot_y=\"10\" link_type=\"simple\""
			" version=\"0.100.0\">"
			"<uuid uuid=\"{2b4f7f3e-1c2d-4a5b-9e8f-0a1b2c3d4e5f}\"/>"
			"<names><name lang=\"en\">Original</name></names>"
			"<description/></definition>\n");
	}
	QETApp::overrideCommonElementsDir(dir.path());
}

QET_APP_TEST_MAIN_PREPARED(tst_commoncollectionedit, useACollectionOfOurOwn)
#include "tst_commoncollectionedit.moc"
