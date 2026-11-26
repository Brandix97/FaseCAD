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
	BrandingTranslator -- FaseCAD where the interface says QElectroTech:

	* an English text and a translated one are both rebranded;
	* a text without the name is left to the other translators;
	* the team and the web address keep their name, and so does the
	  About window;
	* the plural form chosen by the other translator is kept.
*/

#include "branding/brandingtranslator.h"

#include <QtTest>

	///A translator giving one Spanish text, as qet_es would
class FakeTranslator : public QTranslator
{
	public:
		QString translate(const char *context, const char *source,
						  const char *, int n) const override
		{
			if (qstrcmp(context, "Ctx") == 0
					&& qstrcmp(source, "Welcome to QElectroTech") == 0) {
				return QStringLiteral("Bienvenido a QElectroTech");
			}
			if (qstrcmp(source, "%n project(s) open in QElectroTech") == 0) {
				return n == 1 ? QStringLiteral("%n proyecto abierto en QElectroTech")
							  : QStringLiteral("%n proyectos abiertos en QElectroTech");
			}
			return QString();
		}
		bool isEmpty() const override {return false;}
};

class TstBrandingTranslator : public QObject
{
		Q_OBJECT

	private slots:
		void rebrandsKeepingCredits();
		void translatedAndSourceTexts();
		void plainTextsAreLeftAlone();
		void throughQCoreApplication();
};

void TstBrandingTranslator::rebrandsKeepingCredits()
{
	QCOMPARE(BrandingTranslator::rebrand(QStringLiteral("Quit QElectroTech")),
			 QStringLiteral("Quit FaseCAD"));
	QCOMPARE(BrandingTranslator::rebrand(QStringLiteral("The QElectroTech Team")),
			 QStringLiteral("The QElectroTech Team"));
	QCOMPARE(BrandingTranslator::rebrand(QStringLiteral("the QElectroTech team")),
			 QStringLiteral("the QElectroTech team"));
	QCOMPARE(BrandingTranslator::rebrand(QStringLiteral("https://qelectrotech.org")),
			 QStringLiteral("https://qelectrotech.org"));
		//The short name inside a phrase, in any language
	QCOMPARE(BrandingTranslator::rebrand(QStringLiteral("QET Collection")),
			 QStringLiteral("FaseCAD Collection"));
	QCOMPARE(BrandingTranslator::rebrand(QStringLiteral("Cajetines QET")),
			 QStringLiteral("Cajetines FaseCAD"));
		//QET alone is a sample text, QET_ElementScaler another program
	QCOMPARE(BrandingTranslator::rebrand(QStringLiteral("QET")),
			 QStringLiteral("QET"));
	QCOMPARE(BrandingTranslator::rebrand(QStringLiteral("QET_ElementScaler: help")),
			 QStringLiteral("QET_ElementScaler: help"));
	QCOMPARE(BrandingTranslator::rebrand(QStringLiteral("Open a .qet file")),
			 QStringLiteral("Open a .qet file"));
}

void TstBrandingTranslator::translatedAndSourceTexts()
{
	FakeTranslator spanish;
	BrandingTranslator branding;
	branding.setBases({&spanish});

	QCOMPARE(branding.translate("Ctx", "Welcome to QElectroTech"),
			 QStringLiteral("Bienvenido a FaseCAD"));
		//Not translated: the English source, rebranded
	QCOMPARE(branding.translate("Other", "Close QElectroTech"),
			 QStringLiteral("Close FaseCAD"));
		//The credits of the About window stay
	QVERIFY(branding.translate("AboutQETDialog",
							   "QElectroTech, an application").isEmpty());
		//The plural form the translator chose
	QCOMPARE(branding.translate("Ctx", "%n project(s) open in QElectroTech",
								nullptr, 3),
			 QStringLiteral("%n proyectos abiertos en FaseCAD"));
}

void TstBrandingTranslator::plainTextsAreLeftAlone()
{
	FakeTranslator spanish;
	BrandingTranslator branding;
	branding.setBases({&spanish});
	QVERIFY(branding.translate("Ctx", "Save the sheet").isEmpty());
}

void TstBrandingTranslator::throughQCoreApplication()
{
	FakeTranslator spanish;
	BrandingTranslator branding;
	branding.setBases({&spanish});
	QCoreApplication::installTranslator(&spanish);
	QCoreApplication::installTranslator(&branding);

	QCOMPARE(QCoreApplication::translate("Ctx", "Welcome to QElectroTech"),
			 QStringLiteral("Bienvenido a FaseCAD"));
	QCOMPARE(QCoreApplication::translate("Ctx", "%n project(s) open in QElectroTech",
										 nullptr, 1),
			 QStringLiteral("1 proyecto abierto en FaseCAD"));
	QCOMPARE(QCoreApplication::translate("Ctx", "Save the sheet"),
			 QStringLiteral("Save the sheet"));

	QCoreApplication::removeTranslator(&branding);
	QCoreApplication::removeTranslator(&spanish);
}

QTEST_GUILESS_MAIN(TstBrandingTranslator)
#include "tst_brandingtranslator.moc"
