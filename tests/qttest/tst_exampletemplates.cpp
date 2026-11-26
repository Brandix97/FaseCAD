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
	ExampleTemplates -- the example templates copied into the user's
	templates:

	* on the first start every example is copied, and can be written to;
	* an example the user deleted does not come back;
	* a file of the user with the same name is never replaced;
	* a new example shipped later is copied then.
*/

#include "branding/exampletemplates.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class TstExampleTemplates : public QObject
{
		Q_OBJECT

		QTemporaryDir m_dir;

		QString source() const {return m_dir.filePath(QStringLiteral("source"));}
		QString templates() const {return m_dir.filePath(QStringLiteral("templates"));}
		QString copy(const QString &name) const
		{
			return QDir(templates()).filePath(ExampleTemplates::folderName()
											  + QLatin1Char('/') + name);
		}

		void write(const QString &path, const QByteArray &content)
		{
			QFile file(path);
			QVERIFY(file.open(QIODevice::WriteOnly));
			file.write(content);
		}

	private slots:
		void initTestCase();
		void firstStartCopiesEverything();
		void deletedExampleStaysDeleted();
		void newExampleIsCopiedLater();
		void userFileIsKept();
};

void TstExampleTemplates::initTestCase()
{
	QCoreApplication::setOrganizationName(
				QStringLiteral("QElectroTech-tst_exampletemplates"));
	QCoreApplication::setApplicationName(QStringLiteral("tst_exampletemplates"));
	QSettings().clear();
	QVERIFY(m_dir.isValid());
	QDir().mkpath(source());
	write(QDir(source()).filePath(QStringLiteral("Star-delta start.qetmak")),
		  "<qet_macro/>");
	write(QDir(source()).filePath(QStringLiteral("Direct-on-line start.qetmak")),
		  "<qet_macro/>");
	write(QDir(source()).filePath(QStringLiteral("readme.txt")), "not a template");
}

void TstExampleTemplates::firstStartCopiesEverything()
{
	const QStringList copied = ExampleTemplates::install(source(), templates());
	QCOMPARE(copied, (QStringList{QStringLiteral("Direct-on-line start.qetmak"),
								  QStringLiteral("Star-delta start.qetmak")}));
	QVERIFY(QFile::exists(copy(QStringLiteral("Star-delta start.qetmak"))));
	QVERIFY(!QFile::exists(copy(QStringLiteral("readme.txt"))));
	QVERIFY(QFile::permissions(copy(QStringLiteral("Star-delta start.qetmak")))
			& QFileDevice::WriteOwner);
		//Nothing more on the next start
	QVERIFY(ExampleTemplates::install(source(), templates()).isEmpty());
}

void TstExampleTemplates::deletedExampleStaysDeleted()
{
	QVERIFY(QFile::remove(copy(QStringLiteral("Star-delta start.qetmak"))));
	QVERIFY(ExampleTemplates::install(source(), templates()).isEmpty());
	QVERIFY(!QFile::exists(copy(QStringLiteral("Star-delta start.qetmak"))));
}

void TstExampleTemplates::newExampleIsCopiedLater()
{
	write(QDir(source()).filePath(QStringLiteral("Analog IO card.qetmak")),
		  "<qet_macro/>");
	QCOMPARE(ExampleTemplates::install(source(), templates()),
			 QStringList{QStringLiteral("Analog IO card.qetmak")});
}

void TstExampleTemplates::userFileIsKept()
{
	write(QDir(source()).filePath(QStringLiteral("Digital IO card.qetmak")),
		  "<qet_macro>shipped</qet_macro>");
	write(copy(QStringLiteral("Digital IO card.qetmak")), "mine");
	QVERIFY(ExampleTemplates::install(source(), templates()).isEmpty());
	QFile file(copy(QStringLiteral("Digital IO card.qetmak")));
	QVERIFY(file.open(QIODevice::ReadOnly));
	QCOMPARE(file.readAll(), QByteArray("mine"));
}

QTEST_GUILESS_MAIN(TstExampleTemplates)
#include "tst_exampletemplates.moc"
