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
#include "ElementsCollection/xmlelementcollection.h"
#include "ElementsCollection/partscollectionitem.h"
#include "diagram.h"
#include "diagramevent/diagrameventaddelement.h"
#include "partsdb/partsdatabase.h"
#include "partsdb/partslinks.h"
#include "partsdb/partsplacement.h"
#include "qetgraphicsitem/element.h"
#include "qetproject.h"

#include <QDomDocument>
#include <QKeyEvent>
#include <QMimeData>
#include <QScopeGuard>
#include <QUuid>
#include <QUndoCommand>

/**
	The parts collection of the collection panel:

	 - it lists the parts of the parts database by category, in the
	   order of the parts manager (categories by name, a part without one
	   last; parts by manufacturer and order number);
	 - a part is dragged as its symbol (the usual element uri) with the
	   part itself; a part without a symbol only with the part; a category
	   is not dragged;
	 - it reads the database again when told;
	 - a symbol placed with a part gets the part in its Parts tab and the
	   article in its information; a part dropped onto a symbol already
	   placed only goes into its Parts tab, one unit more when it is there.
*/
class tst_partscollection : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;
	const QString m_symbol = QStringLiteral(
			"common://10_electric/05_iec_60617_cuadros/01_contactos/"
			"contacto_conmutado.elmt");

	static Part part(const QString &manufacturer, const QString &order,
					 const QString &category, const QString &symbol = QString())
	{
		Part result;
		result.manufacturer = manufacturer;
		result.order_number = order;
		result.category = category;
		if (!symbol.isEmpty()) {
			result.symbols << symbol;
		}
		result.description = manufacturer + QStringLiteral(" part");
		return result;
	}

	static QStringList texts(QStandardItem *parent)
	{
		QStringList list;
		for (int row = 0 ; row < parent->rowCount() ; ++row) {
			list << parent->child(row)->text();
		}
		return list;
	}

	static PartsCollectionItem *root(ElementsCollectionModel &model)
	{
		for (int row = 0 ; row < model.rowCount() ; ++row) {
			if (model.item(row)->type() == PartsCollectionItem::Type) {
				return static_cast<PartsCollectionItem *>(model.item(row));
			}
		}
		return nullptr;
	}

	private slots:
		void initTestCase()
		{
			QVERIFY(m_dir.isValid());
			QETProject::setBackupEnabled(false);
			PartsDatabase::setConfiguredPath(
						m_dir.filePath(QStringLiteral("parts.sqlite")));
			PartsDatabase::setEnabled(true);

			PartsDatabase db;
			QString error;
			QVERIFY2(db.open(PartsDatabase::path(), &error), qPrintable(error));
			for (const Part &p : {part("ACME", "PLC-1214", "PLC", m_symbol),
								  part("Bravo", "RL-0900", "relés"),
								  part("Charlie", "CB-1016", "Fuentes"),
								  part("Charlie", "RL-0024", "relés"),
								  part("Delta", "TB-4102", QString())}) {
				QVERIFY2(db.addPart(p, &error), qPrintable(error));
			}
		}

		void treeFollowsThePartsManager()
		{
			ElementsCollectionModel model;
			model.addPartsCollection();
			PartsCollectionItem *parts = root(model);
			QVERIFY(parts);
			QVERIFY(parts->isCollectionRoot());
			QCOMPARE(parts->text(), QStringLiteral("Parts collection"));
			QCOMPARE(texts(parts), (QStringList{"Fuentes", "PLC", "relés",
												"(No category)"}));
			QCOMPARE(texts(parts->child(2)),
					 (QStringList{"Bravo RL-0900", "Charlie RL-0024"}));
			QCOMPARE(texts(parts->child(3)), (QStringList{"Delta TB-4102"}));
		}

		void partIsDraggedWithItsSymbol()
		{
			ElementsCollectionModel model;
			model.addPartsCollection();
			PartsCollectionItem *parts = root(model);
			QVERIFY(parts);

			auto *cpu = static_cast<PartsCollectionItem *>(parts->child(1)->child(0));
			QVERIFY(cpu->isElement());
			QScopedPointer<QMimeData> with_symbol(model.mimeData({cpu->index()}));
			QCOMPARE(with_symbol->text(), m_symbol);
			QCOMPARE(with_symbol->data(QStringLiteral("application/x-qet-element-uri")),
					 m_symbol.toLatin1());
			const auto key = PartsPlacement::decode(with_symbol.data());
			QVERIFY(key);
			QCOMPARE(key->manufacturer, QStringLiteral("ACME"));
			QCOMPARE(key->order_number, QStringLiteral("PLC-1214"));
			QCOMPARE(key->symbols, QStringList{m_symbol});

			auto *breaker = static_cast<PartsCollectionItem *>(parts->child(0)->child(0));
			QVERIFY(!breaker->isElement());
			QScopedPointer<QMimeData> alone(model.mimeData({breaker->index()}));
			QVERIFY(!alone->hasText());
			QVERIFY(!alone->hasFormat(QStringLiteral("application/x-qet-element-uri")));
			QCOMPARE(PartsPlacement::decode(alone.data())->order_number,
					 QStringLiteral("CB-1016"));

			QScopedPointer<QMimeData> category(model.mimeData({parts->child(0)->index()}));
			QVERIFY(category->formats().isEmpty());
		}

		/// While a part is placed, Tab puts its next symbol in place of
		/// the one following the mouse, Shift+Tab the one before, round at
		/// both ends; the folio's own Tab, going through its items, waits
		void tabGoesThroughTheSymbols()
		{
				//Placing finds the symbol through the projects the
				//application has open
			QETProject project;
			QETApp::registerProject(&project);
			const auto unregister = qScopeGuard([&project]() {
				QETApp::unregisterProject(&project);
			});
			Diagram *diagram = project.addNewDiagram();
			const QString folder = QStringLiteral("common://10_electric/05_iec_60617_cuadros/");
			const QList<ElementsLocation> symbols{
				ElementsLocation(folder + QStringLiteral("02_bobinas/bobina.elmt")),
				ElementsLocation(folder + QStringLiteral("01_contactos/contacto_na.elmt")),
				ElementsLocation(folder + QStringLiteral("10_senalizacion_medida/piloto_senalizacion.elmt"))};
			for (const ElementsLocation &symbol : symbols) {
				QVERIFY2(symbol.exist(), qPrintable(symbol.toString()));
			}
			const auto shown = [](DiagramEventAddElement *placing) {
				return placing->pendingElement()->location().fileName();
			};

			ElementsLocation first = symbols.first();
			auto *event = new DiagramEventAddElement(first, diagram, QPointF(200, 100));
			QVERIFY(event->isRunning());
			diagram->setEventInterface(event);
			event->setAlternatives(symbols);
			QCOMPARE(shown(event), QStringLiteral("bobina.elmt"));
			event->pendingElement()->setRotation(90);

			const auto press = [diagram](int key, Qt::KeyboardModifiers modifiers) {
				QKeyEvent tab(QEvent::KeyPress, key, modifiers);
				QCoreApplication::sendEvent(diagram, &tab);
			};
			press(Qt::Key_Tab, Qt::NoModifier);
			QCOMPARE(shown(event), QStringLiteral("contacto_na.elmt"));
			QCOMPARE(event->pendingElement()->pos(), QPointF(200, 100));
			QCOMPARE(event->pendingElement()->rotation(), 90.0);
			press(Qt::Key_Tab, Qt::NoModifier);
			press(Qt::Key_Tab, Qt::NoModifier);
			QCOMPARE(shown(event), QStringLiteral("bobina.elmt"));
			press(Qt::Key_Backtab, Qt::ShiftModifier);
			QCOMPARE(shown(event), QStringLiteral("piloto_senalizacion.elmt"));
				//The one following the mouse is the only one on the folio
			QCOMPARE(diagram->elements().size(), 1);
			diagram->clearEventInterface();
		}

		void readAgainWhenTold()
		{
			ElementsCollectionModel model;
			model.addPartsCollection();
			PartsCollectionItem *parts = root(model);
			QCOMPARE(parts->rowCount(), 4);

			PartsDatabase db;
			QVERIFY(db.open(PartsDatabase::path()));
			QVERIFY(db.addPart(part("Echo", "TB-2000", "Bornas")));
			model.reloadPartsCollection();
			QCOMPARE(texts(parts).first(), QStringLiteral("Bornas"));
			QCOMPARE(parts->rowCount(), 5);
		}

		void placedSymbolTakesThePart()
		{
			const QString fixture = QFINDTESTDATA("fixtures/every_feature.qet");
			QVERIFY(!fixture.isEmpty());
			QETProject project(fixture);
			QCOMPARE(project.state(), QETProject::Ok);
			Element *element = nullptr;
			for (Diagram *diagram : project.diagrams()) {
				for (Element *candidate : diagram->elements()) {
					if (!element && candidate->linkType() == Element::Simple
						&& candidate->elementInformations()
							.value(PartsLinks::infoKey()).toString().isEmpty()) {
						element = candidate;
					}
				}
			}
			QVERIFY(element);
			const DiagramContext before = element->elementInformations();
			const PartsPlacement::Key key{QStringLiteral("ACME"),
										  QStringLiteral("PLC-1214")};

				//Placed with the part: the part and its article
			QUndoCommand placed;
			PartsPlacement::preparer(key)(element, &placed);
			placed.redo();
			QList<PartsLinks::Link> links = PartsLinks::decode(
						element->elementInformations().value(PartsLinks::infoKey()).toString());
			QCOMPARE(links.size(), 1);
			QCOMPARE(links.first().order_number, key.order_number);
			QCOMPARE(element->elementInformations().value(QStringLiteral("manufacturer"))
					 .toString(), QStringLiteral("ACME"));

				//Dropped onto it again: one unit more, nothing else
			const DiagramContext with_one = element->elementInformations();
			QUndoCommand dropped;
			PartsPlacement::addPart(element, key, false, &dropped);
			dropped.redo();
			links = PartsLinks::decode(
						element->elementInformations().value(PartsLinks::infoKey()).toString());
			QCOMPARE(links.size(), 1);
			QCOMPARE(links.first().units, 2.0);
			dropped.undo();
			QVERIFY(element->elementInformations() == with_one);
			placed.undo();
			QVERIFY(element->elementInformations() == before);
		}
};

	/// The symbols of the repository's collection, set before the
	/// application starts: it reads the folder once
static void useTheCollection()
{
	QETApp::overrideCommonElementsDir(QStringLiteral(QET_ELEMENTS_DIR));
}

QET_APP_TEST_MAIN_PREPARED(tst_partscollection, useTheCollection)
#include "tst_partscollection.moc"
