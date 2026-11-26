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
#include "wirenumberingdialog.h"

#include "../diagram.h"
#include "../qetproject.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace
{
	const QString custom_id = QStringLiteral("custom");
}

WireNumberingDialog::WireNumberingDialog(QETProject *project, Diagram *sheet,
										 const QList<Conductor *> &selection,
										 QWidget *parent) :
	QDialog(parent),
	m_project(project),
	m_sheet(sheet),
	m_selection(selection),
	m_examples(WireNumbering::examples())
{
	setWindowTitle(tr("Number the wires"));
	resize(860, 640);
	auto *layout = new QVBoxLayout(this);

	auto *form = new QFormLayout();
	auto *scheme_row = new QHBoxLayout();
	m_scheme = new QComboBox(this);
	m_scheme->setObjectName(QStringLiteral("m_scheme"));
	scheme_row->addWidget(m_scheme, 1);
	m_save = new QPushButton(tr("Save as mine…"), this);
	m_save->setToolTip(tr("Keep this formula among your own, for every project"));
	m_delete = new QPushButton(tr("Delete"), this);
	m_delete->setToolTip(tr("Delete this formula of your own"));
	scheme_row->addWidget(m_save);
	scheme_row->addWidget(m_delete);
	form->addRow(tr("Numbering:"), scheme_row);

	m_formula = new QLineEdit(this);
	m_formula->setObjectName(QStringLiteral("m_formula"));
	form->addRow(tr("Formula:"), m_formula);
	auto *help = new QLabel(tr("%N number in the project · %n number on the sheet · %F sheet · "
							   "%C column · %R row · %P potential type (L1, N, PE, P1, M1) · "
							   "%S device and %T terminal it starts from · %D device and %E "
							   "terminal it goes to · %3N three digits: 001"), this);
	help->setWordWrap(true);
	help->setEnabled(false);
	form->addRow(QString(), help);
	m_per_wire = new QCheckBox(tr("A name for each wire, not one for each potential"), this);
	m_per_wire->setObjectName(QStringLiteral("m_per_wire"));
	form->addRow(QString(), m_per_wire);
	layout->addLayout(form);

	auto *which = new QGroupBox(tr("Wires"), this);
	auto *which_layout = new QHBoxLayout(which);
	m_whole_project = new QRadioButton(tr("Whole project"), which);
	m_this_sheet = new QRadioButton(tr("This sheet"), which);
	m_selected = new QRadioButton(tr("Selected wires"), which);
	m_whole_project->setChecked(true);
	m_this_sheet->setEnabled(m_sheet && !m_sheet->isCabinetFolio() && !m_sheet->isRoutingFolio());
	m_selected->setEnabled(!m_selection.isEmpty());
	which_layout->addWidget(m_whole_project);
	which_layout->addWidget(m_this_sheet);
	which_layout->addWidget(m_selected);
	which_layout->addStretch();
	layout->addWidget(which);

	auto *more = new QHBoxLayout();
	more->addWidget(new QLabel(tr("Order on a sheet:"), this));
	m_order = new QComboBox(this);
	m_order->addItem(tr("From top to bottom, then left to right"), int(WireNumbering::Order::RowsFirst));
	m_order->addItem(tr("From left to right, then top to bottom"), int(WireNumbering::Order::ColumnsFirst));
	more->addWidget(m_order);
	more->addSpacing(12);
	more->addWidget(new QLabel(tr("Start at:"), this));
	m_start = new QSpinBox(this);
	m_start->setRange(0, 99999);
	m_start->setValue(1);
	more->addWidget(m_start);
	more->addSpacing(12);
	m_keep_fixed = new QCheckBox(tr("Keep the numbers fixed by hand"), this);
	m_keep_fixed->setChecked(true);
	more->addWidget(m_keep_fixed);
	more->addStretch();
	layout->addLayout(more);

	m_count = new QLabel(this);
	layout->addWidget(m_count);
	m_preview = new QTableWidget(0, 4, this);
	m_preview->setObjectName(QStringLiteral("m_preview"));
	m_preview->setHorizontalHeaderLabels({tr("Sheet"), tr("Now"), tr("New"), tr("From → to")});
	m_preview->verticalHeader()->hide();
	m_preview->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_preview->setSelectionMode(QAbstractItemView::NoSelection);
	m_preview->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
	layout->addWidget(m_preview, 1);

	m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	m_buttons->button(QDialogButtonBox::Ok)->setText(tr("Number"));
	layout->addWidget(m_buttons);
	connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	m_refresh.setSingleShot(true);
	m_refresh.setInterval(250);
	connect(&m_refresh, &QTimer::timeout, this, &WireNumberingDialog::refresh);
	connect(m_scheme, qOverload<int>(&QComboBox::currentIndexChanged), this, &WireNumberingDialog::schemeChosen);
	connect(m_formula, &QLineEdit::textEdited, this, &WireNumberingDialog::formulaEdited);
	connect(m_per_wire, &QCheckBox::toggled, this, &WireNumberingDialog::formulaEdited);
	for (QRadioButton *button : {m_whole_project, m_this_sheet, m_selected}) {
		connect(button, &QRadioButton::toggled, this, &WireNumberingDialog::scheduleRefresh);
	}
	connect(m_order, qOverload<int>(&QComboBox::currentIndexChanged), this, &WireNumberingDialog::scheduleRefresh);
	connect(m_start, qOverload<int>(&QSpinBox::valueChanged), this, &WireNumberingDialog::scheduleRefresh);
	connect(m_keep_fixed, &QCheckBox::toggled, this, &WireNumberingDialog::scheduleRefresh);
	connect(m_save, &QPushButton::clicked, this, &WireNumberingDialog::saveScheme);
	connect(m_delete, &QPushButton::clicked, this, &WireNumberingDialog::deleteScheme);

	fillSchemes();
	schemeChosen();
}

WireNumbering::Options WireNumberingDialog::options() const
{
	WireNumbering::Options options;
	options.formula = m_formula->text();
	options.per_wire = m_per_wire->isChecked();
	options.scope = m_selected->isChecked() ? WireNumbering::Scope::Selection
			: m_this_sheet->isChecked() ? WireNumbering::Scope::Sheet
										: WireNumbering::Scope::Project;
	options.sheet = m_sheet;
	options.selection = m_selection;
	options.order = WireNumbering::Order(m_order->currentData().toInt());
	options.start = m_start->value();
	options.keep_fixed = m_keep_fixed->isChecked();
	return options;
}

void WireNumberingDialog::fillSchemes(const QString &select)
{
	m_filling = true;
	m_own = WireNumbering::ownSchemes();
	m_scheme->clear();
	for (int i = 0 ; i < m_examples.size() ; ++i) {
		m_scheme->addItem(m_examples.at(i).name, QStringLiteral("example:%1").arg(i));
	}
	if (!m_own.isEmpty()) {
		m_scheme->insertSeparator(m_scheme->count());
		for (int i = 0 ; i < m_own.size() ; ++i) {
			m_scheme->addItem(tr("Mine: %1").arg(m_own.at(i).name), QStringLiteral("own:%1").arg(i));
		}
	}
	m_scheme->insertSeparator(m_scheme->count());
	m_scheme->addItem(tr("Formula of my own"), custom_id);
	const int index = select.isEmpty() ? 0 : m_scheme->findData(select);
	m_scheme->setCurrentIndex(index < 0 ? 0 : index);
	m_filling = false;
}

void WireNumberingDialog::schemeChosen()
{
	if (m_filling) {
		return;
	}
	const QString id = m_scheme->currentData().toString();
	const WireNumbering::Scheme *scheme = nullptr;
	if (id.startsWith(QLatin1String("example:"))) {
		scheme = &m_examples.at(id.mid(8).toInt());
	} else if (id.startsWith(QLatin1String("own:"))) {
		scheme = &m_own.at(id.mid(4).toInt());
	}
	if (scheme) {
		const QSignalBlocker blocker(m_per_wire);
		m_formula->setText(scheme->formula);
		m_per_wire->setChecked(scheme->per_wire);
	}
	m_delete->setEnabled(id.startsWith(QLatin1String("own:")));
	refresh();
}

void WireNumberingDialog::formulaEdited()
{
		//A formula changed by hand is one's own
	const int custom = m_scheme->findData(custom_id);
	if (custom >= 0 && m_scheme->currentIndex() != custom) {
		m_filling = true;
		m_scheme->setCurrentIndex(custom);
		m_filling = false;
		m_delete->setEnabled(false);
	}
	scheduleRefresh();
}

void WireNumberingDialog::scheduleRefresh()
{
	m_refresh.start();
}

void WireNumberingDialog::refresh()
{
	m_refresh.stop();
	m_changes = WireNumbering::plan(m_project, options());
	m_preview->setRowCount(0);
	m_preview->setRowCount(m_changes.size());
	for (int row = 0 ; row < m_changes.size() ; ++row)
	{
		const WireNumbering::Change &change = m_changes.at(row);
		const QString sheet = change.sheet && m_project
				? QString::number(m_project->folioIndex(change.sheet) + 1) : QString();
		m_preview->setItem(row, 0, new QTableWidgetItem(sheet));
		m_preview->setItem(row, 1, new QTableWidgetItem(change.before));
		auto *after = new QTableWidgetItem(change.after);
		QFont bold = after->font();
		bold.setBold(true);
		after->setFont(bold);
		m_preview->setItem(row, 2, after);
		m_preview->setItem(row, 3, new QTableWidgetItem(
							   QStringLiteral("%1 → %2").arg(change.from, change.to)));
	}
	m_preview->resizeColumnsToContents();
	m_preview->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
	m_count->setText(m_changes.isEmpty()
					 ? tr("Nothing to change: every wire already has this name.")
					 : tr("%n name(s) change:", "", m_changes.size()));
	m_buttons->button(QDialogButtonBox::Ok)->setEnabled(!m_changes.isEmpty());
}

void WireNumberingDialog::saveScheme()
{
	bool ok = false;
	const QString name = QInputDialog::getText(this, tr("Save as mine"),
											   tr("Name of this numbering:"),
											   QLineEdit::Normal, QString(), &ok).trimmed();
	if (!ok || name.isEmpty() || m_formula->text().trimmed().isEmpty()) {
		return;
	}
	QList<WireNumbering::Scheme> own = WireNumbering::ownSchemes();
	int index = -1;
	for (int i = 0 ; i < own.size() ; ++i) {
		if (own.at(i).name == name) {
			index = i;
		}
	}
	const WireNumbering::Scheme scheme{name, m_formula->text(), m_per_wire->isChecked()};
	if (index >= 0) {
		own[index] = scheme;
	} else {
		own << scheme;
		index = own.size() - 1;
	}
	WireNumbering::setOwnSchemes(own);
	fillSchemes(QStringLiteral("own:%1").arg(index));
	schemeChosen();
}

void WireNumberingDialog::deleteScheme()
{
	const QString id = m_scheme->currentData().toString();
	if (!id.startsWith(QLatin1String("own:"))) {
		return;
	}
	QList<WireNumbering::Scheme> own = WireNumbering::ownSchemes();
	const int index = id.mid(4).toInt();
	if (index >= 0 && index < own.size()) {
		own.removeAt(index);
		WireNumbering::setOwnSchemes(own);
	}
	fillSchemes();
	schemeChosen();
}
