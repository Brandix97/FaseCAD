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
#include "qetapptest.h"

#include "TerminalStrip/physicalterminal.h"
#include "TerminalStrip/realterminal.h"
#include "TerminalStrip/terminalstripautoimport.h"
#include "TerminalStrip/terminalstriplabels.h"
#include "TerminalStrip/terminalstrip.h"
#include "TerminalStrip/terminalstripbridge.h"
#include "TerminalStrip/UndoCommand/bridgeterminalscommand.h"
#include "diagram.h"
#include "diagramcommands.h"
#include "diagramcontent.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminalelement.h"
#include "qetproject.h"
#include "terminals/faseterminal.h"
#include "terminals/terminalsettingswidget.h"
#include "ui/elementpropertieswidget.h"

#include <QComboBox>
#include <QUndoStack>

/**
	The terminal of the toolbar (FaseTerminal):

	 - its label is "-STRIP:NUMBER", read back into strip and number;
	 - its settings go into the fields the terminal manager uses, and the
	   terminal into the strip they name, made when missing, in one step
	   that undo takes back whole;
	 - the next number follows the highest one of the strip;
	 - its symbol is put into the project's collection once.

	fixtures/every_feature.qet has the strip X1 with the terminals
	891b70f2 and 4861e268.
*/
class tst_faseterminal : public QObject
{
	Q_OBJECT

	QETProject *m_project = nullptr;

	TerminalElement *terminal(const QString &uuid) const
	{
		for (Diagram *diagram : m_project->diagrams()) {
			for (Element *element : diagram->elements()) {
				if (element->uuid().toString() == uuid) {
					return qobject_cast<TerminalElement *>(element);
				}
			}
		}
		return nullptr;
	}

	TerminalStrip *strip(const QString &name) const
	{
		for (TerminalStrip *candidate : m_project->terminalStrip()) {
			if (candidate->name() == name) {
				return candidate;
			}
		}
		return nullptr;
	}

private slots:
	void init()
	{
		const QString fixture = QFINDTESTDATA("fixtures/every_feature.qet");
		QVERIFY(!fixture.isEmpty());
		m_project = new QETProject(fixture);
		QCOMPARE(m_project->state(), QETProject::Ok);
	}

	void cleanup()
	{
		delete m_project;
		m_project = nullptr;
	}

	void labelIsStripAndNumber()
	{
		QCOMPARE(FaseTerminal::label(QStringLiteral("XPERS"),
									 QStringLiteral("9")),
				 QStringLiteral("-XPERS:9"));
		QString name, number;
		FaseTerminal::parseLabel(QStringLiteral("-XPERS:9"), &name, &number);
		QCOMPARE(name, QStringLiteral("XPERS"));
		QCOMPARE(number, QStringLiteral("9"));
		FaseTerminal::parseLabel(QStringLiteral("X1:2"), &name, &number);
		QCOMPARE(name, QStringLiteral("X1"));
		QCOMPARE(number, QStringLiteral("2"));
			//The automatic assignment puts it into the same strip
		QCOMPARE(TerminalStripAutoImport::stripNameForLabel(
					 QStringLiteral("-XPERS:9")), QStringLiteral("XPERS"));
	}

	void settingsGoIntoTheTerminalAndItsStrip()
	{
		TerminalElement *element = terminal(
			QStringLiteral("{891b70f2-2822-41c3-9f94-28efb448b5c4}"));
		QVERIFY(element);
		const QSharedPointer<RealTerminal> real = element->realTerminal();
		QVERIFY(real);
		TerminalStrip *before_strip = real->parentStrip();
		QVERIFY(before_strip);
		const ElementData before = element->elementData();

		FaseTerminal::Settings settings;
		settings.strip = QStringLiteral("XNEW");
		settings.number = QStringLiteral("5");
		settings.color = FaseTerminal::colorChoices().first().spec;
		settings.type = ElementData::TTFuse;
		settings.section = QStringLiteral("2.5");
		QVERIFY(!strip(settings.strip));

		QUndoStack &undo = element->diagram()->undoStack();
		undo.push(new FaseTerminal::ChangeCommand(element, settings));

		QCOMPARE(element->elementData().m_informations
				 .value(QStringLiteral("label")).toString(),
				 QStringLiteral("-XNEW:5"));
		QCOMPARE(real->type(), ElementData::TTFuse);
		QCOMPARE(real->section(), 2.5);
		QVERIFY(strip(settings.strip));
		QCOMPARE(real->parentStrip(), strip(settings.strip));
		QCOMPARE(FaseTerminal::settingsOf(element), settings);
		QCOMPARE(FaseTerminal::nextNumber(m_project, settings.strip),
				 QStringLiteral("6"));

		undo.undo();
		QVERIFY(element->elementData() == before);
		QCOMPARE(real->parentStrip(), before_strip);
		QVERIFY(!strip(settings.strip));
	}

	void propertiesApplyTheTerminalTab()
	{
		TerminalElement *element = terminal(
			QStringLiteral("{891b70f2-2822-41c3-9f94-28efb448b5c4}"));
		QVERIFY(element);
		ElementPropertiesWidget properties(element);
		auto *form = properties.findChild<TerminalSettingsWidget *>();
		QVERIFY(form);
		FaseTerminal::Settings settings = form->settings();
		settings.strip = QStringLiteral("XAPPLY");
		settings.number = QStringLiteral("3");
		settings.section = QStringLiteral("4");
		form->setSettings(settings);
		properties.apply();

		QCOMPARE(element->elementData().m_informations
				 .value(QStringLiteral("label")).toString(),
				 QStringLiteral("-XAPPLY:3"));
		QCOMPARE(element->realTerminal()->section(), 4.0);
		QCOMPARE(element->realTerminal()->parentStrip(),
				 strip(QStringLiteral("XAPPLY")));
	}

	void livePanelAppliesAtOnce()
	{
		TerminalElement *element = terminal(
			QStringLiteral("{891b70f2-2822-41c3-9f94-28efb448b5c4}"));
		QVERIFY(element);
		ElementPropertiesWidget properties(element);
		properties.setLiveEdit(true);
		auto *form = properties.findChild<TerminalSettingsWidget *>();
		QVERIFY(form);
		QComboBox *color = nullptr;
		for (QComboBox *combo : form->findChildren<QComboBox *>()) {
			if (combo->findData(FaseTerminal::colorChoices().at(1).spec) >= 0) {
				color = combo;
			}
		}
		QVERIFY(color);
		color->setCurrentIndex(color->findData(
			FaseTerminal::colorChoices().at(1).spec));
		QCOMPARE(element->elementData().m_informations
				 .value(RealTerminal::colorInfoKey()).toString(),
				 FaseTerminal::colorChoices().at(1).spec);
	}

	void pastedTerminalTakesTheNextNumber()
	{
		TerminalElement *element = terminal(
			QStringLiteral("{891b70f2-2822-41c3-9f94-28efb448b5c4}"));
		QVERIFY(element);
		Diagram *diagram = element->diagram();
		FaseTerminal::Settings settings = FaseTerminal::settingsOf(element);
		settings.strip = QStringLiteral("XCOPY");
		settings.number = QStringLiteral("7");
		settings.color = FaseTerminal::colorChoices().at(1).spec;
		diagram->undoStack().push(
			new FaseTerminal::ChangeCommand(element, settings));

		diagram->clearSelection();
		element->setSelected(true);
		QDomDocument copy = diagram->toXml(false, true);
		DiagramContent content;
		QVERIFY(diagram->fromXml(copy, QPointF(400, 400), false, &content));
		QCOMPARE(content.m_elements.size(), 1);
		auto *pasted = qobject_cast<TerminalElement *>(
					content.m_elements.first());
		QVERIFY(pasted);
		diagram->undoStack().push(new PasteDiagramCommand(diagram, content));

		QCOMPARE(pasted->elementData().m_informations
				 .value(QStringLiteral("label")).toString(),
				 QStringLiteral("-XCOPY:8"));
		QCOMPARE(FaseTerminal::settingsOf(pasted).color, settings.color);
		QCOMPARE(pasted->realTerminal()->parentStrip(),
				 strip(QStringLiteral("XCOPY")));

		diagram->undoStack().undo();
		QVERIFY(!diagram->items().contains(pasted));
	}

	void sameNumberMakesLevels()
	{
		TerminalElement *first = terminal(
			QStringLiteral("{891b70f2-2822-41c3-9f94-28efb448b5c4}"));
		TerminalElement *second = terminal(
			QStringLiteral("{4861e268-d344-413e-a86f-034ea5ada16c}"));
		QVERIFY(first);
		QVERIFY(second);
		QUndoStack &undo = first->diagram()->undoStack();
		const int start = undo.index();

		FaseTerminal::Settings settings = FaseTerminal::settingsOf(first);
		settings.strip = QStringLiteral("XL");
		settings.number = QStringLiteral("5");
		settings.level = 1;
		undo.push(new FaseTerminal::ChangeCommand(first, settings));
		settings.level = 2;
		undo.push(new FaseTerminal::ChangeCommand(second, settings));

		const auto a = first->realTerminal();
		const auto b = second->realTerminal();
		QCOMPARE(a->physicalTerminal(), b->physicalTerminal());
		QCOMPARE(a->level(), 0);
		QCOMPARE(b->level(), 1);
		QCOMPARE(FaseTerminal::settingsOf(second).level, 2);

			//Another number: a terminal of its own again
		settings.number = QStringLiteral("6");
		settings.level = 1;
		undo.push(new FaseTerminal::ChangeCommand(second, settings));
		QVERIFY(a->physicalTerminal() != b->physicalTerminal());
		QCOMPARE(b->parentStrip(), strip(QStringLiteral("XL")));

		undo.setIndex(start);
		QVERIFY(!strip(QStringLiteral("XL")));
	}

	void twoBarsPerTerminal()
	{
		TerminalElement *first = terminal(
			QStringLiteral("{891b70f2-2822-41c3-9f94-28efb448b5c4}"));
		TerminalElement *second = terminal(
			QStringLiteral("{4861e268-d344-413e-a86f-034ea5ada16c}"));
		QVERIFY(first);
		QVERIFY(second);
		QUndoStack &undo = first->diagram()->undoStack();
		const int start = undo.index();

		FaseTerminal::Settings settings = FaseTerminal::settingsOf(first);
		settings.strip = QStringLiteral("XB");
		settings.number = QStringLiteral("1");
		undo.push(new FaseTerminal::ChangeCommand(first, settings));
		settings.number = QStringLiteral("2");
		undo.push(new FaseTerminal::ChangeCommand(second, settings));
		TerminalStrip *xb = strip(QStringLiteral("XB"));
		QVERIFY(xb);
		const QVector<QSharedPointer<RealTerminal>> both{
			first->realTerminal(), second->realTerminal()};

			//The first bar, then a second one beside it, then no more
		QVERIFY(xb->isBridgeable(both));
		undo.push(new BridgeTerminalsCommand(xb, both));
		QCOMPARE(xb->bridgesOf(both.first()).size(), 1);
		QVERIFY(xb->isBridgeable(both));
		undo.push(new BridgeTerminalsCommand(xb, both));
		const auto bars = xb->bridgesOf(both.first());
		QCOMPARE(bars.size(), 2);
		QVERIFY(bars.at(0)->track() != bars.at(1)->track());
		QVERIFY(!xb->isBridgeable(both));

			//Taking the second one off leaves the first
		QVERIFY(xb->canUnBridge(both));
		undo.undo();
		QCOMPARE(xb->bridgesOf(both.first()).size(), 1);
		QCOMPARE(xb->bridgesOf(both.last()).size(), 1);

		undo.setIndex(start);
	}

	void emptyStripIsTheOneOnTheLeft()
	{
			//891b70f2 stands on the left of 4861e268, on the same row
		TerminalElement *left = terminal(
			QStringLiteral("{891b70f2-2822-41c3-9f94-28efb448b5c4}"));
		TerminalElement *right = terminal(
			QStringLiteral("{4861e268-d344-413e-a86f-034ea5ada16c}"));
		QVERIFY(left);
		QVERIFY(right);
		QVERIFY(left->scenePos().x() < right->scenePos().x());
		QUndoStack &undo = left->diagram()->undoStack();

		FaseTerminal::Settings settings = FaseTerminal::settingsOf(left);
		settings.strip = QStringLiteral("XINH");
		settings.number = QStringLiteral("1");
		undo.push(new FaseTerminal::ChangeCommand(left, settings));
		settings.strip.clear();
		settings.number = QStringLiteral("2");
		undo.push(new FaseTerminal::ChangeCommand(right, settings));

		QCOMPARE(right->elementData().m_informations
				 .value(QStringLiteral("label")).toString(),
				 QStringLiteral("-XINH:2"));
		QCOMPARE(right->realTerminal()->parentStrip(),
				 strip(QStringLiteral("XINH")));

			//The strip is named once, at the first of the row
		TerminalStripLabels::refresh(left->diagram());
		QVERIFY(!left->showsTerminalNumberOnly());
		QVERIFY(right->showsTerminalNumberOnly());
		QCOMPARE(left->labelForDisplay(), QStringLiteral("-XINH:1"));
		QCOMPARE(right->labelForDisplay(), QStringLiteral("2"));
	}

	void pastedTerminalJoinsTheLevels()
	{
		TerminalElement *first = terminal(
			QStringLiteral("{891b70f2-2822-41c3-9f94-28efb448b5c4}"));
		TerminalElement *second = terminal(
			QStringLiteral("{4861e268-d344-413e-a86f-034ea5ada16c}"));
		QVERIFY(first);
		QVERIFY(second);
		Diagram *diagram = first->diagram();
		QUndoStack &undo = diagram->undoStack();

		FaseTerminal::Settings settings = FaseTerminal::settingsOf(first);
		settings.strip = QStringLiteral("XP");
		settings.number = QStringLiteral("7");
		settings.level = 1;
		undo.push(new FaseTerminal::ChangeCommand(first, settings));
		settings.level = 2;
		undo.push(new FaseTerminal::ChangeCommand(second, settings));
		QCOMPARE(first->realTerminal()->physicalTerminal()->realTerminalCount(), 2);

		diagram->clearSelection();
		second->setSelected(true);
		QDomDocument copy = diagram->toXml(false, true);
		DiagramContent content;
		QVERIFY(diagram->fromXml(copy, QPointF(500, 400), false, &content));
		auto *pasted = qobject_cast<TerminalElement *>(content.m_elements.first());
		QVERIFY(pasted);
		undo.push(new PasteDiagramCommand(diagram, content));
		QCOMPARE(pasted->realTerminal()->physicalTerminal()->realTerminalCount(), 1);

			//The number of the others and the third level, as the panel
			//gives them
		settings = FaseTerminal::settingsOf(pasted);
		settings.number = QStringLiteral("7");
		settings.level = 3;
		undo.push(new FaseTerminal::ChangeCommand(pasted, settings));
		QCOMPARE(pasted->realTerminal()->physicalTerminal(),
				 first->realTerminal()->physicalTerminal());
		QCOMPARE(first->realTerminal()->physicalTerminal()->realTerminalCount(), 3);
		QCOMPARE(pasted->realTerminal()->level(), 2);

			//The folio draws the marks again by itself, as on screen
		QTRY_COMPARE(first->levelMarkLevels(), 3);
		QTRY_COMPARE(pasted->levelMarkLevels(), 3);

			//Another level only: the marks follow too
		settings.level = 1;
		undo.push(new FaseTerminal::ChangeCommand(pasted, settings));
		QCOMPARE(pasted->realTerminal()->level(), 0);
		QTRY_COMPARE(first->levelMarkLevels(), 3);
	}

	void symbolIsPutIntoTheProjectOnce()
	{
		const ElementsLocation first = FaseTerminal::symbol(m_project);
		QVERIFY(first.exist());
		const ElementsLocation second = FaseTerminal::symbol(m_project);
		QCOMPARE(second.projectCollectionPath(), first.projectCollectionPath());
	}
};

QET_APP_TEST_MAIN(tst_faseterminal)
#include "tst_faseterminal.moc"
