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
#include "terminalpropertieswidget.h"

#include "faseterminal.h"
#include "terminalsettingswidget.h"
#include "../diagram.h"
#include "../qetgraphicsitem/element.h"

#include <QVBoxLayout>

TerminalPropertiesWidget::TerminalPropertiesWidget(Element *element,
												   QWidget *parent) :
	AbstractElementPropertiesEditorWidget(parent)
{
	m_layout = new QVBoxLayout(this);
	m_layout->addStretch();
	setElement(element);
}

void TerminalPropertiesWidget::setElement(Element *element)
{
	disconnect(m_watch);
	m_element = element;
		//An undo, or another tab, changes what the form shows
	if (element) {
		m_watch = connect(element, &Element::elementInfoChange,
							  this, &TerminalPropertiesWidget::updateUi);
	}
	if (m_form) {
		delete m_form;
		m_form = nullptr;
	}
	QETProject *project = element && element->diagram()
			? element->diagram()->project() : nullptr;
	m_form = new TerminalSettingsWidget(project, this);
	m_layout->insertWidget(0, m_form);
	if (m_live_edit) {
		enableLiveEdit();
	}
	updateUi();
}

void TerminalPropertiesWidget::updateUi()
{
	if (m_form && m_element) {
		m_form->setSettings(FaseTerminal::settingsOf(m_element));
	}
}

void TerminalPropertiesWidget::apply()
{
	if (QUndoCommand *undo = associatedUndo()) {
		m_element->diagram()->undoStack().push(undo);
	}
}

QUndoCommand *TerminalPropertiesWidget::associatedUndo() const
{
	if (!m_form || !m_element || !m_element->diagram()) {
		return nullptr;
	}
	const FaseTerminal::Settings settings = m_form->settings();
	if (settings == FaseTerminal::settingsOf(m_element)) {
		return nullptr;
	}
	return new FaseTerminal::ChangeCommand(m_element, settings);
}

/**
	@brief TerminalPropertiesWidget::setLiveEdit
	In the properties panel every change is applied at once.
*/
bool TerminalPropertiesWidget::setLiveEdit(bool live_edit)
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

void TerminalPropertiesWidget::enableLiveEdit()
{
	disableLiveEdit();
	if (m_form) {
		m_live = connect(m_form, &TerminalSettingsWidget::changed,
						 this, &TerminalPropertiesWidget::apply);
	}
}

void TerminalPropertiesWidget::disableLiveEdit()
{
	disconnect(m_live);
}
