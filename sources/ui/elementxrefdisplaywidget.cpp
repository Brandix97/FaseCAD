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
#include "elementxrefdisplaywidget.h"

#include "xrefdisplayform.h"
#include "../diagram.h"
#include "../properties/xrefproperties.h"
#include "../qetgraphicsitem/crossrefitem.h"
#include "../qetgraphicsitem/element.h"
#include "../qetproject.h"
#include "../undocommand/changeelementdatacommand.h"

#include <QCheckBox>
#include <QLabel>
#include <QUndoStack>
#include <QVBoxLayout>

ElementXRefDisplayWidget::ElementXRefDisplayWidget(Element *element, QWidget *parent) :
	AbstractElementPropertiesEditorWidget(parent)
{
	auto *layout = new QVBoxLayout(this);
	auto *help = new QLabel(tr("How the contacts of this device are shown beside "
							   "it. For a manual position, Shift-drag the display "
							   "on the folio, or enter where it stands."), this);
	help->setWordWrap(true);
	layout->addWidget(help);

	m_project_cb = new QCheckBox(tr("Use the project setting"), this);
	m_project_cb->setObjectName(QStringLiteral("m_project_cb"));
	m_project_cb->setToolTip(tr("Project properties, Cross references page"));
	layout->addWidget(m_project_cb);

	m_form = new XRefDisplayForm(this);
	m_form->setObjectName(QStringLiteral("m_form"));
	layout->addWidget(m_form);
	layout->addStretch();

	connect(m_project_cb, &QCheckBox::toggled, this, [this](bool project) {
		m_form->setEnabled(!project);
		changedByTheUser();
	});

	setElement(element);
}

/**
	@brief ElementXRefDisplayWidget::isUsefulFor
	@return true for a master which shows its contacts: a PLC places its
	own table instead
*/
bool ElementXRefDisplayWidget::isUsefulFor(const Element *element)
{
	return element && element->linkType() == Element::Master
			&& element->elementData().m_master_type != ElementData::PLC;
}

void ElementXRefDisplayWidget::setElement(Element *element)
{
	disconnect(m_watch);
	m_element = element;
		//An undo, the dialog or a Shift-drag changes what is shown
	if (element && element->diagram() && element->diagram()->project()) {
		m_watch = connect(element->diagram()->project(),
						  &QETProject::XRefPropertiesChanged,
						  this, &ElementXRefDisplayWidget::updateUi);
	}
	updateUi();
}

QString ElementXRefDisplayWidget::title() const
{
	return tr("Contact mirror");
}

/**
	@brief ElementXRefDisplayWidget::startingDisplay
	@return the display of the master, or, while it follows the project,
	what the project shows now, to start from when it gets its own
*/
ElementData::XRefDisplay ElementXRefDisplayWidget::startingDisplay() const
{
	ElementData::XRefDisplay start = m_element->elementData().m_xref_display;
	if (!start.enabled)
	{
		const XRefProperties properties = CrossRefItem::propertiesFor(m_element);
		start.contacts = properties.displayHas() == XRefProperties::Contacts;
		start.position = properties.snapTo() == XRefProperties::Label
				? ElementData::XRefDisplay::UnderLabel
				: ElementData::XRefDisplay::Footer;
		start.showUnlinked = properties.showAllConfiguredSlaves();
	}
	return start;
}

void ElementXRefDisplayWidget::updateUi()
{
	if (!m_element) return;
	m_updating = true;
	const ElementData::XRefDisplay display = m_element->elementData().m_xref_display;
	const QRectF body = m_element->mapRectToScene(m_element->drawingRect());
	m_project_cb->setChecked(!display.enabled);
	m_form->setDisplay(startingDisplay(), m_element->scenePos(),
					   QPointF(body.left(), body.bottom() + 10) - m_element->scenePos());
	m_form->setEnabled(display.enabled);
	m_updating = false;
}

ElementData::XRefDisplay ElementXRefDisplayWidget::editedDisplay() const
{
	return m_project_cb->isChecked() ? ElementData::XRefDisplay()
									 : m_form->display();
}

void ElementXRefDisplayWidget::apply()
{
	if (QUndoCommand *undo = associatedUndo()) {
		m_element->diagram()->undoStack().push(undo);
	}
}

QUndoCommand *ElementXRefDisplayWidget::associatedUndo() const
{
	if (!m_element || !m_element->diagram()) {
		return nullptr;
	}
	ElementData data = m_element->elementData();
	data.m_xref_display = editedDisplay();
	if (data == m_element->elementData()) {
		return nullptr;
	}
	auto *command = new ChangeElementDataCommand(m_element, data);
	command->setText(tr("Change relay contact display"));
	return command;
}

bool ElementXRefDisplayWidget::setLiveEdit(bool live_edit)
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

void ElementXRefDisplayWidget::enableLiveEdit()
{
	connect(m_form, &XRefDisplayForm::changed,
			this, &ElementXRefDisplayWidget::changedByTheUser);
}

void ElementXRefDisplayWidget::disableLiveEdit()
{
	disconnect(m_form, &XRefDisplayForm::changed,
			   this, &ElementXRefDisplayWidget::changedByTheUser);
}

/**
	@brief ElementXRefDisplayWidget::changedByTheUser
	In the properties panel a change is applied as soon as it is made.
*/
void ElementXRefDisplayWidget::changedByTheUser()
{
	if (!m_updating && m_live_edit) {
		apply();
	}
}
