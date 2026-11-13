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

#include "diagram.h"
#include "partsdb/elementpartswidget.h"
#include "partsdb/partslinks.h"
#include "qetgraphicsitem/element.h"
#include "qetproject.h"
#include "ui/elementinfowidget.h"
#include "ui/elementpropertieswidget.h"

#include <QTabWidget>
#include <QUndoStack>

#include <QTableWidget>
#include <QUndoCommand>

/**
	The Parts tab of the element properties keeps its list in the element
	information "parts", saved by the Information tab:

	 - the list goes into the element with the rest of the information, as
	   one step, and undo takes it back;
	 - it shows in the Parts tab, never as a loose row of the Information
	   tab, and is kept when another field is edited.

	fixtures/every_feature.qet is the project of the other app tests.
*/
class tst_elementparts : public QObject
{
	Q_OBJECT

	QETProject *m_project = nullptr;
	Element *m_element = nullptr;

private slots:
	void init()
	{
		const QString fixture = QFINDTESTDATA("fixtures/every_feature.qet");
		QVERIFY(!fixture.isEmpty());
		m_project = new QETProject(fixture);
		QCOMPARE(m_project->state(), QETProject::Ok);
		for (Diagram *diagram : m_project->diagrams()) {
			for (Element *element : diagram->elements()) {
				if (element->linkType() == Element::Simple) {
					m_element = element;
					break;
				}
			}
			if (m_element) {
				break;
			}
		}
		QVERIFY(m_element);
	}

	void cleanup()
	{
		delete m_project;
		m_project = nullptr;
		m_element = nullptr;
	}

	void listIsSavedWithTheInformation()
	{
		const DiagramContext before = m_element->elementInformations();
		const QString value = PartsLinks::encode(
			{{QStringLiteral("ABB"), QStringLiteral("S201-C10"), 2}});

		ElementInfoWidget info(m_element);
		ElementPartsWidget parts(m_element, &info);
		info.setPartsValue(value);
		QCOMPARE(parts.findChild<QTableWidget *>()->rowCount(), 1);

		QUndoCommand *command = info.associatedUndo();
		QVERIFY(command);
		command->redo();
		QCOMPARE(m_element->elementInformations()
				 .value(PartsLinks::infoKey()).toString(), value);
		command->undo();
		QVERIFY(m_element->elementInformations() == before);
		delete command;
	}

	void listIsNoLooseRowAndIsKept()
	{
		DiagramContext with_parts = m_element->elementInformations();
		const QString value = PartsLinks::encode(
			{{QStringLiteral("ABB"), QStringLiteral("S201-C10"), 1}});
		with_parts.addValue(PartsLinks::infoKey(), value);
		m_element->setElementInformations(with_parts);

		ElementInfoWidget info(m_element);
		QCOMPARE(info.partsValue(), value);
			//No row of the Information tab holds it, yet the information
			//it gives back keeps it
		QCOMPARE(info.currentInfo().value(PartsLinks::infoKey()).toString(),
				 value);
		ElementPartsWidget parts(m_element, &info);
		QCOMPARE(parts.findChild<QTableWidget *>()->rowCount(), 1);
	}

		//A contact has a Parts tab of its own, an auxiliary contact block
		//say; its Information tab saves the list and is not shown
	void aContactHasAPartsTab()
	{
		Element *contact = nullptr;
		for (Diagram *diagram : m_project->diagrams()) {
			for (Element *element : diagram->elements()) {
				if (!contact && element->linkType() == Element::Slave) {
					contact = element;
				}
			}
		}
		QVERIFY(contact);

		ElementPropertiesWidget dialog(contact);
		auto *tabs = dialog.findChild<QTabWidget *>();
		auto *parts = dialog.findChild<ElementPartsWidget *>();
		auto *info = dialog.findChild<ElementInfoWidget *>();
		QVERIFY(tabs && parts && info);
		QVERIFY(tabs->indexOf(parts) >= 0);
		QCOMPARE(tabs->indexOf(info), -1);

		const QString value = PartsLinks::encode(
			{{QStringLiteral("ACME"), QStringLiteral("AUX-1122"), 1}});
		info->setPartsValue(value);
		QCOMPARE(parts->findChild<QTableWidget *>()->rowCount(), 1);
		dialog.apply();
		QCOMPARE(contact->elementInformations()
				 .value(PartsLinks::infoKey()).toString(), value);
		contact->diagram()->undoStack().undo();
		QVERIFY(contact->elementInformations()
				.value(PartsLinks::infoKey()).toString().isEmpty());
	}
};

QET_APP_TEST_MAIN(tst_elementparts)
#include "tst_elementparts.moc"
