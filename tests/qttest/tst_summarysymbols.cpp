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
#include "ElementsCollection/xmlelementcollection.h"
#include "dataBase/projectdatabase.h"
#include "devicexrefs.h"
#include "diagram.h"
#include "factory/elementfactory.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/independenttextitem.h"
#include "qetinformation.h"
#include "qetproject.h"
#include "qetresult.h"
#include "summarysymbols.h"
#include "structureboxes.h"
#include "qetgraphicsitem/qetshapeitem.h"

#include <QDomDocument>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QtTest>

/**
	Summary folios, function texts and summary symbols (Brandon,
	2026-11-28): a PLC or network summary folio keeps its kind; a summary
	symbol goes on its own kind of folio only; a function text gives its
	function to what is over and under it, within its width, unless it has
	its own; a new PLC summary folio has an example whose four symbols find
	the terminals of the device -KF1 by label and terminal, and show their
	function texts and places.
*/
class tst_summarysymbols : public QObject
{
	Q_OBJECT

		/// A device -KF1 with the terminals X1:1 to X1:4 on @p folio
	static Element *device(Diagram *folio, const QPointF &pos)
	{
		QDomDocument definition;
		definition.setContent(QStringLiteral(
			R"(<definition type="element" link_type="simple" width="60" height="20" hotspot_x="0" hotspot_y="10">
<names><name lang="en">io</name></names>
<description>
<terminal x="0" y="0" orientation="n" name="X1:1" uuid="{11111111-1111-4111-8111-111111111111}"/>
<terminal x="10" y="0" orientation="n" name="X1:2" uuid="{22222222-2222-4222-8222-222222222222}"/>
<terminal x="20" y="0" orientation="n" name="X1:3" uuid="{33333333-3333-4333-8333-333333333333}"/>
<terminal x="30" y="0" orientation="n" name="X1:4" uuid="{44444444-4444-4444-8444-444444444444}"/>
</description></definition>)"));
		auto *collection = folio->project()->embeddedElementCollection();
		if (!collection->addElementDefinition(QStringLiteral("import"), QStringLiteral("io"),
											  definition.documentElement())) {
			return nullptr;
		}
		int state = 0;
		Element *element = ElementFactory::Instance()->createElement(
			ElementsLocation(QStringLiteral("embed://import/io.elmt"), folio->project()),
			nullptr, &state);
		if (state || !element) {
			return nullptr;
		}
		folio->addItem(element);
		element->setPos(pos);
		DiagramContext information = element->elementInformations();
		information.addValue(QETInformation::ELMT_LABEL, QStringLiteral("-KF1"));
		element->setElementInformations(information);
		return element;
	}

private slots:
	void initTestCase()
	{
		QETProject::setBackupEnabled(false);
	}

	void aSummaryFolioKeepsItsKind()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		QETProject project;
		project.addNewDiagram();
		Diagram *plc = project.addNewSummaryDiagram(Diagram::FolioKind::PlcSummary);
		Diagram *network = project.addNewSummaryDiagram(Diagram::FolioKind::NetworkSummary);
		QVERIFY(plc && network);
		QVERIFY(plc->acceptsSymbol(Element::PlcSummary));
		QVERIFY(!plc->acceptsSymbol(Element::NetworkSummary));
		QVERIFY(plc->acceptsSymbol(Element::Simple));
		QVERIFY(!project.folios().first()->acceptsSymbol(Element::PlcSummary));
		QVERIFY(network->acceptsSymbol(Element::NetworkSummary));

		project.setFilePath(dir.filePath(QStringLiteral("summary.qet")));
		QVERIFY(project.write().isOk());
		QETProject reopened(project.filePath());
		QCOMPARE(reopened.state(), QETProject::Ok);
		reopened.loadFolios();
		QVERIFY(reopened.folios().at(1)->isPlcSummaryFolio());
		QVERIFY(reopened.folios().at(2)->isNetworkSummaryFolio());
		QCOMPARE(reopened.folios().first()->folioKind(), Diagram::FolioKind::Schematic);
	}

	void aFunctionTextGivesItsFunction()
	{
		QETProject project;
		Diagram *folio = project.addNewDiagram();
		auto *text = new IndependentTextItem(QStringLiteral("BEACON"));
		folio->addItem(text);
		text->setPos(100, 300);
		const QRectF under(text->sceneBoundingRect().center().x() - 5, 400, 10, 10);
		const QRectF aside(text->sceneBoundingRect().right() + 50, 400, 10, 10);
			//Not a function text yet
		QVERIFY(SummarySymbols::functionTextAt(folio, under).isEmpty());
		text->setFunctionText(true);
		QCOMPARE(SummarySymbols::functionTextAt(folio, under), QStringLiteral("BEACON"));
		QVERIFY(SummarySymbols::functionTextAt(folio, aside).isEmpty());
			//Over it too
		QCOMPARE(SummarySymbols::functionTextAt(folio, under.translated(0, -250)),
				 QStringLiteral("BEACON"));
	}

	void theExampleShowsTheTerminals()
	{
		QETProject project;
		Diagram *schematic = project.addNewDiagram();
		Element *io = device(schematic, QPointF(200, 400));
		QVERIFY(io);
			//A function text over the terminals; its own function wins
		auto *text = new IndependentTextItem(QStringLiteral("PRESSURE SENSOR"));
		schematic->addItem(text);
			//Over its four terminals
		text->setPos(io->sceneBoundingRect().left() - 30, 250);
		QVERIFY(text->sceneBoundingRect().right() > io->sceneBoundingRect().left() + 40);
		text->setFunctionText(true);

		Diagram *summary = project.addNewSummaryDiagram(Diagram::FolioKind::PlcSummary);
		QVERIFY(summary);
			//The symbol a new PLC summary symbol starts with, put on it
		auto *collection = project.embeddedElementCollection();
		QVERIFY(collection->addElementDefinition(QStringLiteral("import"), QStringLiteral("plc_summary"),
												 SummarySymbols::plcExampleDefinition().documentElement()));
		int state = 0;
		Element *symbol = ElementFactory::Instance()->createElement(
			ElementsLocation(QStringLiteral("embed://import/plc_summary.elmt"), &project), nullptr, &state);
		QVERIFY(symbol && !state);
		QCOMPARE(symbol->linkType(), int(Element::PlcSummary));
		QCOMPARE(symbol->terminals().size(), 4);
		QVERIFY(summary->acceptsSymbol(symbol->linkType()));
		summary->addItem(symbol);
		DiagramContext information = symbol->elementInformations();
		information.addValue(QETInformation::ELMT_LABEL, QStringLiteral("-KF1"));
		symbol->setElementInformations(information);
		project.dataBase()->updateDB();
		SummarySymbols::refresh(&project);

			//Each of its terminals shows the terminal of the same name
		QHash<QString, QString> shown = symbol->computedInfo();
		for (int i = 1 ; i <= 4 ; ++i) {
			const QString reference = shown.value(QETInformation::summaryInfoKey(
													  QETInformation::ELMT_SUMMARY_REFERENCE, i));
			QVERIFY2(!reference.isEmpty() && reference != QStringLiteral("?"), qPrintable(reference));
			QCOMPARE(shown.value(QETInformation::summaryInfoKey(QETInformation::ELMT_SUMMARY_PIN, i)),
					 QStringLiteral("X1:%1").arg(i));
			QCOMPARE(shown.value(QETInformation::summaryInfoKey(QETInformation::ELMT_SUMMARY_FUNCTION, i)),
					 QStringLiteral("PRESSURE SENSOR"));
		}
			//And its texts show it
		QCOMPARE(symbol->infoForDisplay(QETInformation::summaryInfoKey(QETInformation::ELMT_SUMMARY_PIN, 2)),
				 QStringLiteral("X1:2"));
			//And by the name of the terminal
		QCOMPARE(shown.value(QETInformation::summaryInfoKeyFor(QETInformation::ELMT_SUMMARY_FUNCTION,
																 QStringLiteral("X1:3"))),
				 QStringLiteral("PRESSURE SENSOR"));

			//Its own function wins
		information = io->elementInformations();
		information.addValue(QETInformation::ELMT_FUNCTION, QStringLiteral("CHILLER"));
		io->setElementInformations(information);
		project.dataBase()->updateDB();
		SummarySymbols::refresh(&project);
		QCOMPARE(symbol->computedInfo().value(QETInformation::summaryInfoKey(
												 QETInformation::ELMT_SUMMARY_FUNCTION, 1)),
				 QStringLiteral("CHILLER"));

			//A device the project does not have
		information = symbol->elementInformations();
		information.addValue(QETInformation::ELMT_LABEL, QStringLiteral("-KF9"));
		symbol->setElementInformations(information);
		project.dataBase()->updateDB();
		SummarySymbols::refresh(&project);
		QCOMPARE(symbol->computedInfo().value(QETInformation::summaryInfoKey(
												 QETInformation::ELMT_SUMMARY_REFERENCE, 3)),
				 QStringLiteral("?"));
	}

		/// The generator of PLC summary symbols: names from a pattern, a
		/// terminal and three texts for each, the texts following their
		/// terminal by its name
	void theGeneratorDrawsEveryTerminal()
	{
		QCOMPARE(SummarySymbols::expandPattern(QStringLiteral(".0-.7")).size(), 8);
		const QStringList bytes = SummarySymbols::expandPattern(QStringLiteral("I0.0-I3.7"));
		QCOMPARE(bytes.size(), 32);
		QCOMPARE(bytes.at(8), QStringLiteral("I1.0"));
		QCOMPARE(bytes.last(), QStringLiteral("I3.7"));
		QCOMPARE(SummarySymbols::expandPattern(QStringLiteral("X1:1-X1:3, PE")),
				 (QStringList{"X1:1", "X1:2", "X1:3", "PE"}));

		const QDomDocument definition = SummarySymbols::plcSummaryDefinition(bytes, true);
		QCOMPARE(definition.documentElement().attribute(QStringLiteral("link_type")),
				 QStringLiteral("plc_summary"));
		QCOMPARE(definition.elementsByTagName(QStringLiteral("terminal")).size(), 32);
		QCOMPARE(definition.elementsByTagName(QStringLiteral("dynamic_text")).size(), 1 + 3 * 32);
		QVERIFY(definition.toString().contains(QStringLiteral("summary_function@I2.5")));
			//Half on each side
		int right = 0;
		const QDomNodeList terminals = definition.elementsByTagName(QStringLiteral("terminal"));
		for (int i = 0 ; i < terminals.size() ; ++i) {
			if (terminals.at(i).toElement().attribute(QStringLiteral("orientation")) == QLatin1String("e")) ++right;
		}
		QCOMPARE(right, 16);
	}

		/// A structure box gives its plant and location to the symbols
		/// in it, over theirs, the smallest box counting; undone with it
	void aStructureBoxGivesItsPlace()
	{
		QETProject project;
		Diagram *folio = project.addNewDiagram();
		Element *inside = device(folio, QPointF(200, 200));
		QVERIFY(inside);
		DiagramContext information = inside->elementInformations();
		information.addValue(QETInformation::ELMT_LOCATION, QStringLiteral("S5"));
		inside->setElementInformations(information);

		auto *big = new QetShapeItem(QPointF(0, 0), QPointF(600, 500), QetShapeItem::Rectangle);
		big->setStructureBox(true);
		big->setStructurePlant(QStringLiteral("A1"));
		big->setStructureLocation(QStringLiteral("S1"));
		folio->addItem(big);
		big->setPos(50, 50);
		auto *small = new QetShapeItem(QPointF(0, 0), QPointF(200, 150), QetShapeItem::Rectangle);
		small->setStructureBox(true);
		small->setStructureLocation(QStringLiteral("S2"));
		folio->addItem(small);
		small->setPos(inside->sceneBoundingRect().center() - QPointF(100, 75));
		QCOMPARE(StructureBoxes::boxAt(folio, inside->sceneBoundingRect().center()), small);
		QCOMPARE(small->structureText(), QStringLiteral("+S2"));
		QCOMPARE(big->structureText(), QStringLiteral("=A1+S1"));

		auto *undo = new QUndoCommand(QStringLiteral("apply"));
		StructureBoxes::apply(folio, undo);
		QCOMPARE(undo->childCount(), 1);
		project.undoStack()->push(undo);
			//The smallest box: its location, the plant left as it was
		QCOMPARE(inside->elementInformations().value(QETInformation::ELMT_LOCATION).toString(),
				 QStringLiteral("S2"));
		project.undoStack()->undo();
		QCOMPARE(inside->elementInformations().value(QETInformation::ELMT_LOCATION).toString(),
				 QStringLiteral("S5"));

			//Out of the small box: the big one's
		small->setPos(1000, 1000);
		undo = new QUndoCommand(QStringLiteral("apply"));
		StructureBoxes::apply(folio, undo);
		project.undoStack()->push(undo);
		QCOMPARE(inside->elementInformations().value(QETInformation::ELMT_PLANT).toString(),
				 QStringLiteral("A1"));
		QCOMPARE(inside->elementInformations().value(QETInformation::ELMT_LOCATION).toString(),
				 QStringLiteral("S1"));
	}
};

QET_APP_TEST_MAIN(tst_summarysymbols)
#include "tst_summarysymbols.moc"
