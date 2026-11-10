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
#include "elementpartswidget.h"

#include "partpickerdialog.h"
#include "partsdatabase.h"
#include "partsmaterial.h"
#include "../qetgraphicsitem/element.h"
#include "../ui/elementinfowidget.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

enum Column {
	ManufacturerColumn,
	OrderColumn,
	DescriptionColumn,
	UnitsColumn,
	ColumnCount
};

	///The blocks of the element information : the main article and the
	///auxiliary articles 1 to 4
const int block_count = 5;

} // namespace

ElementPartsWidget::ElementPartsWidget(Element *element,
									   ElementInfoWidget *info,
									   QWidget *parent) :
	AbstractElementPropertiesEditorWidget(parent),
	m_info(info)
{
	auto *layout = new QVBoxLayout(this);
	m_hint = new QLabel(this);
	m_hint->setWordWrap(true);
	layout->addWidget(m_hint);

	m_table = new QTableWidget(0, ColumnCount, this);
	m_table->setHorizontalHeaderLabels({
		QObject::tr("Manufacturer"), QObject::tr("Order number"),
		QObject::tr("Textual description"), tr("Units")});
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setSelectionMode(QAbstractItemView::SingleSelection);
	m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_table->verticalHeader()->hide();
	m_table->horizontalHeader()->setSectionResizeMode(
				DescriptionColumn, QHeaderView::Stretch);
	layout->addWidget(m_table, 1);

	auto *buttons = new QHBoxLayout();
	m_add = new QPushButton(tr("Add a part..."), this);
	m_remove = new QPushButton(tr("Remove"), this);
	buttons->addWidget(m_add);
	buttons->addWidget(m_remove);
	buttons->addStretch();
	layout->addLayout(buttons);

	connect(m_add, &QPushButton::clicked, this, &ElementPartsWidget::addPart);
	connect(m_remove, &QPushButton::clicked,
			this, &ElementPartsWidget::removePart);
	connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
		m_remove->setEnabled(!m_table->selectedItems().isEmpty());
	});
	if (m_info) {
		connect(m_info, &ElementInfoWidget::partsValueChanged,
				this, &ElementPartsWidget::updateUi);
	}
	setElement(element);
}

void ElementPartsWidget::setElement(Element *element)
{
	m_element = element;
	updateUi();
}

/**
	@brief ElementPartsWidget::updateUi
	Show the parts of the element, described from the parts database when
	it is used.
*/
void ElementPartsWidget::updateUi()
{
	if (m_filling) {
		return;
	}
	const bool database = PartsDatabase::isEnabled();
	m_hint->setText(database
					? tr("The parts of the parts database this element is"
						 " made of, and how many units of each.")
					: tr("Use the parts database (Settings > General) to"
						 " choose the parts of this element."));
	m_add->setEnabled(database && m_info);

	PartsDatabase db;
	const bool open = database && db.open(PartsDatabase::path());

	m_filling = true;
	m_table->setRowCount(0);
	const QList<PartsLinks::Link> list = m_info
			? PartsLinks::decode(m_info->partsValue())
			: QList<PartsLinks::Link>();
	for (const PartsLinks::Link &link : list) {
		const int row = m_table->rowCount();
		m_table->insertRow(row);
		m_table->setItem(row, ManufacturerColumn,
						 new QTableWidgetItem(link.manufacturer));
		m_table->setItem(row, OrderColumn,
						 new QTableWidgetItem(link.order_number));
		QString description;
		if (open) {
			if (const std::optional<Part> part = db.part(
						db.findPart(link.manufacturer, link.order_number))) {
				description = part->description;
			} else {
				description = tr("(not in the parts database)");
			}
		}
		m_table->setItem(row, DescriptionColumn,
						 new QTableWidgetItem(description));

		auto *units = new QDoubleSpinBox(m_table);
		units->setRange(0.001, 1e6);
		units->setDecimals(3);
		units->setValue(link.units);
		connect(units, qOverload<double>(&QDoubleSpinBox::valueChanged),
				this, &ElementPartsWidget::store);
		m_table->setCellWidget(row, UnitsColumn, units);
	}
	m_table->resizeColumnToContents(ManufacturerColumn);
	m_table->resizeColumnToContents(OrderColumn);
	m_remove->setEnabled(!m_table->selectedItems().isEmpty());
	m_filling = false;
}

QList<PartsLinks::Link> ElementPartsWidget::links() const
{
	QList<PartsLinks::Link> list;
	for (int row = 0; row < m_table->rowCount(); ++row) {
		PartsLinks::Link link;
		link.manufacturer = m_table->item(row, ManufacturerColumn)->text();
		link.order_number = m_table->item(row, OrderColumn)->text();
		if (auto *units = qobject_cast<QDoubleSpinBox *>(
					m_table->cellWidget(row, UnitsColumn))) {
			link.units = units->value();
		}
		list << link;
	}
	return list;
}

/**
	@brief ElementPartsWidget::store
	Give the list of the table to the Information tab, which saves it.
*/
void ElementPartsWidget::store()
{
	if (m_filling || !m_info) {
		return;
	}
	m_filling = true;
	m_info->setPartsValue(PartsLinks::encode(links()));
	m_filling = false;
}

/**
	@brief ElementPartsWidget::addPart
	Choose a part in the parts database, add it with one unit, and offer
	to write its data into the element information: the first part into
	the main article, the next ones into the auxiliary articles.
*/
void ElementPartsWidget::addPart()
{
	if (!m_info) {
		return;
	}
	PartsDatabase db;
	QString error;
	if (!db.open(PartsDatabase::path(), &error)) {
		QMessageBox::warning(this, tr("Parts database"), error);
		return;
	}
	PartPickerDialog dialog(&db, this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}
	const std::optional<Part> part = dialog.selectedPart();
	if (!part) {
		return;
	}

	QList<PartsLinks::Link> list = links();
	PartsLinks::Link link;
	link.manufacturer = part->manufacturer;
	link.order_number = part->order_number;
	link.units = 1;
	const int block = list.size();
	list << link;
	m_info->setPartsValue(PartsLinks::encode(list));

	if (block >= block_count) {
		QMessageBox::information(
					this, tr("Parts"),
					tr("The part is added. The element information holds"
					   " the main article and four auxiliary ones: this part"
					   " has no block of its own there."));
		return;
	}
	const QString where = block == 0
			? tr("the main article")
			: tr("the auxiliary article %1").arg(block);
	if (QMessageBox::question(
				this, tr("Parts"),
				tr("Overwrite %1 of the element information with the data of"
				   " this part (manufacturer, order number, description...)?")
				.arg(where),
				QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
			== QMessageBox::Yes) {
		m_info->writeMaterialRecord(PartsMaterial::record(*part, link.units),
									block);
	}
}

void ElementPartsWidget::removePart()
{
	const QList<QTableWidgetItem *> items = m_table->selectedItems();
	if (items.isEmpty()) {
		return;
	}
	m_table->removeRow(items.first()->row());
	store();
}
