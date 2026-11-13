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
#include "elementterminaltextswidget.h"

#include "../diagram.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/terminal.h"
#include "../undocommand/changeterminaltextscommand.h"

#include <QHeaderView>
#include <QLabel>
#include <QSet>
#include <QTableWidget>
#include <QUndoStack>
#include <QVBoxLayout>

namespace {
	//What the first column keeps of its terminal
const int uuid_role = Qt::UserRole;
const int symbol_name_role = Qt::UserRole + 1;
}

ElementTerminalTextsWidget::ElementTerminalTextsWidget(Element *element,
													   QWidget *parent) :
	AbstractElementPropertiesEditorWidget(parent)
{
	auto *layout = new QVBoxLayout(this);
	auto *help = new QLabel(tr("The text of a terminal replaces its name "
							   "on the folio, in the terminal strip plans "
							   "and in the lists, and the number the "
							   "symbol draws beside the terminal changes "
							   "with it. Leave the name of the symbol, or "
							   "nothing, to keep that name."),
							this);
	help->setWordWrap(true);
	layout->addWidget(help);

	m_table = new QTableWidget(0, 2, this);
	m_table->setObjectName(QStringLiteral("terminal_texts_table"));
	m_table->setHorizontalHeaderLabels({tr("Terminal of the symbol"),
										tr("Text")});
	m_table->horizontalHeader()->setStretchLastSection(true);
	m_table->verticalHeader()->setVisible(false);
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setEditTriggers(QAbstractItemView::AllEditTriggers);
	layout->addWidget(m_table);

	setElement(element);
}

/**
	@brief ElementTerminalTextsWidget::isUsefulFor
	@return true if @p element has terminals whose names are shown and
	listed: not a folio reference, a thumbnail or a wire definition.
*/
bool ElementTerminalTextsWidget::isUsefulFor(const Element *element)
{
	if (!element || element->terminals().isEmpty()) {
		return false;
	}
	switch (element->linkType()) {
		case Element::Simple:
		case Element::Master:
		case Element::Slave:
		case Element::Terminale:
			return true;
		default:
			return false;
	}
}

void ElementTerminalTextsWidget::setElement(Element *element)
{
	disconnect(m_watch);
	m_element = element;
		//An undo, or another editor, changes what the table shows
	if (element) {
		m_watch = connect(element, &Element::terminalTextsChanged,
						  this, &ElementTerminalTextsWidget::updateUi);
	}
	updateUi();
}

QString ElementTerminalTextsWidget::title() const
{
	return tr("Terminal texts");
}

/**
	@brief ElementTerminalTextsWidget::updateUi
	One row per terminal, in the order of Element::terminals() (top to
	bottom, then left to right): the name the symbol gives it, and the
	text it is shown with. Terminals sharing a uuid share their text: the
	first of them stands for all.
*/
void ElementTerminalTextsWidget::updateUi()
{
	m_updating = true;
	m_table->setRowCount(0);
	if (m_element) {
		QSet<QString> seen;
		const QList<Terminal *> terminals = m_element->terminals();
		for (const Terminal *terminal : terminals) {
			const QString uuid = terminal->stableUuid().toString();
			if (seen.contains(uuid)) {
				continue;
			}
			seen.insert(uuid);

			const int row = m_table->rowCount();
			m_table->insertRow(row);
			const QString symbol_name = terminal->symbolName();
			auto *name = new QTableWidgetItem(
						symbol_name.isEmpty() ? tr("(no name)") : symbol_name);
			name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
			name->setData(uuid_role, uuid);
			name->setData(symbol_name_role, symbol_name);
			m_table->setItem(row, 0, name);
			m_table->setItem(row, 1, new QTableWidgetItem(terminal->name()));
		}
	}
	m_table->resizeColumnToContents(0);
	m_updating = false;
}

/**
	@brief ElementTerminalTextsWidget::editedTexts
	@return the texts of the table, by terminal uuid: only those which
	are not empty nor the name the symbol gives the terminal
*/
QMap<QString, QString> ElementTerminalTextsWidget::editedTexts() const
{
	QMap<QString, QString> texts;
	for (int row = 0 ; row < m_table->rowCount() ; ++row) {
		const QTableWidgetItem *name = m_table->item(row, 0);
		const QTableWidgetItem *text = m_table->item(row, 1);
		if (!name || !text) {
			continue;
		}
		const QString value = text->text().trimmed();
		if (value.isEmpty()
			|| value == name->data(symbol_name_role).toString()) {
			continue;
		}
		texts.insert(name->data(uuid_role).toString(), value);
	}
	return texts;
}

void ElementTerminalTextsWidget::apply()
{
	if (QUndoCommand *undo = associatedUndo()) {
		m_element->diagram()->undoStack().push(undo);
	}
}

QUndoCommand *ElementTerminalTextsWidget::associatedUndo() const
{
	if (!m_element || !m_element->diagram()) {
		return nullptr;
	}
	const QMap<QString, QString> texts = editedTexts();
	if (texts == m_element->terminalTexts()) {
		return nullptr;
	}
	return new ChangeTerminalTextsCommand(m_element, texts);
}

/**
	@brief ElementTerminalTextsWidget::setLiveEdit
	In the properties panel a text is applied as soon as it is typed.
*/
bool ElementTerminalTextsWidget::setLiveEdit(bool live_edit)
{
	if (m_live_edit == live_edit) {
		return true;
	}
	m_live_edit = live_edit;
	if (m_live_edit) {
		enableLiveEdit();
	} else {
		disableLiveEdit();
	}
	return true;
}

void ElementTerminalTextsWidget::enableLiveEdit()
{
	disableLiveEdit();
		//Queued: applying fills the table again, which must not happen
		//while the table is still telling of the edit
	m_live = connect(m_table, &QTableWidget::itemChanged, this, [this]() {
		if (!m_updating) {
			apply();
		}
	}, Qt::QueuedConnection);
}

void ElementTerminalTextsWidget::disableLiveEdit()
{
	disconnect(m_live);
}
