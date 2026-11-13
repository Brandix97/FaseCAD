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
#include "TerminalStrip/GraphicsItem/terminalstripdrawer.h"
#include "TerminalStrip/GraphicsItem/trueterminalstrip.h"
#include "TerminalStrip/realterminal.h"
#include "TerminalStrip/terminalstrip.h"
#include "dataBase/projectdatabase.h"
#include "diagram.h"
#include "factory/elementfactory.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/dynamicelementtextitem.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminal.h"
#include "qetproject.h"
#include "qetresult.h"
#include "ui/elementpropertieswidget.h"
#include "ui/elementterminaltextswidget.h"
#include "undocommand/changeterminaltextscommand.h"

#include <QFile>
#include <QTableWidget>
#include <QUndoStack>

/**
	The texts a placed symbol gives its terminals instead of their names
	(the "Terminal texts" tab of its properties):

	 - a terminal is named by its text, and by the symbol's name again
	   when the text is emptied;
	 - the terminal strip plan and the project database show the text;
	 - the change is one step of undo;
	 - the texts are saved in the .qet and the .edrwz, and a symbol that
	   gives none saves as before;
	 - the tab shows a row per terminal and gives the texts typed there.

	fixtures/every_feature.qet has the strip X1 (elements 891b70f2 and
	4861e268) and the contact fc9cc920 wired to 891b70f2 by the
	conductor 8bd16d7a.
*/
class tst_terminaltexts : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;
	QETProject *m_project = nullptr;
	const QString m_text = QStringLiteral("13 NO");

	static Element *element(QETProject *project, const QString &uuid)
	{
		for (Diagram *diagram : project->diagrams()) {
			for (Element *candidate : diagram->elements()) {
				if (candidate->uuid().toString() == uuid) {
					return candidate;
				}
			}
		}
		return nullptr;
	}

	static Element *contact(QETProject *project)
	{
		return element(project, QStringLiteral(
						   "{fc9cc920-5822-421a-916e-a17100acdcf6}"));
	}

	/// The terminal of the contact the wire to the strip ends on
	static Terminal *pin(QETProject *project)
	{
		Element *device = contact(project);
		if (!device) {
			return nullptr;
		}
		for (Terminal *terminal : device->terminals()) {
			for (Conductor *wire : terminal->conductors()) {
				if (wire->uuid().toString() == QStringLiteral(
						"{8bd16d7a-18c4-4fa0-9495-8dab65fd6408}")) {
					return terminal;
				}
			}
		}
		return nullptr;
	}

	QMap<QString, QString> textOfThePin() const
	{
		return {{pin(m_project)->stableUuid().toString(), m_text}};
	}

	/// What the plans of the strips say of the devices' pins
	QStringList pinsInThePlans() const
	{
		QStringList pins;
		for (TerminalStrip *strip : m_project->terminalStrip()) {
			TerminalStripDrawer::TrueTerminalStrip drawn(strip);
			for (const auto &physical : drawn.physicalTerminal()) {
				for (const auto &real : physical->realTerminals()) {
					const auto column = real->diagramColumn();
					pins << column.device_pin << column.inner_connection;
				}
			}
		}
		return pins;
	}

	private slots:
		void initTestCase()
		{
			QVERIFY(m_dir.isValid());
			QETProject::setBackupEnabled(false);
		}

		void init()
		{
			const QString fixture = QFINDTESTDATA("fixtures/every_feature.qet");
			QVERIFY(!fixture.isEmpty());
			m_project = new QETProject(fixture);
			QCOMPARE(m_project->state(), QETProject::Ok);
			QVERIFY(contact(m_project));
			QVERIFY(pin(m_project));
		}

		void cleanup()
		{
			delete m_project;
			m_project = nullptr;
		}

		void theTextComesBeforeTheName()
		{
			Element *device = contact(m_project);
			Terminal *terminal = pin(m_project);
			const QString symbol_name = terminal->symbolName();
			QCOMPARE(terminal->name(), symbol_name);
			QVERIFY(symbol_name != m_text);

			QSignalSpy changed(device, &Element::terminalTextsChanged);
			device->setTerminalTexts(textOfThePin());
			QCOMPARE(changed.count(), 1);
			QCOMPARE(terminal->name(), m_text);
			QCOMPARE(terminal->symbolName(), symbol_name);
			for (Terminal *other : device->terminals()) {
				if (other->stableUuid() != terminal->stableUuid()) {
					QCOMPARE(other->name(), other->symbolName());
				}
			}

				//The same texts again change nothing
			device->setTerminalTexts(textOfThePin());
			QCOMPARE(changed.count(), 1);

				//An empty text gives the name back
			device->setTerminalTexts(
						{{terminal->stableUuid().toString(), QString()}});
			QVERIFY(device->terminalTexts().isEmpty());
			QCOMPARE(terminal->name(), symbol_name);
		}

		void thePlanAndTheDatabaseShowTheText()
		{
			Element *device = contact(m_project);
			Terminal *terminal = pin(m_project);
			const QString symbol_name = terminal->symbolName();
			const QStringList before = pinsInThePlans();
			QVERIFY(before.contains(symbol_name));
			QVERIFY(!before.contains(m_text));
			QCOMPARE(m_project->dataBase()->terminalName(
						 terminal->stableUuid(), device->uuid()), symbol_name);

			QUndoStack stack;
			stack.push(new ChangeTerminalTextsCommand(device, textOfThePin()));
			QCOMPARE(terminal->name(), m_text);
				//Other terminals of the fixture have that name too: the
				//pin of the contact alone takes the text
			const QStringList after = pinsInThePlans();
			QVERIFY(after.count(m_text) >= 1);
			QCOMPARE(after.count(symbol_name),
					 before.count(symbol_name) - after.count(m_text));
			QCOMPARE(m_project->dataBase()->terminalName(
						 terminal->stableUuid(), device->uuid()), m_text);

			stack.undo();
			QCOMPARE(terminal->name(), symbol_name);
			QCOMPARE(pinsInThePlans(), before);
			QCOMPARE(m_project->dataBase()->terminalName(
						 terminal->stableUuid(), device->uuid()), symbol_name);

			stack.redo();
			QCOMPARE(terminal->name(), m_text);
		}

		/// The number a symbol draws beside a terminal, a typed text of
		/// its own, takes the terminal's text; one farther away saying
		/// the same does not
		void theDrawnNumberFollows()
		{
			Element *device = contact(m_project);
			Terminal *terminal = pin(m_project);
			const QString symbol_name = terminal->symbolName();
			const QString uuid = terminal->stableUuid().toString();
			const QString first = QStringLiteral("21");
			QVERIFY(symbol_name != first);
			QUndoStack stack;
				//The terminal of the fixture may have no name: a first
				//text gives it one, drawn by no text yet
			stack.push(new ChangeTerminalTextsCommand(device, {{uuid, first}}));
			QCOMPARE(terminal->name(), first);

			const QPointF dock = device->mapFromScene(terminal->dockConductor());
			auto *near = new DynamicElementTextItem(device);
			device->addDynamicTextItem(near);
			near->setText(first);
			near->setPos(dock + QPointF(2, -8));
			auto *far = new DynamicElementTextItem(device);
			device->addDynamicTextItem(far);
			far->setText(first);
			far->setPos(dock + QPointF(0, 300));

			stack.push(new ChangeTerminalTextsCommand(device, textOfThePin()));
			QCOMPARE(near->text(), m_text);
			QCOMPARE(far->text(), first);

			stack.undo();
			QCOMPARE(near->text(), first);
			stack.redo();
			QCOMPARE(near->text(), m_text);

			stack.push(new ChangeTerminalTextsCommand(device, {{uuid, first}}));
			QCOMPARE(near->text(), first);

				//Emptied, the terminal takes the symbol's name again, and
				//the number goes with it unless that name is empty
			stack.push(new ChangeTerminalTextsCommand(
						   device, QMap<QString, QString>()));
			QCOMPARE(terminal->name(), symbol_name);
			QCOMPARE(near->text(), symbol_name.isEmpty() ? first : symbol_name);
		}

		/// The changeover contact of the collection draws 11, 12 and 14
		/// as texts of its own beside its terminals: all three follow
		void theCollectionContactShowsTheTexts()
		{
			QFile file(QStringLiteral(QET_ELEMENTS_DIR) + QStringLiteral(
						   "/10_electric/05_iec_60617_cuadros/01_contactos/"
						   "contacto_conmutado.elmt"));
			QVERIFY(file.open(QIODevice::ReadOnly));
			QDomDocument definition;
			QVERIFY(bool(definition.setContent(&file)));

			QETProject project;
			Diagram *diagram = project.addNewDiagram();
			QVERIFY(project.embeddedElementCollection()->addElementDefinition(
						QStringLiteral("import"), QStringLiteral("changeover"),
						definition.documentElement()));
			int state = 0;
			Element *changeover = ElementFactory::Instance()->createElement(
						ElementsLocation(QStringLiteral(
							"embed://import/changeover.elmt"), &project),
						nullptr, &state);
			QVERIFY(changeover && !state);
			diagram->addItem(changeover);
			changeover->setPos(200, 200);

			const QMap<QString, QString> wanted = {{QStringLiteral("11"), QStringLiteral("99")},
												   {QStringLiteral("12"), QStringLiteral("22")},
												   {QStringLiteral("14"), QStringLiteral("33")}};
			QMap<QString, QString> texts;
			for (Terminal *terminal : changeover->terminals()) {
				QVERIFY(wanted.contains(terminal->symbolName()));
				texts.insert(terminal->stableUuid().toString(),
							 wanted.value(terminal->symbolName()));
			}
			const auto drawn = [changeover]() {
				QStringList list;
				for (DynamicElementTextItem *text : changeover->dynamicTextItems()) {
					if (text->textFrom() == DynamicElementTextItem::UserText) {
						list << text->text();
					}
				}
				list.sort();
				return list;
			};
			QCOMPARE(drawn(), (QStringList{"11", "12", "14"}));

			diagram->undoStack().push(new ChangeTerminalTextsCommand(changeover, texts));
			QCOMPARE(drawn(), (QStringList{"22", "33", "99"}));
			for (Terminal *terminal : changeover->terminals()) {
				QCOMPARE(terminal->name(), wanted.value(terminal->symbolName()));
			}
			diagram->undoStack().undo();
			QCOMPARE(drawn(), (QStringList{"11", "12", "14"}));
		}

		void savedAndReadBack_data()
		{
			QTest::addColumn<QString>("suffix");
			QTest::newRow("qet") << QStringLiteral("qet");
			QTest::newRow("edrwz") << QStringLiteral("edrwz");
		}

		void savedAndReadBack()
		{
			QFETCH(QString, suffix);
			Element *device = contact(m_project);
			const QUuid device_uuid = device->uuid();
			const QUuid terminal_uuid = pin(m_project)->stableUuid();
			device->setTerminalTexts(textOfThePin());

			const QString path = m_dir.filePath(
						QStringLiteral("texts.") + suffix);
			m_project->setFilePath(path);
			QVERIFY(m_project->write().isOk());

			QETProject reopened(path);
			QCOMPARE(reopened.state(), QETProject::Ok);
			QCOMPARE(reopened.dataBase()->terminalName(terminal_uuid,
													   device_uuid), m_text);
			Element *read = contact(&reopened);
			QVERIFY(read);
			QCOMPARE(read->terminalTexts(), textOfThePin());
			QCOMPARE(pin(&reopened)->name(), m_text);
		}

		void noTextSavesAsBefore()
		{
			const QString path = m_dir.filePath(QStringLiteral("plain.qet"));
			m_project->setFilePath(path);
			QVERIFY(m_project->write().isOk());
			QFile file(path);
			QVERIFY(file.open(QIODevice::ReadOnly));
			QVERIFY(!file.readAll().contains("terminalTexts"));
		}

		void theTabGivesTheTexts()
		{
			Element *device = contact(m_project);
			Terminal *terminal = pin(m_project);

			ElementPropertiesWidget dialog(device);
			QVERIFY(dialog.findChild<ElementTerminalTextsWidget *>());

			ElementTerminalTextsWidget tab(device);
			auto *table = tab.findChild<QTableWidget *>(
						QStringLiteral("terminal_texts_table"));
			QVERIFY(table);
			QVERIFY(table->rowCount() >= 1);
			QVERIFY(table->rowCount() <= device->terminals().size());
			QVERIFY(!tab.associatedUndo());

			int row = -1;
			for (int i = 0 ; i < table->rowCount() ; ++i) {
				if (table->item(i, 0)->data(Qt::UserRole).toString()
					== terminal->stableUuid().toString()) {
					row = i;
				}
			}
			QVERIFY(row >= 0);
			QCOMPARE(table->item(row, 1)->text(), terminal->symbolName());

			table->item(row, 1)->setText(QStringLiteral("  ") + m_text);
			QUndoCommand *undo = tab.associatedUndo();
			QVERIFY(undo);
			device->diagram()->undoStack().push(undo);
			QCOMPARE(terminal->name(), m_text);
				//The table follows the symbol
			QCOMPARE(table->item(row, 1)->text(), m_text);

				//The symbol's name typed again is no text of its own
			table->item(row, 1)->setText(terminal->symbolName());
			undo = tab.associatedUndo();
			QVERIFY(undo);
			device->diagram()->undoStack().push(undo);
			QVERIFY(device->terminalTexts().isEmpty());

			device->diagram()->undoStack().undo();
			QCOMPARE(device->terminalTexts(), textOfThePin());
		}
};

QET_APP_TEST_MAIN(tst_terminaltexts)
#include "tst_terminaltexts.moc"
