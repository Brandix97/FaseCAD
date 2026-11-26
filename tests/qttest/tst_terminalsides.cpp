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
#include "TerminalStrip/terminalfieldsupdate.h"
#include "TerminalStrip/terminalsides.h"
#include "factory/elementfactory.h"
#include "qetgraphicsitem/dynamicelementtextitem.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminal.h"
#include "qetgraphicsitem/terminalelement.h"
#include "qetinformation.h"
#include "qetproject.h"
#include "ui/elementinfowidget.h"

#include <QCheckBox>
#include <QDomDocument>
#include <QFile>
#include <QGraphicsSimpleTextItem>
#include <QImage>
#include <QLabel>
#include <QPainter>

/**
	The terminals of the panel collection (Brandon, 2026-11-16):

	 - the strip on the left of the circle, the number on the right: the
	   symbol has a field for each, a new project splits the label, and a
	   terminal showing its number only (the next one of a row) does not
	   show it on both sides;
	 - which connection point goes into the panel and which one out of
	   it: the lowest goes out, unless the terminal swaps them; the
	   Information tab says it and has the box; "Inside" and "Outside"
	   stand beside a selected terminal, in the views only;
	 - "Update elements from the collections" gives a terminal placed
	   from the old symbol the number field, and moves a field only where
	   nobody moved it by hand.
*/
class tst_terminalsides : public QObject
{
	Q_OBJECT

	int m_made = 0;

	static QString symbolFile()
	{
		return QStringLiteral(QET_ELEMENTS_DIR)
				+ QStringLiteral("/10_electric/05_iec_60617_cuadros/11_conexiones/borne.elmt");
	}

		/// The terminal of the collection, as it is now
	static QDomDocument newSymbol()
	{
		QDomDocument document;
		QFile file(symbolFile());
		if (file.open(QIODevice::ReadOnly)) {
			document.setContent(&file);
		}
		return document;
	}

		/// The terminal as it was before: its label field only
	static QDomDocument oldSymbol()
	{
		QDomDocument document = newSymbol();
		QDomElement description =
				document.documentElement().firstChildElement(QStringLiteral("description"));
		for (QDomElement text = description.firstChildElement(QStringLiteral("dynamic_text"));
			 !text.isNull();
			 text = text.nextSiblingElement(QStringLiteral("dynamic_text")))
		{
			if (text.firstChildElement(QStringLiteral("info_name")).text()
					== QETInformation::ELMT_TERMINAL_NUMBER) {
				description.removeChild(text);
				break;
			}
		}
		return document;
	}

	static DynamicElementTextItem *fieldOf(Element *element, const QString &info)
	{
		for (DynamicElementTextItem *text : element->dynamicTextItems()) {
			if (text->textFrom() == DynamicElementTextItem::ElementInfo
				&& text->infoName() == info) {
				return text;
			}
		}
		return nullptr;
	}

	static QDomElement labelFieldOf(QDomDocument &document)
	{
		QDomElement description =
				document.documentElement().firstChildElement(QStringLiteral("description"));
		for (QDomElement text = description.firstChildElement(QStringLiteral("dynamic_text"));
			 !text.isNull();
			 text = text.nextSiblingElement(QStringLiteral("dynamic_text"))) {
			if (text.firstChildElement(QStringLiteral("info_name")).text()
					== QETInformation::ELMT_LABEL) {
				return text;
			}
		}
		return QDomElement();
	}

		/// @p definition placed on the folio, labelled @p label
	TerminalElement *place(const QDomDocument &definition, const QString &label)
	{
		Diagram *diagram = QetAppTest::diagram();
		const QString name = QStringLiteral("terminal%1").arg(++m_made);
		if (!diagram->project()->embeddedElementCollection()->addElementDefinition(
				QStringLiteral("import"), name, definition.documentElement())) {
			return nullptr;
		}
		int state = 0;
		Element *element = ElementFactory::Instance()->createElement(
					ElementsLocation(QStringLiteral("embed://import/%1.elmt").arg(name),
									 diagram->project()),
					nullptr, &state);
		auto *terminal = qobject_cast<TerminalElement *>(element);
		if (state || !terminal) {
			delete element;
			return nullptr;
		}
		diagram->addItem(terminal);
		terminal->setPos(100 + 80 * m_made, 200);
		DiagramContext information = terminal->elementInformations();
		information.addValue(QETInformation::ELMT_LABEL, label);
		terminal->setElementInformations(information);
		return terminal;
	}

	static void swap(Element *element, bool swapped)
	{
		DiagramContext information = element->elementInformations();
		if (swapped) {
			information.addValue(TerminalSides::swappedKey(), QStringLiteral("true"));
		} else {
			information.remove(TerminalSides::swappedKey());
		}
		element->setElementInformations(information);
	}

	static Terminal *highest(Element *element)
	{
		Terminal *top = nullptr;
		for (Terminal *terminal : element->terminals()) {
			if (!top || terminal->dockConductor().y() < top->dockConductor().y()) {
				top = terminal;
			}
		}
		return top;
	}

private slots:
	void initTestCase()
	{
		QETProject::setBackupEnabled(false);
		QVERIFY(QetAppTest::diagram());
		QVERIFY2(!newSymbol().isNull(), qPrintable(symbolFile()));
	}

	void aNewProjectSplitsTheLabel()
	{
		QVERIFY(QetAppTest::diagram()->project()->terminalLabelSplit());
	}

	void theStripIsOnTheLeftAndTheNumberOnTheRight()
	{
		TerminalElement *terminal = place(newSymbol(), QStringLiteral("-XPERS:23"));
		QVERIFY(terminal);
		DynamicElementTextItem *label = fieldOf(terminal, QETInformation::ELMT_LABEL);
		DynamicElementTextItem *number = fieldOf(terminal, QETInformation::ELMT_TERMINAL_NUMBER);
		QVERIFY(label && number);
		QCOMPARE(label->toPlainText(), QStringLiteral("-XPERS"));
		QCOMPARE(number->toPlainText(), QStringLiteral("23"));
		QVERIFY(label->sceneBoundingRect().right() <= terminal->sceneBoundingRect().center().x());
		QVERIFY(number->sceneBoundingRect().left() >= terminal->sceneBoundingRect().center().x());

			//The next one of a row: its number, once, on the right
		terminal->setNumberOnly(true);
		QCOMPARE(terminal->labelForDisplay(), QString());
		QCOMPARE(terminal->terminalNumberForDisplay(), QStringLiteral("23"));
	}

	void theLowestSideGoesOutUnlessSwapped()
	{
		TerminalElement *terminal = place(newSymbol(), QStringLiteral("-X1:1"));
		QVERIFY(terminal);
		Terminal *top = highest(terminal);
		QVERIFY(top);

		TerminalSides::Reason reason = TerminalSides::Reason::None;
		TerminalSides::outsideIndex(terminal, &reason);
		QCOMPARE(int(reason), int(TerminalSides::Reason::Position));
		QCOMPARE(TerminalSides::inside(terminal), top);
		QVERIFY(TerminalSides::outside(terminal) != top);
		QCOMPARE(TerminalSides::placeOf(terminal, TerminalSides::inside(terminal)),
				 QStringLiteral("top"));
		QCOMPARE(TerminalSides::placeOf(terminal, TerminalSides::outside(terminal)),
				 QStringLiteral("bottom"));

		swap(terminal, true);
		QVERIFY(TerminalSides::isSwapped(terminal));
		QCOMPARE(TerminalSides::outside(terminal), top);
		QVERIFY(TerminalSides::inside(terminal) != top);
	}

	void aSelectedTerminalShowsItsSides()
	{
		TerminalElement *terminal = place(newSymbol(), QStringLiteral("-X2:1"));
		QVERIFY(terminal);
		QVERIFY(!terminal->insideMark());

		terminal->setSelected(true);
		QVERIFY(terminal->insideMark() && terminal->outsideMark());
		QCOMPARE(terminal->insideMark()->text(), QStringLiteral("Inside"));
		QCOMPARE(terminal->outsideMark()->text(), QStringLiteral("Outside"));
		QVERIFY(terminal->insideMark()->sceneBoundingRect().center().y()
				< terminal->outsideMark()->sceneBoundingRect().center().y());

		swap(terminal, true);
		QVERIFY(terminal->insideMark()->sceneBoundingRect().center().y()
				> terminal->outsideMark()->sceneBoundingRect().center().y());

			//A render without a view (a print, a PDF, an image) leaves
			//them out: drawn hidden or not, it is the same picture
		Diagram *diagram = QetAppTest::diagram();
		const QRectF area = terminal->sceneBoundingRect().adjusted(-40, -40, 40, 40);
		auto render = [diagram, &area]() {
			QImage image(area.size().toSize() * 2, QImage::Format_ARGB32);
			image.fill(Qt::white);
			QPainter painter(&image);
			diagram->render(&painter, QRectF(image.rect()), area);
			return image;
		};
		const QImage with_marks = render();
		terminal->insideMark()->setVisible(false);
		terminal->outsideMark()->setVisible(false);
		QCOMPARE(render(), with_marks);

		terminal->setSelected(false);
		QVERIFY(!terminal->insideMark() && !terminal->outsideMark());
	}

	void theInformationTabSaysWhichSideAndSwapsThem()
	{
		TerminalElement *terminal = place(newSymbol(), QStringLiteral("-X3:1"));
		QVERIFY(terminal);
		ElementInfoWidget widget(terminal);
		auto *box = widget.findChild<QCheckBox *>(QStringLiteral("m_outside_swapped_cb"));
		auto *sides = widget.findChild<QLabel *>(QStringLiteral("m_sides_lb"));
		QVERIFY(box && sides);
		QVERIFY(!box->isChecked());
		QVERIFY2(sides->text().contains(QStringLiteral("Outside the panel: bottom")),
				 qPrintable(sides->text()));

		box->setChecked(true);
		widget.apply();
		QVERIFY(TerminalSides::isSwapped(terminal));
		QVERIFY2(sides->text().contains(QStringLiteral("Outside the panel: top")),
				 qPrintable(sides->text()));
	}

	void anOldTerminalGetsTheNumberField()
	{
		const QDomDocument before = oldSymbol();
		TerminalElement *terminal = place(before, QStringLiteral("-X4:7"));
		QVERIFY(terminal);
		QVERIFY(!fieldOf(terminal, QETInformation::ELMT_TERMINAL_NUMBER));
		const QPointF label_at = fieldOf(terminal, QETInformation::ELMT_LABEL)->anchorPos();

		const QDomDocument after = newSymbol();
		QCOMPARE(TerminalFieldsUpdate::adopt(terminal, before.documentElement(),
											 after.documentElement()), 1);
		DynamicElementTextItem *number = fieldOf(terminal, QETInformation::ELMT_TERMINAL_NUMBER);
		QVERIFY(number);
		QCOMPARE(number->toPlainText(), QStringLiteral("7"));
		QVERIFY(number->uuid() != QUuid(after.documentElement()
										.firstChildElement(QStringLiteral("uuid"))
										.attribute(QStringLiteral("uuid"))));
			//Where the new symbol keeps it, the label stays
		QCOMPARE(fieldOf(terminal, QETInformation::ELMT_LABEL)->anchorPos(), label_at);
			//Done once: nothing left to do
		QCOMPARE(TerminalFieldsUpdate::adopt(terminal, after.documentElement(),
											 after.documentElement()), 0);
	}

	void aFieldMovedByHandStaysWhereItIs()
	{
		const QDomDocument before = oldSymbol();
		QDomDocument after = newSymbol();
		QDomElement label = labelFieldOf(after);
		label.setAttribute(QStringLiteral("y"),
						   label.attribute(QStringLiteral("y")).toDouble() - 10);

			//Not moved: it follows the symbol
		TerminalElement *kept = place(before, QStringLiteral("-X5:1"));
		QVERIFY(kept);
		const QPointF kept_at = fieldOf(kept, QETInformation::ELMT_LABEL)->anchorPos();
		TerminalFieldsUpdate::adopt(kept, before.documentElement(), after.documentElement());
		QCOMPARE(fieldOf(kept, QETInformation::ELMT_LABEL)->anchorPos(),
				 kept_at - QPointF(0, 10));

			//Moved by hand: it stays
		TerminalElement *moved = place(before, QStringLiteral("-X6:1"));
		QVERIFY(moved);
		DynamicElementTextItem *text = fieldOf(moved, QETInformation::ELMT_LABEL);
		text->setAnchorPos(text->anchorPos() + QPointF(-20, 0));
		const QPointF moved_at = text->anchorPos();
		TerminalFieldsUpdate::adopt(moved, before.documentElement(), after.documentElement());
		QCOMPARE(fieldOf(moved, QETInformation::ELMT_LABEL)->anchorPos(), moved_at);
	}
};

QET_APP_TEST_MAIN(tst_terminalsides)
#include "tst_terminalsides.moc"
