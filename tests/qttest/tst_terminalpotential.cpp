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
#include "ElementsCollection/elementslocation.h"
#include "ElementsCollection/xmlelementcollection.h"
#include "factory/elementfactory.h"
#include "qetapptest.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminal.h"
#include "qetproject.h"

#include <QDomDocument>
#include <QUuid>

/**
	The potential of the terminals of a terminal block (element editor,
	"Potential"), when the block's function separation is on:

	 - terminals of one potential share their wires, and two potentials
	   share nothing, not even the voltage, which crosses a separation
	   otherwise: they are two electric nodes, as the two levels of a
	   terminal are;
	 - a terminal without a potential, in a block whose other terminals
	   have one, is a node of its own;
	 - a block whose terminals have no potential keeps its two sides
	   apart and lets the voltage cross, as before;
	 - with the separation off the potentials change nothing: the whole
	   block is one potential, as before.
*/
class tst_terminalpotential : public QObject
{
	Q_OBJECT

	int m_blocks = 0;
	int m_position = 0;

	/// A symbol of the definition @p xml, kept in the project as @p name
	Element *place(Diagram *diagram, const QString &name, const QString &xml)
	{
		XmlElementCollection *collection =
				diagram->project()->embeddedElementCollection();
		const QString path = QStringLiteral("import/%1.elmt").arg(name);
		if (!collection->exist(path)) {
			QDomDocument definition;
			if (!definition.setContent(xml)
					|| !collection->addElementDefinition(
						QStringLiteral("import"), name,
						definition.documentElement())) {
				return nullptr;
			}
		}
		int state = 0;
		Element *element = ElementFactory::Instance()->createElement(
					ElementsLocation(QStringLiteral("embed://") + path,
									 diagram->project()),
					nullptr, &state);
		if (state || !element) {
			delete element;
			return nullptr;
		}
		diagram->addItem(element);
		element->setPos(100 + 60 * (m_position % 10),
						100 + 120 * (m_position / 10));
		++m_position;
		return element;
	}

	/// A terminal block whose terminals t0, t1... have the potentials
	/// @p potentials, the even ones on top and the odd ones below
	Element *block(Diagram *diagram, const QStringList &potentials,
				   bool separated)
	{
		QString terminals;
		for (int i = 0; i < potentials.size(); ++i) {
			QString terminal = QStringLiteral(
					"<terminal x=\"%1\" y=\"%2\" orientation=\"%3\" "
					"name=\"t%4\" uuid=\"%5\"");
			terminal = terminal.arg(10 * (i / 2))
					.arg(i % 2 ? 20 : -20)
					.arg(i % 2 ? QStringLiteral("s") : QStringLiteral("n"))
					.arg(i)
					.arg(QUuid::createUuid().toString());
			if (!potentials.at(i).isEmpty()) {
				terminal += QStringLiteral(" potential=\"%1\"")
						.arg(potentials.at(i));
			}
			terminals += terminal + QStringLiteral("/>");
		}
		const QString xml = QStringLiteral(
				"<definition type=\"element\" link_type=\"terminal\" "
				"uuid=\"%1\" width=\"%2\" height=\"40\" hotspot_x=\"10\" "
				"hotspot_y=\"20\">"
				"<names><name lang=\"en\">Test terminal block</name></names>"
				"<kindInformations><kindInformation name=\"type\">generic"
				"</kindInformation></kindInformations>"
				"<description>"
				"<line x1=\"0\" y1=\"-10\" x2=\"0\" y2=\"10\"/>%3"
				"</description></definition>")
				.arg(QUuid::createUuid().toString())
				.arg(20 + 10 * ((potentials.size() + 1) / 2))
				.arg(terminals);
		Element *element = place(diagram,
				QStringLiteral("block%1").arg(m_blocks++), xml);
		if (!element) {
			return nullptr;
		}
		DiagramContext info = element->elementInformations();
		info.addValue(QStringLiteral("potential_isolating"),
					  separated ? QStringLiteral("true")
								: QStringLiteral("false"));
		element->setElementInformations(info);
		return element;
	}

	/// A symbol of one terminal, for a wire to end on
	Element *device(Diagram *diagram)
	{
		const QString xml = QStringLiteral(
				"<definition type=\"element\" link_type=\"simple\" "
				"uuid=\"%1\" width=\"20\" height=\"40\" hotspot_x=\"10\" "
				"hotspot_y=\"20\">"
				"<names><name lang=\"en\">Test device</name></names>"
				"<kindInformations><kindInformation name=\"type\">coil"
				"</kindInformation></kindInformations>"
				"<description>"
				"<line x1=\"0\" y1=\"-10\" x2=\"0\" y2=\"10\"/>"
				"<terminal x=\"0\" y=\"-20\" orientation=\"n\" uuid=\"%2\"/>"
				"</description></definition>")
				.arg(QUuid::createUuid().toString(),
					 QUuid::createUuid().toString());
		return place(diagram, QStringLiteral("device"), xml);
	}

	static Terminal *pin(Element *element, const QString &name)
	{
		for (Terminal *terminal : element->terminals()) {
			if (terminal->name() == name) {
				return terminal;
			}
		}
		return nullptr;
	}

	static Conductor *join(Terminal *a, Terminal *b)
	{
		if (!a || !b) {
			return nullptr;
		}
		auto *conductor = new Conductor(a, b);
		a->parentElement()->diagram()->addItem(conductor);
		return conductor;
	}

	/// A wire from the terminal t<@p index> of @p block to a device
	Conductor *wire(Element *block, int index)
	{
		Element *other = device(block->diagram());
		if (!other || other->terminals().isEmpty()) {
			return nullptr;
		}
		return join(pin(block, QStringLiteral("t%1").arg(index)),
					other->terminals().first());
	}

	/// The wires of the terminals of @p block, in the order of the terminals
	QList<Conductor *> wires(Element *block, int count)
	{
		QList<Conductor *> list;
		for (int i = 0; i < count; ++i) {
			list << wire(block, i);
		}
		return list;
	}

	private slots:
		void initTestCase()
		{
			QETProject::setBackupEnabled(false);
		}

		/// Without potentials, the separation keeps the two sides of the
		/// block apart and only the voltage crosses it, as before
		void sidesWithoutPotentialShareTheVoltage()
		{
			QETProject project;
			Diagram *diagram = project.addNewDiagram();
			Element *x1 = block(diagram, {QString(), QString()}, true);
			QVERIFY(x1);
			const QList<Conductor *> w = wires(x1, 2);
			QVERIFY(!w.contains(nullptr));

			QVERIFY(w[0]->relatedPotentialConductors().isEmpty());
			QCOMPARE(w[0]->functionSeparatedConductors(),
					 QSet<Conductor *>({w[1]}));
			QCOMPARE(w[1]->functionSeparatedConductors(),
					 QSet<Conductor *>({w[0]}));
		}

		/// With the separation off the potentials change nothing: the
		/// whole block is one potential
		void potentialsNeedTheSeparation()
		{
			QETProject project;
			Diagram *diagram = project.addNewDiagram();
			Element *x1 = block(diagram, {"1", "1", "2", "2"}, false);
			QVERIFY(x1);
			const QList<Conductor *> w = wires(x1, 4);
			QVERIFY(!w.contains(nullptr));

			QCOMPARE(w[0]->relatedPotentialConductors(),
					 QSet<Conductor *>({w[1], w[2], w[3]}));
			QVERIFY(w[0]->functionSeparatedConductors().isEmpty());
		}

		/// A terminal of two levels: each level shares its wires, and
		/// nothing goes from one level to the other, not even the voltage
		void twoLevelsAreTwoNodes()
		{
			QETProject project;
			Diagram *diagram = project.addNewDiagram();
			Element *x1 = block(diagram, {"1", "1", "2", "2"}, true);
			QVERIFY(x1);
			const QList<Conductor *> w = wires(x1, 4);
			QVERIFY(!w.contains(nullptr));

			QCOMPARE(w[0]->relatedPotentialConductors(),
					 QSet<Conductor *>({w[1]}));
			QCOMPARE(w[2]->relatedPotentialConductors(),
					 QSet<Conductor *>({w[3]}));
			QCOMPARE(w[0]->functionSeparatedConductors(),
					 QSet<Conductor *>());
			QCOMPARE(w[3]->functionSeparatedConductors(),
					 QSet<Conductor *>());
		}

		/// In a block whose terminals have potentials, a terminal without
		/// one is a node of its own
		void aTerminalWithoutPotentialStandsAlone()
		{
			QETProject project;
			Diagram *diagram = project.addNewDiagram();
			Element *x1 = block(diagram, {"1", "1", QString()}, true);
			QVERIFY(x1);
			const QList<Conductor *> w = wires(x1, 3);
			QVERIFY(!w.contains(nullptr));

			QCOMPARE(w[0]->relatedPotentialConductors(),
					 QSet<Conductor *>({w[1]}));
			QVERIFY(w[2]->relatedPotentialConductors().isEmpty());
			QCOMPARE(w[2]->functionSeparatedConductors(),
					 QSet<Conductor *>());
			QCOMPARE(w[0]->functionSeparatedConductors(),
					 QSet<Conductor *>());
		}

		/// What crosses is decided block by block: the voltage crosses a
		/// block without potentials and goes on along one level of a
		/// terminal of two levels, never to its other level
		void theVoltageCrossesBlockByBlock()
		{
			QETProject project;
			Diagram *diagram = project.addNewDiagram();
			Element *x1 = block(diagram, {QString(), QString()}, true);
			Element *x2 = block(diagram, {"1", "1", "2", "2"}, true);
			QVERIFY(x1 && x2);
			Conductor *in = wire(x1, 0);
			Conductor *between = join(pin(x1, QStringLiteral("t1")),
									  pin(x2, QStringLiteral("t0")));
			Conductor *level_1 = wire(x2, 1);
			Conductor *level_2_top = wire(x2, 2);
			Conductor *level_2_bottom = wire(x2, 3);
			QVERIFY(in && between && level_1 && level_2_top
					&& level_2_bottom);

			QVERIFY(in->relatedPotentialConductors().isEmpty());
			QCOMPARE(in->functionSeparatedConductors(),
					 QSet<Conductor *>({between, level_1}));
			QCOMPARE(level_2_top->relatedPotentialConductors(),
					 QSet<Conductor *>({level_2_bottom}));
			QVERIFY(level_2_top->functionSeparatedConductors().isEmpty());
		}
};

QET_APP_TEST_MAIN(tst_terminalpotential)
#include "tst_terminalpotential.moc"
