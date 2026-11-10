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

#include "TerminalStrip/GraphicsItem/properties/terminalstriplayoutpattern.h"
#include "TerminalStrip/GraphicsItem/terminaldiagramdrawer.h"
#include "TerminalStrip/GraphicsItem/terminalstripdrawer.h"
#include "TerminalStrip/GraphicsItem/trueterminalstrip.h"
#include "TerminalStrip/realterminal.h"
#include "TerminalStrip/terminalstrip.h"
#include "cable/cable.h"
#include "diagram.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/element.h"
#include "qetproject.h"

#include <QImage>
#include <QPainter>

/**
	The terminal diagram style of a terminal strip:

	 - each terminal tells the core colour of the cable its outer wire
	   is, the label, pin and function text of the device it lands on;
	 - the style is saved and read back by its name;
	 - the drawing has one column per terminal, with an empty column
	   between two devices, and paints.

	fixtures/every_feature.qet has the strip X1 (elements 891b70f2 and
	4861e268) and the contact fc9cc920 wired to 891b70f2 by the
	conductor 8bd16d7a.
*/
class tst_terminaldiagram : public QObject
{
	Q_OBJECT

	QETProject *m_project = nullptr;

	Element *element(const QString &uuid) const
	{
		for (Diagram *diagram : m_project->diagrams()) {
			for (Element *candidate : diagram->elements()) {
				if (candidate->uuid().toString() == uuid) {
					return candidate;
				}
			}
		}
		return nullptr;
	}

	Conductor *conductor(const QString &uuid) const
	{
		for (Diagram *diagram : m_project->diagrams()) {
			for (Conductor *candidate : diagram->conductors()) {
				if (candidate && candidate->uuid().toString() == uuid) {
					return candidate;
				}
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

	void styleIsSavedByName()
	{
		using Style = TerminalStripLayoutPattern::DrawStyle;
		const QString name = TerminalStripLayoutPattern::drawStyleToString(
					Style::TerminalDiagram);
		QCOMPARE(name, QStringLiteral("terminal_diagram"));
		QCOMPARE(TerminalStripLayoutPattern::drawStyleFromString(
					 name, Style::Schematic), Style::TerminalDiagram);
	}

	void columnTellsCoreAndDevice()
	{
		Element *contact = element(
			QStringLiteral("{fc9cc920-5822-421a-916e-a17100acdcf6}"));
		Conductor *wire = conductor(
			QStringLiteral("{8bd16d7a-18c4-4fa0-9495-8dab65fd6408}"));
		QVERIFY(contact);
		QVERIFY(wire);

		ElementData data = contact->elementData();
		data.m_informations.addValue(QStringLiteral("label"),
									 QStringLiteral("-S7"));
		data.m_informations.addValue(QStringLiteral("function"),
									 QStringLiteral("PUSH UP BLIND 1"));
		contact->setElementData(data);

		Cable *cable = m_project->newCable();
		QVERIFY(cable);
		cable->setCoreCount(3);
		cable->setCoreColors({QStringLiteral("BN"), QStringLiteral("BU"),
							  QStringLiteral("GNYE")});
		CableCore core;
		core.core = 1;
		core.conductor = wire->uuid();
		cable->setCore(core);

		const auto strips = m_project->terminalStrip();
		QVERIFY(!strips.isEmpty());
		TerminalStripDrawer::TrueTerminalStrip strip(strips.first());
		TerminalStripDrawer::DiagramColumn found;
		int count = 0;
		for (const auto &physical : strip.physicalTerminal()) {
			for (const auto &real : physical->realTerminals()) {
				++count;
				const auto column = real->diagramColumn();
				if (column.outer_wire == QStringLiteral("BU")) {
					found = column;
				}
			}
		}
		QCOMPARE(found.outer_wire, QStringLiteral("BU"));
		QCOMPARE(found.device_label, contact->actualLabel());
		QCOMPARE(found.function_text, QStringLiteral("PUSH UP BLIND 1"));
		QVERIFY(!found.device_key.isEmpty());

		const TerminalStripLayoutPattern pattern;
		const QRectF bounds = TerminalStripDrawer::Diagram::boundingRect(
					&strip, pattern);
		QVERIFY(bounds.width() > count * 18);

		QImage image(bounds.size().toSize() + QSize(4, 4),
					 QImage::Format_ARGB32_Premultiplied);
		image.fill(Qt::white);
		QPainter painter(&image);
		painter.translate(2, 2);
		QVector<TerminalStripDrawer::XrefRect> rects;
		TerminalStripDrawer::Diagram::paint(&painter, strip, pattern, false,
											QPointF(), &rects);
		painter.end();
		QCOMPARE(rects.size(), count);
		const QString out = qEnvironmentVariable("QET_TERMINAL_DIAGRAM_PNG");
		if (!out.isEmpty()) {
			image.save(out);
		}
	}
};

QET_APP_TEST_MAIN(tst_terminaldiagram)
#include "tst_terminaldiagram.moc"
