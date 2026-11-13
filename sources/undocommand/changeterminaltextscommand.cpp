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
#include "changeterminaltextscommand.h"

#include "../diagram.h"
#include "../qetgraphicsitem/dynamicelementtextitem.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/elementtextitemgroup.h"
#include "../qetgraphicsitem/terminal.h"
#include "../qetproject.h"

#include <QCoreApplication>
#include <QLineF>
#include <QSet>

ChangeTerminalTextsCommand::ChangeTerminalTextsCommand(
		Element *element, const QMap<QString, QString> &texts,
		QUndoCommand *parent) :
	QUndoCommand(parent),
	m_element(element),
	m_old_texts(element ? element->terminalTexts()
						: QMap<QString, QString>()),
	m_new_texts(texts)
{
	setText(QCoreApplication::translate("ChangeTerminalTextsCommand",
										"Change the texts of the terminals"));
	findDrawnNumbers();
}

/**
	@brief ChangeTerminalTextsCommand::findDrawnNumbers
	For each terminal whose name changes, the text of the symbol which
	draws its number: a typed text saying the name the terminal has now,
	the nearest of those to it and at most number_distance away. A text
	goes with one terminal only.
*/
void ChangeTerminalTextsCommand::findDrawnNumbers()
{
	if (!m_element) {
		return;
	}
	QList<DynamicElementTextItem *> candidates = m_element->dynamicTextItems();
	const QList<ElementTextItemGroup *> groups = m_element->textGroups();
	for (const ElementTextItemGroup *group : groups) {
		candidates += group->texts();
	}

	QSet<DynamicElementTextItem *> taken;
	const QList<Terminal *> terminals = m_element->terminals();
	for (const Terminal *terminal : terminals) {
		const QString own = m_new_texts.value(terminal->stableUuid().toString());
		const QString before = terminal->name();
		const QString after = own.isEmpty() ? terminal->symbolName() : own;
			//A terminal without a name has no number to find, and one
			//going back to none keeps the number drawn
		if (before.isEmpty() || after.isEmpty() || before == after) {
			continue;
		}

		DynamicElementTextItem *nearest = nullptr;
		qreal best = number_distance;
		const QPointF dock = terminal->dockConductor();
		for (DynamicElementTextItem *text : std::as_const(candidates)) {
			if (taken.contains(text)
				|| text->textFrom() != DynamicElementTextItem::UserText
				|| text->text() != before) {
				continue;
			}
			const qreal distance =
					QLineF(text->sceneBoundingRect().center(), dock).length();
			if (distance <= best) {
				best = distance;
				nearest = text;
			}
		}
		if (nearest) {
			taken.insert(nearest);
			m_numbers.append({nearest, before, after});
		}
	}
}

void ChangeTerminalTextsCommand::undo()
{
	apply(m_old_texts, false);
}

void ChangeTerminalTextsCommand::redo()
{
	apply(m_new_texts, true);
}

void ChangeTerminalTextsCommand::apply(const QMap<QString, QString> &texts,
									   bool forward)
{
	if (!m_element) {
		return;
	}
	m_element->setTerminalTexts(texts);
	for (const DrawnNumber &number : std::as_const(m_numbers)) {
		if (number.text) {
			number.text->setText(forward ? number.new_text
										 : number.old_text);
		}
	}

		//A terminal strip plan reads the names of the terminals when it
		//is drawn, on whatever folio it is
	Diagram *diagram = m_element->diagram();
	QETProject *project = diagram ? diagram->project() : nullptr;
	if (!project) {
		return;
	}
	const QList<Diagram *> folios = project->diagrams();
	for (Diagram *folio : folios) {
		if (folio && folio->isLoaded()) {
			folio->update();
		}
	}
}
