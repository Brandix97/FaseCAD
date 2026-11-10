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
#include "devicespanel.h"

#include "../dataBase/projectdatabase.h"
#include "../diagram.h"
#include "../diagramcontent.h"
#include "../undocommand/deleteqgraphicsitemcommand.h"
#include "../qetgraphicsitem/element.h"
#include "../qetproject.h"

#include <QCollator>
#include <QGraphicsView>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QSet>
#include <QShortcut>
#include <QSqlQuery>
#include <QTimer>
#include <QTreeWidget>
#include <QUndoStack>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>

namespace {

const int uuid_role = Qt::UserRole;
const int diagram_role = Qt::UserRole + 1;

} // namespace

DevicesPanel::DevicesPanel(QWidget *parent) :
	QWidget(parent)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	m_search = new QLineEdit(this);
	m_search->setPlaceholderText(tr("Search a device"));
	m_search->setClearButtonEnabled(true);
	m_tree = new QTreeWidget(this);
	m_tree->setColumnCount(3);
	m_tree->setHeaderLabels({tr("Label"), tr("Description"), tr("Sheet")});
	m_tree->setRootIsDecorated(true);
	m_tree->setUniformRowHeights(true);
	m_tree->header()->setStretchLastSection(false);
	m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
	m_tree->setToolTip(tr("Double-click a device to show it on its sheet."
						  " Shift and Ctrl select several, Delete removes"
						  " them."));
	m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_tree, &QWidget::customContextMenuRequested,
			this, &DevicesPanel::showContextMenu);
	auto *delete_shortcut = new QShortcut(QKeySequence::Delete, m_tree);
	delete_shortcut->setContext(Qt::WidgetShortcut);
	connect(delete_shortcut, &QShortcut::activated,
			this, &DevicesPanel::deleteSelected);
	layout->addWidget(m_search);
	layout->addWidget(m_tree, 1);

	m_refresh_timer = new QTimer(this);
	m_refresh_timer->setSingleShot(true);
	m_refresh_timer->setInterval(200);
	connect(m_refresh_timer, &QTimer::timeout, this, &DevicesPanel::refresh);
	connect(m_search, &QLineEdit::textChanged,
			this, &DevicesPanel::scheduleRefresh);
	connect(m_tree, &QTreeWidget::itemDoubleClicked,
			this, [this](QTreeWidgetItem *item) {showDevice(item);});
}

/**
	@brief DevicesPanel::setProject
	List the devices of @p project, and follow its changes; nullptr empties
	the list.
*/
void DevicesPanel::setProject(QETProject *project)
{
	if (project == m_project) {
		return;
	}
	disconnect(m_database_connection);
	m_project = project;
	if (m_project) {
			//Every symbol added, removed or renamed goes through the
			//project database
		m_database_connection = connect(m_project->dataBase(),
										&projectDataBase::dataBaseUpdated,
										this, &DevicesPanel::scheduleRefresh);
	}
	m_stale = true;
	scheduleRefresh();
}

/**
	@brief DevicesPanel::scheduleRefresh
	Read the list again soon, or, while the panel is hidden behind another
	tab, once it shows: nothing is read for a list nobody looks at.
*/
void DevicesPanel::scheduleRefresh()
{
	m_stale = true;
	if (isVisible()) {
		m_refresh_timer->start();
	}
}

void DevicesPanel::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	if (m_stale) {
		m_refresh_timer->start();
	}
}

/**
	@brief DevicesPanel::labelLessThan
	@return true when @p a sorts before @p b: case ignored, and the numbers
	inside compared by their value, so -K2 comes before -K10.
*/
bool DevicesPanel::labelLessThan(const QString &a, const QString &b)
{
	static QCollator collator = []() {
		QCollator c;
		c.setNumericMode(true);
		c.setCaseSensitivity(Qt::CaseInsensitive);
		return c;
	}();
	return collator.compare(a, b) < 0;
}

/**
	@brief DevicesPanel::devices
	@return the symbols of @p project the panel lists, read from the
	project database: every symbol but the folio reports, the slave
	contacts, the cabinet thumbnails and the wire definitions, sorted by
	label, then by sheet, then by cell. Symbols
	without a label come last.
*/
QList<DevicesPanel::Device> DevicesPanel::devices(QETProject *project)
{
	QList<Device> result;
	if (!project) {
		return result;
	}
	QSqlQuery query = project->dataBase()->newQuery(QStringLiteral(
		"SELECT e.uuid, e.diagram_uuid, e.pos, d.pos, ei.label, "
		"ei.description, ei.comment "
		"FROM element e "
		"JOIN diagram d ON d.uuid = e.diagram_uuid "
		"LEFT JOIN element_info ei ON ei.element_uuid = e.uuid "
			//Written by ElementData::typeToString(): reports, slave
			//contacts, cabinet thumbnails and wire definitions are no
			//devices of their own
		"WHERE e.type NOT IN ('slave', 'next_report', 'previous_report', "
		"'thumbnail', 'conductor_definition')"));
	if (!query.exec()) {
		return result;
	}
	while (query.next()) {
		Device device;
		device.uuid = query.value(0).toString();
		device.diagram_uuid = query.value(1).toString();
		device.cell = query.value(2).toString();
			//The diagram table counts the sheets from 1
		device.folio = query.value(3).toInt();
		device.label = query.value(4).toString().trimmed();
		device.description = query.value(5).toString().trimmed();
		if (device.description.isEmpty()) {
			device.description = query.value(6).toString().trimmed();
		}
		result.append(device);
	}
	std::stable_sort(result.begin(), result.end(),
					 [](const Device &a, const Device &b) {
		if (a.label.isEmpty() != b.label.isEmpty()) {
			return b.label.isEmpty();
		}
		if (a.label != b.label) {
			return labelLessThan(a.label, b.label);
		}
		if (a.folio != b.folio) {
			return a.folio < b.folio;
		}
		return labelLessThan(a.cell, b.cell);
	});
	return result;
}

/**
	@brief DevicesPanel::refresh
	Fill the tree again, keeping the labels the user opened.
*/
void DevicesPanel::refresh()
{
	m_stale = false;
	QStringList expanded;
	for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
		if (m_tree->topLevelItem(i)->isExpanded()) {
			expanded << m_tree->topLevelItem(i)->text(0);
		}
	}

	m_tree->clear();
	const QString search = m_search->text().trimmed();
	const QList<Device> list = devices(m_project);
	QTreeWidgetItem *group = nullptr;
	QList<Device> members;

	auto flush = [&]() {
		if (members.isEmpty()) {
			return;
		}
		const Device &first = members.first();
		const QString label = first.label.isEmpty() ? tr("(No label)")
													: first.label;
		QStringList sheets;
		for (const Device &d : std::as_const(members)) {
			const QString sheet = QString::number(d.folio);
			if (!sheets.contains(sheet)) {
				sheets << sheet;
			}
		}
		group = new QTreeWidgetItem(m_tree, {label, first.description,
											 members.size() == 1
											 ? QStringLiteral("%1-%2").arg(first.folio).arg(first.cell)
											 : sheets.join(QStringLiteral(", "))});
		group->setData(0, uuid_role, first.uuid);
		group->setData(0, diagram_role, first.diagram_uuid);
		if (members.size() > 1) {
			group->setText(0, QStringLiteral("%1 (%2)").arg(label).arg(members.size()));
			for (const Device &d : std::as_const(members)) {
				auto *child = new QTreeWidgetItem(group, {
					label, d.description,
					QStringLiteral("%1-%2").arg(d.folio).arg(d.cell)});
				child->setData(0, uuid_role, d.uuid);
				child->setData(0, diagram_role, d.diagram_uuid);
			}
			group->setExpanded(expanded.contains(group->text(0)));
		}
		members.clear();
	};

	for (const Device &device : list) {
		if (!search.isEmpty()
				&& !device.label.contains(search, Qt::CaseInsensitive)
				&& !device.description.contains(search, Qt::CaseInsensitive)) {
			continue;
		}
		if (!members.isEmpty() && members.first().label != device.label) {
			flush();
		}
		members << device;
	}
	flush();
	m_tree->resizeColumnToContents(0);
	m_tree->resizeColumnToContents(2);
}

/**
	@brief DevicesPanel::showContextMenu
	Show or delete the devices under the mouse.
*/
void DevicesPanel::showContextMenu(const QPoint &pos)
{
	QTreeWidgetItem *item = m_tree->itemAt(pos);
	if (!item) {
		return;
	}
	if (!item->isSelected()) {
		m_tree->clearSelection();
		item->setSelected(true);
	}
	QMenu menu(this);
	QAction *show = menu.addAction(tr("Show on the sheet"));
	show->setEnabled(m_tree->selectedItems().size() == 1);
	connect(show, &QAction::triggered, this, [this, item]() {showDevice(item);});
	menu.addSeparator();
	QAction *remove = menu.addAction(tr("Delete"));
	remove->setShortcut(QKeySequence::Delete);
	connect(remove, &QAction::triggered, this, &DevicesPanel::deleteSelected);
	menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

/**
	@brief DevicesPanel::deleteSelected
	Delete the selected devices, on every sheet they stand on, as one
	step of undo. A line holding several symbols (the terminals of a
	strip) stands for all of them. Each sheet goes through the command
	the sheet itself uses, so the wires of the symbols go with them, and
	a bridged or multi-level terminal is refused the same way.
*/
void DevicesPanel::deleteSelected()
{
	if (!m_project) {
		return;
	}
	QHash<QString, QSet<QString>> by_diagram;
	int count = 0;
	auto take = [&](QTreeWidgetItem *item) {
		const QString uuid = item->data(0, uuid_role).toString();
		QSet<QString> &uuids = by_diagram[item->data(0, diagram_role).toString()];
		if (!uuids.contains(uuid)) {
			uuids.insert(uuid);
			++count;
		}
	};
	for (QTreeWidgetItem *item : m_tree->selectedItems()) {
		if (item->childCount()) {
			for (int i = 0; i < item->childCount(); ++i) {
				take(item->child(i));
			}
		} else {
			take(item);
		}
	}
	if (!count) {
		return;
	}
	if (QMessageBox::question(this, tr("Delete devices"),
							  tr("Delete %n device(s), with their wires?",
								 nullptr, count))
			!= QMessageBox::Yes) {
		return;
	}

	QUndoStack *stack = m_project->undoStack();
	stack->beginMacro(tr("Delete %n device(s)", nullptr, count));
	int refused = 0;
	for (auto it = by_diagram.cbegin(); it != by_diagram.cend(); ++it) {
		Diagram *diagram = m_project->diagramByUuid(QUuid(it.key()));
		if (!diagram) {
			continue;
		}
		diagram->clearSelection();
		for (Element *element : diagram->elements()) {
			if (it.value().contains(element->uuid().toString())) {
				element->setSelected(true);
			}
		}
		DiagramContent content(diagram);
		diagram->clearSelection();
		if (DeleteQGraphicsItemCommand::hasNonDeletableTerminal(content)) {
			++refused;
			continue;
		}
		stack->push(new DeleteQGraphicsItemCommand(diagram, content));
	}
	stack->endMacro();

	if (refused) {
		QMessageBox::information(
					this, tr("Delete devices"),
					tr("Some terminals were not deleted: they are bridged or"
					   " belong to a multi-level terminal. Unbridge them, or"
					   " remove their levels, first."));
	}
	scheduleRefresh();
}

/**
	@brief DevicesPanel::showDevice
	Show the sheet of the symbol of @p item, with the symbol selected in
	the middle of the view.
*/
void DevicesPanel::showDevice(QTreeWidgetItem *item)
{
	if (!item || !m_project) {
		return;
	}
	Diagram *diagram = m_project->diagramByUuid(
				QUuid(item->data(0, diagram_role).toString()));
	if (!diagram) {
		return;
	}
	diagram->showMe();
	const QUuid uuid(item->data(0, uuid_role).toString());
	for (Element *element : diagram->elements()) {
		if (element->uuid() == uuid) {
			diagram->clearSelection();
			element->setSelected(true);
			for (QGraphicsView *view : diagram->views()) {
				view->centerOn(element);
			}
			return;
		}
	}
}
