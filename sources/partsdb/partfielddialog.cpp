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
#include "partfielddialog.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace
{
	enum Column {
		NameColumn,
		ValueColumn,
		PartColumn,
		ColumnCount
	};

	QString partOf(const PartTexts::Field &field)
	{
		return field.manufacturer.isEmpty()
				? field.order_number
				: field.manufacturer + QStringLiteral(" ") + field.order_number;
	}
}

PartFieldDialog::PartFieldDialog(const QList<PartTexts::Field> &fields,
								 const QString &current_reference,
								 QWidget *parent) :
	QDialog(parent),
	m_fields(fields)
{
	setWindowTitle(tr("Data of the parts"));
	resize(960, 640);

	auto *layout = new QVBoxLayout(this);

	m_search = new QLineEdit(this);
	m_search->setObjectName(QStringLiteral("m_search"));
	m_search->setClearButtonEnabled(true);
	m_search->setPlaceholderText(tr("Search a piece of data, a value or a part..."));
	QFont big = m_search->font();
	if (big.pointSizeF() > 0) {
		big.setPointSizeF(big.pointSizeF() * 1.4);
	} else if (big.pixelSize() > 0) {
		big.setPixelSize(qRound(big.pixelSize() * 1.4));
	}
	m_search->setFont(big);
	m_search->setMinimumHeight(m_search->sizeHint().height() + 10);
	layout->addWidget(m_search);

	m_count = new QLabel(this);
	layout->addWidget(m_count);

	m_table = new QTableWidget(0, ColumnCount, this);
	m_table->setObjectName(QStringLiteral("m_table"));
	m_table->setHorizontalHeaderLabels({tr("Data"), tr("Value"), tr("Part")});
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setSelectionMode(QAbstractItemView::SingleSelection);
	m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_table->setAlternatingRowColors(true);
	m_table->verticalHeader()->hide();
	m_table->horizontalHeader()->setSectionResizeMode(ValueColumn, QHeaderView::Stretch);
	layout->addWidget(m_table, 1);

	m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	layout->addWidget(m_buttons);
	connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	int current_row = -1;
	for (int i = 0 ; i < m_fields.size() ; ++i)
	{
		const PartTexts::Field &field = m_fields.at(i);
		m_table->insertRow(i);
		auto *name = new QTableWidgetItem(field.name);
		name->setData(Qt::UserRole, i);
		m_table->setItem(i, NameColumn, name);
		m_table->setItem(i, ValueColumn, new QTableWidgetItem(field.value));
		m_table->setItem(i, PartColumn, new QTableWidgetItem(partOf(field)));
		if (field.reference() == current_reference) {
			current_row = i;
		}
	}
	m_table->resizeColumnToContents(NameColumn);
	m_table->resizeColumnToContents(PartColumn);

	connect(m_search, &QLineEdit::textChanged, this, &PartFieldDialog::filter);
	connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
		m_buttons->button(QDialogButtonBox::Ok)->setEnabled(hasSelection());
	});
	connect(m_table, &QTableWidget::itemDoubleClicked, this, [this]() {
		if (hasSelection()) {
			accept();
		}
	});
		//Enter in the search bar takes the row chosen, or the only one left
	connect(m_search, &QLineEdit::returnPressed, this, [this]() {
		if (!hasSelection()) {
			for (int row = 0 ; row < m_table->rowCount() ; ++row) {
				if (!m_table->isRowHidden(row)) {
					m_table->selectRow(row);
					break;
				}
			}
		}
		if (hasSelection()) {
			accept();
		}
	});

	if (current_row >= 0) {
		m_table->selectRow(current_row);
	}
	m_buttons->button(QDialogButtonBox::Ok)->setEnabled(hasSelection());
	filter();
	m_search->setFocus();
}

bool PartFieldDialog::hasSelection() const
{
	return selectedIndex() >= 0;
}

PartTexts::Field PartFieldDialog::selectedField() const
{
	const int index = selectedIndex();
	return index >= 0 ? m_fields.at(index) : PartTexts::Field();
}

/**
	@brief PartFieldDialog::filter
	Show the rows holding every word of the search, in their name, their
	value or their part, whatever the case
*/
void PartFieldDialog::filter()
{
	const QStringList words = m_search->text().split(QLatin1Char(' '), Qt::SkipEmptyParts);
	int shown = 0;
	for (int row = 0 ; row < m_table->rowCount() ; ++row)
	{
		QString line;
		for (int column = 0 ; column < ColumnCount ; ++column) {
			line += m_table->item(row, column)->text() + QLatin1Char(' ');
		}
		bool match = true;
		for (const QString &word : words) {
			if (!line.contains(word, Qt::CaseInsensitive)) {
				match = false;
				break;
			}
		}
		m_table->setRowHidden(row, !match);
		if (match) {
			++shown;
		}
	}
	m_count->setText(tr("%n piece(s) of data", "", shown));
	m_buttons->button(QDialogButtonBox::Ok)->setEnabled(hasSelection());
}

int PartFieldDialog::selectedIndex() const
{
	const QList<QTableWidgetItem *> items = m_table->selectedItems();
	for (QTableWidgetItem *item : items) {
		if (item->column() == NameColumn && !m_table->isRowHidden(item->row())) {
			return item->data(Qt::UserRole).toInt();
		}
	}
	return -1;
}
