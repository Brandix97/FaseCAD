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
#include "editor/elementformatpattern.h"
#include "editor/elementscene.h"
#include "editor/graphicspart/partline.h"
#include "editor/graphicspart/partrectangle.h"
#include "editor/graphicspart/parttext.h"
#include "editor/graphicspart/partdynamictextfield.h"

#include <QtTest>
#include <QUndoStack>

/**
	Copy format and Apply format of the element editor, and its A4 sheet.
*/
class TestElementFormat : public QObject
{
	Q_OBJECT

private slots:
	void theFormatGoesFromALineToARectangle()
	{
		PartLine line(nullptr);
		line.setLineStyle(CustomElementGraphicPart::DashedStyle);
		line.setLineWeight(CustomElementGraphicPart::UltraWeight);
		line.setColor(CustomElementGraphicPart::RedColor);
		PartRectangle rectangle(nullptr);
		rectangle.setFilling(CustomElementGraphicPart::BlueFilling);

		const QVariantHash pattern = ElementFormatPattern::take(&line);
		QVERIFY(pattern.contains(QStringLiteral("line_style")));
		QVERIFY(pattern.contains(QStringLiteral("color")));

		QUndoStack stack;
		stack.push(ElementFormatPattern::applyCommand({&rectangle}, pattern));
		QCOMPARE(rectangle.lineStyle(), CustomElementGraphicPart::DashedStyle);
		QCOMPARE(rectangle.lineWeight(), CustomElementGraphicPart::UltraWeight);
		QCOMPARE(rectangle.color(), CustomElementGraphicPart::RedColor);
			//The filling of the line goes too: a line has none
		QCOMPARE(rectangle.filling(), line.filling());

			//One step of undo gives it all back
		stack.undo();
		QCOMPARE(rectangle.lineStyle(), CustomElementGraphicPart::NormalStyle);
		QCOMPARE(rectangle.filling(), CustomElementGraphicPart::BlueFilling);
	}

	void aTextTakesOnlyWhatATextHas()
	{
		PartText source(nullptr);
		QFont font = source.font();
		font.setPointSize(17);
		font.setBold(true);
		source.setFont(font);
		source.setDefaultTextColor(Qt::darkGreen);
		PartText target(nullptr);
		PartLine line(nullptr);
		const auto line_color = line.color();

		QUndoStack stack;
		stack.push(ElementFormatPattern::applyCommand(
					   {&target, &line}, ElementFormatPattern::take(&source)));
		QCOMPARE(target.font().pointSize(), 17);
		QVERIFY(target.font().bold());
		QCOMPARE(target.defaultTextColor(), QColor(Qt::darkGreen));
			//A QColor is not the colour of a line
		QCOMPARE(line.color(), line_color);
	}

		/// The texts of a symbol are mostly dynamic texts: their font, size,
		/// colour and frame go from one to the other, and to a plain text
	void aDynamicTextGivesItsFormat()
	{
		PartDynamicTextField source(nullptr);
		QFont font = source.font();
		font.setPointSize(13);
		font.setItalic(true);
		source.setFont(font);
		source.setColor(Qt::blue);
		source.setFrame(true);
		PartDynamicTextField target(nullptr);
		PartText plain(nullptr);

		QUndoStack stack;
		QUndoCommand *command = ElementFormatPattern::applyCommand(
					{&target, &plain}, ElementFormatPattern::take(&source));
		QVERIFY(command);
		stack.push(command);
		QCOMPARE(target.font().pointSize(), 13);
		QVERIFY(target.font().italic());
		QCOMPARE(target.color(), QColor(Qt::blue));
		QVERIFY(target.frame());
		QCOMPARE(plain.font().pointSize(), 13);
		QCOMPARE(plain.defaultTextColor(), QColor(Qt::blue));
		stack.undo();
		QVERIFY(!target.frame());
	}

	void nothingToChangeIsNoCommand()
	{
		PartLine a(nullptr), b(nullptr);
		QVERIFY(!ElementFormatPattern::applyCommand({&b}, ElementFormatPattern::take(&a)));
		QVERIFY(!ElementFormatPattern::applyCommand({&b}, QVariantHash()));
	}

	void theCopiedFormatIsShared()
	{
		ElementFormatPattern::setCopied(QVariantHash());
		QVERIFY(!ElementFormatPattern::hasCopied());
		PartLine line(nullptr);
		ElementFormatPattern::setCopied(ElementFormatPattern::take(&line));
		QVERIFY(ElementFormatPattern::hasCopied());
	}

	void theSheetIsAnA4()
	{
			//297 x 210 mm at 96 px an inch, as the paper sizes of folios
		QCOMPARE(ElementScene::a4SheetSize(), QSizeF(1123, 794));
	}
};

QET_APP_TEST_MAIN(TestElementFormat)
#include "tst_elementformat.moc"
