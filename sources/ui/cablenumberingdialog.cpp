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
#include "cablenumberingdialog.h"

#include "../cable/cablemanager.h"
#include "../cable/cablerenumber.h"
#include "../cable/editcablecommand.h"
#include "../qetproject.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QUndoStack>
#include <QVBoxLayout>

/**
	@brief CableNumberingDialog::CableNumberingDialog
	Build the window: what a cable is called, the axis the cables are
	laid out along when the whole project is numbered again, and the
	button which does that numbering.
	@param project the project whose cables are numbered
	@param parent
*/
CableNumberingDialog::CableNumberingDialog(QETProject *project, QWidget *parent) :
	QDialog(parent),
	m_project(project)
{
	setWindowTitle(tr("Cable numbering"));

	auto *layout = new QVBoxLayout(this);

	const QString base = CableManager::baseName();
	auto *note = new QLabel(tr(
		"A new cable takes the first number free in the project: %1, "
		"%2... Renumbering names every cable of the project again in "
		"the order below, which closes the gaps left by deleted cables.")
		.arg(base + QLatin1Char('1'), base + QLatin1Char('2')), this);
	note->setWordWrap(true);
	layout->addWidget(note);

	auto *axis_group = new QGroupBox(tr("Axis priority"), this);
	auto *axis_layout = new QHBoxLayout(axis_group);
	m_axis_x = new QRadioButton(tr("Priority to the X axis (horizontal)"), axis_group);
	m_axis_y = new QRadioButton(tr("Priority to the Y axis (vertical)"), axis_group);
	axis_layout->addWidget(m_axis_x);
	axis_layout->addWidget(m_axis_y);
	layout->addWidget(axis_group);

	auto *axis_note = new QLabel(tr(
		"The chosen axis decides the order in which the cables are "
		"renumbered: X numbers them from left to right, Y numbers them "
		"from top to bottom."), this);
	axis_note->setWordWrap(true);
	layout->addWidget(axis_note);

	auto *actions = new QHBoxLayout();
	m_renumber_pb = new QPushButton(tr("Renumber all cables"), this);
	m_renumber_pb->setToolTip(tr(
		"Sets the number of each cable of the project again, starting "
		"at 1: a number left missing after a cable is deleted is filled "
		"in this way."));
	actions->addWidget(m_renumber_pb);
	actions->addStretch();

	m_box = new QDialogButtonBox(QDialogButtonBox::Close, this);
	actions->addWidget(m_box);
	layout->addLayout(actions);

	connect(m_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(m_renumber_pb, &QPushButton::clicked, this, &CableNumberingDialog::renumber);

	const bool editable = m_project && !m_project->isReadOnly();
	if (m_project)
	{
		m_axis_x->setChecked(m_project->cableXAxisFirst());
		m_axis_y->setChecked(!m_project->cableXAxisFirst());
	}
	m_axis_x->setEnabled(editable);
	m_axis_y->setEnabled(editable);
	m_renumber_pb->setEnabled(editable);
		//The axis is kept with the project as soon as it is chosen
	connect(m_axis_x, &QRadioButton::toggled, this, &CableNumberingDialog::saveAxis);
}

/**
	@brief CableNumberingDialog::saveAxis
	Write the axis into the project, when it changed.
*/
void CableNumberingDialog::saveAxis()
{
	if (!m_project || m_project->isReadOnly()) return;

	const bool axis_first = m_axis_x->isChecked();
	if (m_project->cableXAxisFirst() == axis_first) return;
	m_project->setCableXAxisFirst(axis_first);
	m_project->setModified(true);
}

/**
	@brief CableNumberingDialog::askAboutHandWritten
	Some cables carry a name he typed in himself rather than one the
	numbering handed out. Taking such a name away is his to allow: yes and
	they are numbered like all the others, no and they keep their name
	while the rest is numbered around them, and cancelling leaves every
	cable exactly where it stands.
	@param count how many such names the project holds
	@return 1 to number them too, 0 to leave them alone, -1 for nothing
*/
int CableNumberingDialog::askAboutHandWritten(int count)
{
	if (count <= 0) return 1;

	QMessageBox box(QMessageBox::Question, tr("Cable numbering"),
					tr("%n cable(s) of this project were entered by "
					   "hand.\n\n"
					   "Do you want to renumber them too?", "", count),
					QMessageBox::NoButton, this);
	QPushButton *take = box.addButton(
		tr("Yes", "Les noms saisis à la main sont remplacés eux aussi."),
		QMessageBox::YesRole);
	QPushButton *leave = box.addButton(
		tr("No", "Les noms saisis à la main sont laissés tels quels."),
		QMessageBox::NoRole);
	QPushButton *cancel = box.addButton(
		tr("Cancel", "Ne renumérote rien du tout."),
		QMessageBox::RejectRole);
		//Keeping his own names is the safe answer under his finger
	box.setDefaultButton(leave);
	box.setEscapeButton(cancel);
	box.exec();

	const QAbstractButton *clicked = box.clickedButton();
	if (clicked == take) return 1;
	if (clicked == leave) return 0;
	return -1;
}

/**
	@brief CableNumberingDialog::renumber
	Name every cable of the project again, -W1, -W2..., in the order the
	cables stand along the chosen axis. One single undo step takes it
	all back, and the window closes once the numbering has been written
	so that he can see what became of his cables.
*/
void CableNumberingDialog::renumber()
{
	if (!m_project || m_project->isReadOnly()) return;

	const bool axis_first = m_axis_x->isChecked();
	CableRenumberPlan plan = CableRenumber::plan(m_project, axis_first, true);
	if (!plan.ok())
	{
		QMessageBox::warning(this, tr("Cable numbering"), plan.error);
		return;
	}

	const int answer = askAboutHandWritten(CableRenumber::byHandCount(m_project));
	if (answer < 0) return;

		//Leaving his own names alone means the numbering runs around
		//them: worked out again without them
	if (answer == 0)
	{
		plan = CableRenumber::plan(m_project, axis_first, false);
		if (!plan.ok())
		{
			QMessageBox::warning(this, tr("Cable numbering"), plan.error);
			return;
		}
	}
	if (plan.cables.isEmpty())
	{
		QMessageBox::information(this, tr("Cable numbering"),
								 tr("No cable to renumber in this project."));
		return;
	}

		//Every cable already carries its number: there is nothing to
		//write, and an undo step which undoes nothing would only be in
		//the way
	bool changed = false;
	for (int i = 0; i < plan.cables.size(); ++i)
	{
		if (plan.cables.at(i)->designation() != plan.designations.at(i)
			|| plan.cables.at(i)->designationByHand()) {
			changed = true;
			break;
		}
	}
	if (!changed)
	{
		QMessageBox::information(this, tr("Cable numbering"),
								 tr("All the cables already carry the number "
									"they would be given."));
		return;
	}

	if (QUndoStack *stack = m_project->undoStack()) {
		stack->push(new RenumberCablesCommand(plan.cables, plan.designations));
	}
	accept();
}
