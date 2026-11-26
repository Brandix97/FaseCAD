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
#include "TerminalStrip/physicalterminal.h"
#include "TerminalStrip/realterminal.h"
#include "TerminalStrip/terminalstrip.h"
#include "TerminalStrip/terminalstripbridge.h"
#include "bridge/bridge.h"
#include "diagram.h"
#include "diagramcontent.h"
#include "factory/elementfactory.h"
#include "qetgraphicsitem/terminal.h"
#include "qetgraphicsitem/terminalelement.h"
#include "qetproject.h"
#include "undocommand/deleteqgraphicsitemcommand.h"
#include "undocommand/removediagramcommand.h"

#include <QDomDocument>
#include <QUndoStack>

/**
	A terminal deleted from the folio leaves its strip (Brandon,
	2026-11-16): the editor of the strips and the strips drawn on the
	folios list it no more, and it is not saved with the strip. Undone,
	it comes back where it stood, at its level and in its bridge. A sheet
	deleted takes its terminals out of the strips the same way.
*/
class tst_deletedterminals : public QObject
{
	Q_OBJECT

	int m_made = 0;

	TerminalElement *place(Diagram *diagram)
	{
		const QString name = QStringLiteral("terminal%1").arg(++m_made);
		const QString xml = QStringLiteral(
				"<definition type=\"element\" link_type=\"terminal\" "
				"width=\"20\" height=\"40\" hotspot_x=\"10\" hotspot_y=\"20\">"
				"<uuid uuid=\"%1\"/>"
				"<names><name lang=\"en\">Test terminal</name></names>"
				"<kindInformations><kindInformation name=\"type\">generic</kindInformation>"
				"</kindInformations>"
				"<description><circle x=\"-3\" y=\"-3\" diameter=\"6\" antialias=\"true\" "
				"style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
				"<terminal x=\"0\" y=\"-20\" orientation=\"n\" uuid=\"%2\"/>"
				"<terminal x=\"0\" y=\"20\" orientation=\"s\" uuid=\"%3\"/>"
				"</description></definition>")
				.arg(QUuid::createUuid().toString(), QUuid::createUuid().toString(),
					 QUuid::createUuid().toString());
		QDomDocument definition;
		if (!definition.setContent(xml)
			|| !diagram->project()->embeddedElementCollection()->addElementDefinition(
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
		terminal->setPos(100 + 40 * m_made, 200);
		return terminal;
	}

		/// The strip, terminal by terminal and level by level
	static QList<QList<Element *>> layoutOf(TerminalStrip *strip)
	{
		QList<QList<Element *>> layout;
		for (const auto &physical : strip->physicalTerminal()) {
			QList<Element *> levels;
			for (const auto &real_t : physical->realTerminals()) {
				levels << real_t->element();
			}
			layout << levels;
		}
		return layout;
	}

	static void remove(Diagram *diagram, const QList<Element *> &elements)
	{
		DiagramContent content;
		content.m_elements = elements;
		diagram->project()->undoStack()->push(new DeleteQGraphicsItemCommand(diagram, content));
	}

	static QString savedStrip(TerminalStrip *strip)
	{
		QDomDocument document;
		document.appendChild(strip->toXml(document));
		return document.toString();
	}

private slots:
	void initTestCase()
	{
		QETProject::setBackupEnabled(false);
	}

	void aDeletedTerminalLeavesItsStrip()
	{
		QETProject project;
		Diagram *folio = project.addNewDiagram();
		TerminalElement *a = place(folio);
		TerminalElement *b = place(folio);
		TerminalElement *c = place(folio);
		QVERIFY(a && b && c);
		TerminalStrip *strip = project.newTerminalStrip(QString(), QString(), QStringLiteral("X1"));
		QVERIFY(strip->addTerminal(a) && strip->addTerminal(b) && strip->addTerminal(c));
		const QList<QList<Element *>> before {{a}, {b}, {c}};
		QCOMPARE(layoutOf(strip), before);

		int told = 0;
		connect(strip, &TerminalStrip::orderChanged, this, [&told] {++told;});
		remove(folio, {b});
		const QList<QList<Element *>> after {{a}, {c}};
		QCOMPARE(layoutOf(strip), after);
		QVERIFY(!b->parentTerminalStrip());
		QVERIFY(told > 0);
		QVERIFY(!savedStrip(strip).contains(b->uuid().toString(QUuid::WithoutBraces)));
		QVERIFY(savedStrip(strip).contains(a->uuid().toString(QUuid::WithoutBraces)));

		project.undoStack()->undo();
		QCOMPARE(layoutOf(strip), before);
		QCOMPARE(b->parentTerminalStrip(), strip);

		project.undoStack()->redo();
		QCOMPARE(layoutOf(strip), after);
	}

	void theFirstAndTheLastComeBackInPlace()
	{
		QETProject project;
		Diagram *folio = project.addNewDiagram();
		TerminalElement *a = place(folio);
		TerminalElement *b = place(folio);
		TerminalElement *c = place(folio);
		TerminalElement *d = place(folio);
		QVERIFY(a && b && c && d);
		TerminalStrip *strip = project.newTerminalStrip(QString(), QString(), QStringLiteral("X2"));
		for (Element *terminal : {static_cast<Element *>(a), static_cast<Element *>(b),
								  static_cast<Element *>(c), static_cast<Element *>(d)}) {
			QVERIFY(strip->addTerminal(terminal));
		}
		const QList<QList<Element *>> before {{a}, {b}, {c}, {d}};

		remove(folio, {d, a, c});
		const QList<QList<Element *>> after {{b}};
		QCOMPARE(layoutOf(strip), after);

		project.undoStack()->undo();
		QCOMPARE(layoutOf(strip), before);
	}

	void aLevelGoesAndComesBack()
	{
		QETProject project;
		Diagram *folio = project.addNewDiagram();
		TerminalElement *a = place(folio);
		TerminalElement *b = place(folio);
		TerminalElement *c = place(folio);
		QVERIFY(a && b && c);
		TerminalStrip *strip = project.newTerminalStrip(QString(), QString(), QStringLiteral("X3"));
		QVERIFY(strip->addTerminal(a) && strip->addTerminal(b) && strip->addTerminal(c));
		QVERIFY(strip->groupTerminals(a->realTerminal()->physicalTerminal(), {b->realTerminal()}));
		const QList<QList<Element *>> before {{a, b}, {c}};
		QCOMPARE(layoutOf(strip), before);

			//The editor refuses it, the command still keeps the strip right
		remove(folio, {b});
		const QList<QList<Element *>> after {{a}, {c}};
		QCOMPARE(layoutOf(strip), after);
		QVERIFY(!b->parentTerminalStrip());

		project.undoStack()->undo();
		QCOMPARE(layoutOf(strip), before);
		QCOMPARE(b->realTerminal()->level(), 1);
	}

	void aBridgeIsWholeAgainOnUndo()
	{
		QETProject project;
		Diagram *folio = project.addNewDiagram();
		TerminalElement *a = place(folio);
		TerminalElement *b = place(folio);
		TerminalElement *c = place(folio);
		QVERIFY(a && b && c);
		TerminalStrip *strip = project.newTerminalStrip(QString(), QString(), QStringLiteral("X4"));
		QVERIFY(strip->addTerminal(a) && strip->addTerminal(b) && strip->addTerminal(c));
		const QVector<QSharedPointer<RealTerminal>> all {a->realTerminal(), b->realTerminal(),
														 c->realTerminal()};
		QVERIFY(strip->setBridge(all));
		const auto bridge = strip->isBridged(a->realTerminal());
		QVERIFY(bridge);

		remove(folio, {b});
		const QVector<QSharedPointer<RealTerminal>> left {a->realTerminal(), c->realTerminal()};
		QCOMPARE(bridge->realTerminals(), left);

		project.undoStack()->undo();
		QCOMPARE(bridge->realTerminals(), all);
		QCOMPARE(strip->isBridged(b->realTerminal()), bridge);
	}

	void aDeletedSheetTakesItsTerminalsAlong()
	{
		QETProject project;
		Diagram *first = project.addNewDiagram();
		Diagram *second = project.addNewDiagram();
		TerminalElement *a = place(first);
		TerminalElement *b = place(second);
		TerminalElement *c = place(first);
		TerminalElement *d = place(second);
		QVERIFY(a && b && c && d);
		TerminalStrip *strip = project.newTerminalStrip(QString(), QString(), QStringLiteral("X5"));
		for (Element *terminal : {static_cast<Element *>(a), static_cast<Element *>(b),
								  static_cast<Element *>(c), static_cast<Element *>(d)}) {
			QVERIFY(strip->addTerminal(terminal));
		}
		const QList<QList<Element *>> before {{a}, {b}, {c}, {d}};

		project.undoStack()->push(new RemoveDiagramCommand(&project, second));
		const QList<QList<Element *>> after {{a}, {c}};
		QCOMPARE(layoutOf(strip), after);

		project.undoStack()->undo();
		QCOMPARE(layoutOf(strip), before);
		QVERIFY(project.diagrams().contains(second));
	}

	void aDrawnBridgeLetsGoOfADeletedTerminal()
	{
		QETProject project;
		Diagram *folio = project.addNewDiagram();
		TerminalElement *a = place(folio);
		TerminalElement *b = place(folio);
		TerminalElement *c = place(folio);
		QVERIFY(a && b && c);
		auto landing = [](Element *element) {
			BridgeTerminal terminal;
			terminal.element = element->uuid();
			terminal.terminal = element->terminals().first()->stableUuid().toString();
			return terminal;
		};
		auto *bridge = new Bridge(&project);
		QVERIFY(project.addBridge(bridge));
		const QList<BridgeTerminal> all {landing(a), landing(b), landing(c)};
		bridge->setTerminals(all);

			//Nothing to unbridge first: the bridge goes on over the others
		remove(folio, {b});
		const QList<BridgeTerminal> left {landing(a), landing(c)};
		QCOMPARE(bridge->landings(), left);

		project.undoStack()->undo();
		QCOMPARE(bridge->landings(), all);
		project.undoStack()->redo();
		QCOMPARE(bridge->landings(), left);
	}

	void aFreeTerminalAddsNothing()
	{
		QETProject project;
		Diagram *folio = project.addNewDiagram();
		TerminalElement *a = place(folio);
		QVERIFY(a);
		QVERIFY(!a->parentTerminalStrip());
		DiagramContent content;
		content.m_elements << a;
		DeleteQGraphicsItemCommand command(folio, content);
		QCOMPARE(command.childCount(), 0);
	}
};

QET_APP_TEST_MAIN(tst_deletedterminals)
#include "tst_deletedterminals.moc"
