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
#include "TerminalStrip/realterminal.h"
#include "TerminalStrip/terminalstrip.h"
#include "TerminalStrip/terminalstripbridge.h"
#include "TerminalStrip/wirebridges.h"
#include "bridge/bridge.h"
#include "diagram.h"
#include "diagramcontent.h"
#include "factory/elementfactory.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/terminal.h"
#include "qetgraphicsitem/terminalelement.h"
#include "qetproject.h"
#include "qetresult.h"
#include "undocommand/addgraphicsobjectcommand.h"
#include "undocommand/deleteqgraphicsitemcommand.h"

#include <QDomDocument>
#include <QTemporaryDir>
#include <QUndoStack>

/**
	A wire between two terminals of one strip, side by side, bridges them
	(Brandon, 2026-11-25), in the same undo step; deleting it takes the
	bridge back. The drawn bridges of a project saved with them become
	bridges of their strips when it is opened.
*/
class tst_wirebridges : public QObject
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

		/// A wire from the lower pin of @p a to the lower pin of @p b, drawn
		/// as the hand draws it: one undo step with its bridge
	static Conductor *wire(Diagram *folio, TerminalElement *a, TerminalElement *b)
	{
		auto *conductor = new Conductor(a->terminals().last(), b->terminals().last());
		auto *step = new QUndoCommand(QStringLiteral("wire"));
		new AddGraphicsObjectCommand(conductor, folio, QPointF(), step);
		WireBridges::bridge(conductor, step);
		folio->project()->undoStack()->push(step);
		return conductor;
	}

	static bool bridged(TerminalStrip *strip, const QList<TerminalElement *> &terminals)
	{
		QVector<QSharedPointer<RealTerminal>> real;
		for (TerminalElement *terminal : terminals) real << terminal->realTerminal();
		return !strip->commonBridge(real).isNull();
	}

	struct Strip
	{
		QETProject *project = nullptr;
		Diagram *folio = nullptr;
		TerminalStrip *strip = nullptr;
		QList<TerminalElement *> terminals;
	};

	Strip strip(QETProject &project, int count)
	{
		Strip made;
		made.project = &project;
		made.folio = project.addNewDiagram();
		made.strip = project.newTerminalStrip(QString(), QString(), QStringLiteral("X1"));
		for (int i = 0 ; i < count ; ++i) {
			TerminalElement *terminal = place(made.folio);
			if (!terminal || !made.strip->addTerminal(terminal)) {
				return Strip();
			}
			made.terminals << terminal;
		}
		return made;
	}

private slots:
	void initTestCase()
	{
		QETProject::setBackupEnabled(false);
	}

	void aWireBridgesTwoTerminalsSideBySide()
	{
		QETProject project;
		const Strip x1 = strip(project, 3);
		QVERIFY(x1.strip);
		const auto &t = x1.terminals;
		wire(x1.folio, t[0], t[1]);
		QVERIFY(bridged(x1.strip, {t[0], t[1]}));
		QVERIFY(!bridged(x1.strip, {t[1], t[2]}));
			//Set by the drawing of a wire, his to change in the editor
		QVERIFY(!x1.strip->commonBridge({t[0]->realTerminal(), t[1]->realTerminal()})->automatic());

			//Undone with the wire, back with it
		project.undoStack()->undo();
		QVERIFY(!bridged(x1.strip, {t[0], t[1]}));
		project.undoStack()->redo();
		QVERIFY(bridged(x1.strip, {t[0], t[1]}));
	}

	void aChainOfWiresIsOneBar()
	{
		QETProject project;
		const Strip x1 = strip(project, 3);
		QVERIFY(x1.strip);
		const auto &t = x1.terminals;
		wire(x1.folio, t[0], t[1]);
		wire(x1.folio, t[1], t[2]);
		QVERIFY(bridged(x1.strip, {t[0], t[1], t[2]}));
	}

	void everyOtherTerminalIsBridged()
	{
			//2-4-6-8 wired (Brandon, 2026-11-26): one comb with teeth
			//taken out, the terminals between left free
		QETProject project;
		const Strip x1 = strip(project, 8);
		QVERIFY(x1.strip);
		const auto &t = x1.terminals;
		wire(x1.folio, t[1], t[3]);
		wire(x1.folio, t[3], t[5]);
		wire(x1.folio, t[5], t[7]);
		QVERIFY(bridged(x1.strip, {t[1], t[3], t[5], t[7]}));
		for (int i : {0, 2, 4, 6})
			QVERIFY(!x1.strip->isBridged(t[i]->realTerminal()));
			//Undone one wire after the other, back to nothing
		project.undoStack()->undo();
		QVERIFY(bridged(x1.strip, {t[1], t[3], t[5]}));
		QVERIFY(!x1.strip->isBridged(t[7]->realTerminal()));
		project.undoStack()->undo();
		project.undoStack()->undo();
		for (int i = 0 ; i < 8 ; ++i)
			QVERIFY(!x1.strip->isBridged(t[i]->realTerminal()));
		project.undoStack()->redo();
		project.undoStack()->redo();
		project.undoStack()->redo();
		QVERIFY(bridged(x1.strip, {t[1], t[3], t[5], t[7]}));
	}

	void terminalsOfOtherStripsAreNotBridged()
	{
		QETProject project;
		const Strip x1 = strip(project, 3);
		QVERIFY(x1.strip);
		const auto &t = x1.terminals;

			//A wire to a terminal of no strip neither
		TerminalElement *loose = place(x1.folio);
		QVERIFY(loose);
		auto *other = new Conductor(t[0]->terminals().last(), loose->terminals().last());
		QVERIFY(!WireBridges::bridge(other, nullptr));
		delete other;
	}

	void deletingTheWireTakesItsBridge()
	{
		QETProject project;
		const Strip x1 = strip(project, 3);
		QVERIFY(x1.strip);
		const auto &t = x1.terminals;
		Conductor *first = wire(x1.folio, t[0], t[1]);
		wire(x1.folio, t[1], t[2]);
		QVERIFY(bridged(x1.strip, {t[0], t[1], t[2]}));

		DiagramContent content;
		content.m_conductors_to_move << first;
		project.undoStack()->push(new DeleteQGraphicsItemCommand(x1.folio, content));
			//The first terminal leaves, the others stay bridged by their wire
		QVERIFY(!bridged(x1.strip, {t[0], t[1]}));
		QVERIFY(bridged(x1.strip, {t[1], t[2]}));
		project.undoStack()->undo();
		QVERIFY(bridged(x1.strip, {t[0], t[1], t[2]}));
	}

	void aDrawnBridgeBecomesABridgeOfItsStrip()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString path = dir.filePath(QStringLiteral("drawn.qet"));
		{
			QETProject project;
			const Strip x1 = strip(project, 3);
			QVERIFY(x1.strip);
			auto landing = [](Element *element) {
				BridgeTerminal terminal;
				terminal.element = element->uuid();
				terminal.terminal = element->terminals().first()->stableUuid().toString();
				return terminal;
			};
			auto *bridge = new Bridge(&project);
			QVERIFY(project.addBridge(bridge));
			bridge->setTerminals({landing(x1.terminals[1]), landing(x1.terminals[2])});
			project.setFilePath(path);
			QVERIFY(project.write().isOk());
		}
		QETProject opened(path);
		QCOMPARE(opened.state(), QETProject::Ok);
		QVERIFY(opened.bridges().isEmpty());
		QCOMPARE(opened.terminalStrip().size(), 1);
		TerminalStrip *strip = opened.terminalStrip().first();
		QCOMPARE(strip->physicalTerminalCount(), 3);
		const auto real = strip->realTerminals();
		QCOMPARE(real.size(), 3);
		const auto bar = strip->commonBridge({real.at(1), real.at(2)});
		QVERIFY(bar);
		QVERIFY(!bar->automatic());
		QVERIFY(!strip->commonBridge({real.at(0), real.at(1)}));
	}
};

QET_APP_TEST_MAIN(tst_wirebridges)
#include "tst_wirebridges.moc"
