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
#include "wiresectionsdialog.h"

#include "../qetproject.h"

#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTableWidget>
#include <QUndoStack>
#include <QVBoxLayout>

namespace
{
	const QString method_key = QStringLiteral("wiresections/method");
	const QString ambient_key = QStringLiteral("wiresections/ambient");
	const QString grouped_key = QStringLiteral("wiresections/grouped");
}

WireSectionsDialog::WireSectionsDialog(QETProject *project, QWidget *parent) :
	QDialog(parent),
	m_project(project)
{
	setWindowTitle(tr("Calculate sections"));
	resize(820, 560);
	auto *layout = new QVBoxLayout(this);

	auto *intro = new QLabel(tr("The section IEC 60204-1 (table 6, copper, PVC) asks for the wires "
								"each protection feeds, from its rated current, and the wires each "
								"device feeds whose part has a maximum current, from it: the wires on "
								"its outgoing terminals (2, 4, 6…) and every wire joined to them."), this);
	intro->setWordWrap(true);
	layout->addWidget(intro);

	auto *conditions = new QHBoxLayout();
	conditions->addWidget(new QLabel(tr("Installation:"), this));
	m_method = new QComboBox(this);
	m_method->setObjectName(QStringLiteral("m_method"));
	m_method->addItem(tr("B1: single cores in a duct"), int(WireSections::Method::B1));
	m_method->addItem(tr("B2: cables in a duct"), int(WireSections::Method::B2));
	m_method->addItem(tr("C: on a wall"), int(WireSections::Method::C));
	m_method->addItem(tr("E: on a cable tray"), int(WireSections::Method::E));
	conditions->addWidget(m_method);
	conditions->addSpacing(12);
	conditions->addWidget(new QLabel(tr("Ambient:"), this));
	m_ambient = new QComboBox(this);
	for (int temperature : {30, 35, 40, 45, 50, 55, 60}) {
		m_ambient->addItem(QStringLiteral("%1 °C").arg(temperature), temperature);
	}
	conditions->addWidget(m_ambient);
	conditions->addSpacing(12);
	conditions->addWidget(new QLabel(tr("Circuits grouped:"), this));
	m_grouped = new QSpinBox(this);
	m_grouped->setRange(1, 20);
	conditions->addWidget(m_grouped);
	conditions->addStretch();
	layout->addLayout(conditions);

	QSettings settings;
	m_method->setCurrentIndex(qMax(0, m_method->findData(settings.value(method_key, 0).toInt())));
	m_ambient->setCurrentIndex(qMax(0, m_ambient->findData(settings.value(ambient_key, 40).toInt())));
	m_grouped->setValue(settings.value(grouped_key, 1).toInt());

	m_table = new QTableWidget(0, 6, this);
	m_table->setObjectName(QStringLiteral("m_table"));
	m_table->setHorizontalHeaderLabels({tr("Device"), tr("Current"),
										tr("Section needed"), tr("Wires"),
										tr("Without section"), tr("Too small")});
	m_table->verticalHeader()->hide();
	m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_table->setSelectionMode(QAbstractItemView::NoSelection);
	m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	layout->addWidget(m_table, 1);

	m_summary = new QLabel(this);
	m_summary->setWordWrap(true);
	layout->addWidget(m_summary);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	m_give = buttons->addButton(tr("Give them their section"), QDialogButtonBox::ActionRole);
	m_give->setObjectName(QStringLiteral("m_give"));
	layout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(m_give, &QPushButton::clicked, this, &WireSectionsDialog::giveSections);

	auto changed = [this]() {
		QSettings settings;
		settings.setValue(method_key, m_method->currentData());
		settings.setValue(ambient_key, m_ambient->currentData());
		settings.setValue(grouped_key, m_grouped->value());
		refresh();
	};
	connect(m_method, qOverload<int>(&QComboBox::currentIndexChanged), this, changed);
	connect(m_ambient, qOverload<int>(&QComboBox::currentIndexChanged), this, changed);
	connect(m_grouped, qOverload<int>(&QSpinBox::valueChanged), this, changed);
	refresh();
}

WireSections::Conditions WireSectionsDialog::conditions() const
{
	WireSections::Conditions conditions;
	conditions.method = WireSections::Method(m_method->currentData().toInt());
	conditions.ambient = m_ambient->currentData().toInt();
	conditions.grouped = m_grouped->value();
	return conditions;
}

void WireSectionsDialog::refresh()
{
	m_rows = WireSections::check(m_project, conditions());
	m_table->setRowCount(m_rows.size());
	int to_give = 0;
	for (int row = 0 ; row < m_rows.size() ; ++row)
	{
		const WireSections::Row &data = m_rows.at(row);
		const QString current = data.current > 0
				? QStringLiteral("%1 A").arg(QLocale().toString(data.current))
				: tr("not given");
		const QString needed = data.required > 0
				? WireSections::sectionText(data.required)
				: (data.current > 0 ? tr("over 120 mm²") : QStringLiteral("—"));
		m_table->setItem(row, 0, new QTableWidgetItem(data.label));
		m_table->setItem(row, 1, new QTableWidgetItem(current));
		m_table->setItem(row, 2, new QTableWidgetItem(needed));
		m_table->setItem(row, 3, new QTableWidgetItem(QString::number(data.wires.size())));
		m_table->setItem(row, 4, new QTableWidgetItem(QString::number(data.without)));
		auto *small = new QTableWidgetItem(QString::number(data.too_small));
		if (data.too_small) {
			small->setForeground(QColor(200, 30, 30));
		}
		m_table->setItem(row, 5, small);
		if (data.required > 0) {
			to_give += data.without + data.too_small;
		}
	}
	m_summary->setText(m_rows.isEmpty()
					   ? tr("The project has no protection (circuit breaker, motor protection, fuse) nor device with a maximum current.")
					   : tr("%n wire(s) without a section or with a smaller one than their protection asks for.",
							"", to_give));
	m_give->setEnabled(to_give > 0 && m_project && !m_project->isReadOnly());
}

void WireSectionsDialog::giveSections()
{
	if (!m_project) {
		return;
	}
	if (QUndoCommand *undo = WireSections::apply(m_rows)) {
		m_project->undoStack()->push(undo);
	}
	refresh();
}
