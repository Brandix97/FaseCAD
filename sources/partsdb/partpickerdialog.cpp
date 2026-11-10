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
#include "partpickerdialog.h"

#include "partsmanagerdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

const int id_role = Qt::UserRole;

} // namespace

PartPickerDialog::PartPickerDialog(PartsDatabase *database, QWidget *parent) :
	QDialog(parent),
	m_db(database)
{
	setWindowTitle(tr("Choose a part"));
	resize(900, 520);
	auto *layout = new QVBoxLayout(this);

	auto *filters = new QHBoxLayout();
	m_search = new QLineEdit(this);
	m_search->setPlaceholderText(tr("Search a part"));
	m_search->setClearButtonEnabled(true);
	m_category = new QComboBox(this);
	filters->addWidget(m_search, 1);
	filters->addWidget(new QLabel(tr("Category:"), this));
	filters->addWidget(m_category);
	layout->addLayout(filters);

	m_table = new QTableWidget(0, 6, this);
	m_table->setHorizontalHeaderLabels({
		QObject::tr("Manufacturer"), QObject::tr("Order number"),
		QObject::tr("Textual description"), QObject::tr("Article number"),
		QObject::tr("Category"), QObject::tr("Internal number")});
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setSelectionMode(QAbstractItemView::SingleSelection);
	m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_table->verticalHeader()->hide();
	m_table->horizontalHeader()->setStretchLastSection(true);
	layout->addWidget(m_table, 1);

	auto *bottom = new QHBoxLayout();
	auto *manager = new QPushButton(tr("Parts manager..."), this);
	manager->setToolTip(tr("Add or change parts; this window shows them when"
						   " you come back to it."));
	bottom->addWidget(manager);
	bottom->addStretch();
	m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok
									 | QDialogButtonBox::Cancel, this);
	bottom->addWidget(m_buttons);
	layout->addLayout(bottom);

	connect(m_search, &QLineEdit::textChanged, this, &PartPickerDialog::refresh);
	connect(m_category, qOverload<int>(&QComboBox::currentIndexChanged),
			this, &PartPickerDialog::refresh);
	connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
		m_buttons->button(QDialogButtonBox::Ok)->setEnabled(selectedId() != 0);
	});
	connect(m_table, &QTableWidget::cellDoubleClicked, this, &QDialog::accept);
	connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
		//A child of this modal window, which would block any other
	connect(manager, &QPushButton::clicked, this, [this]() {
		PartsManagerDialog::showManager(this);
	});

	const QString category = QSettings().value(
				QStringLiteral("parts-database/picker-category")).toString();
	refresh();
	const int index = m_category->findData(category);
	if (index > 0) {
		m_category->setCurrentIndex(index);
	}
	m_search->setFocus();
}

/**
	@brief PartPickerDialog::changeEvent
	Coming back from the parts manager shows what was added there.
*/
void PartPickerDialog::changeEvent(QEvent *event)
{
	QDialog::changeEvent(event);
	if (event->type() == QEvent::ActivationChange && isActiveWindow()) {
		refresh();
	}
}

/**
	@brief PartPickerDialog::refresh
	Fill the categories and the table again, keeping the choices made.
*/
void PartPickerDialog::refresh()
{
	if (!m_db || !m_db->isOpen()) {
		return;
	}
	const qint64 selected = selectedId();
	const QString category = m_category->currentData().toString();
	{
		const QSignalBlocker blocker(m_category);
		m_category->clear();
		m_category->addItem(tr("All"), QString());
		for (const QString &name : m_db->categories()) {
			m_category->addItem(name, name);
		}
		const int index = m_category->findData(category);
		m_category->setCurrentIndex(index > 0 ? index : 0);
	}
	QSettings().setValue(QStringLiteral("parts-database/picker-category"),
						 m_category->currentData().toString());

	const QString filter = m_category->currentData().toString();
	const QSignalBlocker blocker(m_table);
	m_table->setRowCount(0);
	for (const Part &part : m_db->parts(m_search->text())) {
		if (!filter.isEmpty() && part.category != filter) {
			continue;
		}
		const int row = m_table->rowCount();
		m_table->insertRow(row);
		const QStringList cells = {part.manufacturer, part.order_number,
								   part.description, part.designation,
								   part.category, part.internal_reference};
		for (int column = 0; column < cells.size(); ++column) {
			auto *item = new QTableWidgetItem(cells.at(column));
			item->setData(id_role, part.id);
			m_table->setItem(row, column, item);
		}
		if (part.id == selected) {
			m_table->selectRow(row);
		}
	}
	m_table->resizeColumnsToContents();
	m_buttons->button(QDialogButtonBox::Ok)->setEnabled(selectedId() != 0);
}

qint64 PartPickerDialog::selectedId() const
{
	const QList<QTableWidgetItem *> items = m_table->selectedItems();
	return items.isEmpty() ? 0 : items.first()->data(id_role).toLongLong();
}

std::optional<Part> PartPickerDialog::selectedPart() const
{
	const qint64 id = selectedId();
	if (!id || !m_db) {
		return std::nullopt;
	}
	return m_db->part(id);
}
