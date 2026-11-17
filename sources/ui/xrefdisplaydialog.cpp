// SPDX-License-Identifier: GPL-2.0-or-later
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
#include "xrefdisplaydialog.h"

#include "xrefdisplayform.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

/**
	@brief XRefDisplayDialog::XRefDisplayDialog
	@param display the master's display now: not enabled when it follows the
	project's setting, in which case the fields start from the defaults
	@param symbol_pos where the master stands on the folio, for a manual
	position measured from the folio
	@param suggested_manual the offset from the symbol where "Manual position" starts, when the master
	has no position of its own yet
	@param parent
*/
XRefDisplayDialog::XRefDisplayDialog(const ElementData::XRefDisplay &display,
									 const QPointF &symbol_pos,
									 const QPointF &suggested_manual,
									 QWidget *parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Relay contact display"));
	setMinimumWidth(420);
	auto *layout = new QVBoxLayout(this);

	auto *note = new QLabel(tr("The contact display keeps its electrical links. "
							   "For a manual position, Shift-drag the display on the folio, "
							   "or enter the offset of its top left corner from the symbol."));
	note->setWordWrap(true);
	layout->addWidget(note);

	m_form = new XRefDisplayForm(this);
	m_form->setDisplay(display, symbol_pos, suggested_manual);
	layout->addWidget(m_form);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	auto *restore = buttons->addButton(tr("Use the project setting"), QDialogButtonBox::ResetRole);
	layout->addWidget(buttons);
	connect(restore, &QPushButton::clicked, this, [this]() {
		m_restore = true;
		accept();
	});
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

/**
	@brief XRefDisplayDialog::display
	@return what the user chose: not enabled if they asked for the project's
	setting
*/
ElementData::XRefDisplay XRefDisplayDialog::display() const
{
	if (m_restore)
		return ElementData::XRefDisplay();
	return m_form->display();
}
