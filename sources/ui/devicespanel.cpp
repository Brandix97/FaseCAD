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

#include "../cabinet/cabinetdevices.h"
#include "../cabinet/cabinetdialogs.h"
#include "../dataBase/projectdatabase.h"
#include "../diagram.h"
#include "../diagramcontent.h"
#include "../diagramevent/diagrameventaddcabinetitem.h"
#include "../partsdb/partslinks.h"
#include "../qetinformation.h"
#include "../undocommand/changeelementinformationcommand.h"
#include "../undocommand/deleteqgraphicsitemcommand.h"
#include "../qetgraphicsitem/element.h"
#include "../qetproject.h"

#include <QCollator>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QShortcut>
#include <QSpinBox>
#include <QSqlQuery>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QUndoStack>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>

namespace {

const int uuid_role = Qt::UserRole;
const int diagram_role = Qt::UserRole + 1;
	/// The label of the device, as the symbols carry it
const int label_role = Qt::UserRole + 2;
	/// The symbols of a device's line, and their folios (same order)
const int symbols_role = Qt::UserRole + 3;
const int symbol_folios_role = Qt::UserRole + 4;
	/// Drawn on a cabinet sheet already (the green tick)
const int placed_role = Qt::UserRole + 5;
	/// Where the symbol a line stands for is: its sheet (from 1), its cell
const int folio_role = Qt::UserRole + 6;
const int cell_role = Qt::UserRole + 7;

	/// The symbols a line stands for, as (uuid, folio uuid): those of a
	/// device, the ones it opens on, or its own
QList<QPair<QString, QString>> symbolsOf(QTreeWidgetItem *item)
{
	QList<QPair<QString, QString>> symbols;
	const QStringList uuids = item->data(0, symbols_role).toStringList();
	const QStringList folios = item->data(0, symbol_folios_role).toStringList();
	if (!uuids.isEmpty() && uuids.size() == folios.size()) {
		for (int i = 0; i < uuids.size(); ++i) {
			symbols.append({uuids.at(i), folios.at(i)});
		}
	} else if (item->childCount()) {
		for (int i = 0; i < item->childCount(); ++i) {
			symbols.append({item->child(i)->data(0, uuid_role).toString(),
							item->child(i)->data(0, diagram_role).toString()});
		}
	} else {
		symbols.append({item->data(0, uuid_role).toString(),
						item->data(0, diagram_role).toString()});
	}
	return symbols;
}

	/// The order numbers of @p parts (PartsLinks), each once, added to @p numbers
void addArticles(const QString &parts, QStringList &numbers)
{
	for (const PartsLinks::Link &link : PartsLinks::decode(parts)) {
		QString number = link.order_number.trimmed();
		if (number.isEmpty()) {
			number = link.manufacturer.trimmed();
		}
		if (!number.isEmpty() && !numbers.contains(number)) {
			numbers << number;
		}
	}
}

	/// "ACME PLC-24DC", one part a line, for a tooltip
QString partsText(const QList<DevicesPanel::Device> &symbols)
{
	QStringList lines;
	for (const DevicesPanel::Device &symbol : symbols) {
		for (const PartsLinks::Link &link : PartsLinks::decode(symbol.parts)) {
			QString line = QStringLiteral("%1 %2").arg(link.manufacturer.trimmed(),
													  link.order_number.trimmed()).trimmed();
			if (link.units != 1) {
				line += QStringLiteral(" × %1").arg(link.units);
			}
			if (!line.isEmpty() && !lines.contains(line)) {
				lines << line;
			}
		}
	}
	return lines.join(QLatin1Char('\n'));
}

	/// The symbol of @p folio whose uuid is @p uuid, nullptr if none
Element *elementOf(Diagram *folio, const QUuid &uuid)
{
	if (!folio) return nullptr;
	for (Element *element : folio->elements()) {
		if (element->uuid() == uuid) return element;
	}
	return nullptr;
}

	/// "3 - Cabinet construction": a folio as the project panel names it
QString folioName(QETProject *project, Diagram *folio)
{
	const QString number = QString::number(project->folioIndex(folio) + 1);
	const QString title = folio->title().trimmed();
	return title.isEmpty() ? number : QStringLiteral("%1 - %2").arg(number, title);
}

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
	m_tree->setColumnCount(4);
	m_tree->setHeaderLabels({tr("Label"), tr("Description"), tr("Article"), tr("Sheet")});
	m_tree->setRootIsDecorated(false);
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
	m_number_button = new QToolButton(this);
	m_number_button->setText(tr("Number..."));
	m_number_button->setToolTip(tr("Rename the selected devices one after the"
									" other from a first label: -K1, -K2..."));
	connect(m_number_button, &QToolButton::clicked, this, &DevicesPanel::numberSelected);
	auto *bar = new QHBoxLayout();
	bar->setContentsMargins(0, 0, 0, 0);
	bar->addWidget(m_search, 1);
	bar->addWidget(m_number_button);
	layout->addLayout(bar);
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
	disconnect(m_undo_connection);
	m_project = project;
	if (m_project) {
			//Every symbol added, removed or renamed goes through the
			//project database
		m_database_connection = connect(m_project->dataBase(),
										&projectDataBase::dataBaseUpdated,
										this, &DevicesPanel::scheduleRefresh);
			//A drawing placed on a cabinet sheet or deleted does not: it
			//goes through the undo stack, as its undo and redo do
		m_undo_connection = connect(m_project->undoStack(), &QUndoStack::indexChanged,
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
			//Not a column of element_info: from the information the
			//database keeps for every placed symbol, built or not
		device.parts = project->dataBase()->elementInformation(QUuid(device.uuid))
				.value(PartsLinks::infoKey()).toString();
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
	@brief DevicesPanel::lines
	@return @p devices, sorted as devices() sorts them, as the lines of the
	list: one per label, with the symbols carrying it; the symbols without
	a label together, last. A line stands for its first symbol made of
	parts (the one the parts list counts), else its first symbol; it says
	the articles of all its symbols, each once.
*/
QList<DevicesPanel::Line> DevicesPanel::lines(const QList<Device> &devices)
{
	QList<Line> result;
	for (const Device &device : devices) {
		if (result.isEmpty() || result.last().label != device.label) {
			result.append(Line());
			result.last().label = device.label;
		}
		result.last().symbols << device;
	}
	for (Line &line : result) {
		for (int i = 0; i < line.symbols.size(); ++i) {
			if (!PartsLinks::decode(line.symbols.at(i).parts).isEmpty()) {
				line.main = i;
				break;
			}
		}
		line.description = line.symbols.at(line.main).description;
		QStringList numbers;
		QStringList sheets;
		for (const Device &symbol : std::as_const(line.symbols)) {
			if (line.description.isEmpty()) {
				line.description = symbol.description;
			}
			addArticles(symbol.parts, numbers);
			const QString sheet = QString::number(symbol.folio);
			if (!sheets.contains(sheet)) {
				sheets << sheet;
			}
		}
		line.article = numbers.join(QStringLiteral(", "));
		const Device &first = line.symbols.first();
		line.sheets = line.symbols.size() == 1
				? QStringLiteral("%1-%2").arg(first.folio).arg(first.cell)
				: sheets.join(QStringLiteral(", "));
	}
	return result;
}

/**
	@brief DevicesPanel::refresh
	Fill the tree again, keeping the line of the symbols without a label
	open if the user opened it.
*/
void DevicesPanel::refresh()
{
	m_stale = false;
	bool unlabelled_open = false;
	for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
		QTreeWidgetItem *line = m_tree->topLevelItem(i);
		if (line->childCount() && line->isExpanded()) {
			unlabelled_open = true;
		}
	}

	m_tree->clear();
	const QString search = m_search->text().trimmed();
	auto found = [&search](const QString &text) {
		return text.contains(search, Qt::CaseInsensitive);
	};

		//The devices drawn on a cabinet sheet: by their symbol, and by
		//their label for a drawing whose symbol is gone
	QHash<QString, QList<int>> placed_symbols;
	QHash<QString, QList<int>> placed_labels;
	for (const CabinetDevices::Placed &placed : CabinetDevices::placed(m_project)) {
		if (!placed.device.isNull())
			placed_symbols[placed.device.toString()] << placed.folio;
		if (!placed.label.isEmpty())
			placed_labels[placed.label] << placed.folio;
	}
		//With a cabinet sheet in the project every line keeps the room
		//of the tick, so the labels stay lined up
	const bool cabinet = !CabinetDevices::cabinetFolios(m_project).isEmpty();
	auto mark = [&](QTreeWidgetItem *line, QList<int> folios) {
		if (!cabinet && folios.isEmpty()) {
			return;
		}
		std::sort(folios.begin(), folios.end());
		folios.erase(std::unique(folios.begin(), folios.end()), folios.end());
		line->setIcon(0, CabinetDialogs::placedIcon(folios.isEmpty()));
		line->setData(0, placed_role, !folios.isEmpty());
		if (folios.isEmpty()) {
			return;
		}
		QStringList numbers;
		for (int folio : std::as_const(folios)) {
			numbers << QString::number(folio);
		}
		line->setToolTip(0, tr("Drawn on the cabinet sheet(s) %1", nullptr, folios.size())
						 .arg(numbers.join(QStringLiteral(", "))));
	};

	bool unlabelled_line = false;
	for (const Line &line : lines(devices(m_project))) {
		if (line.label.isEmpty()) {
				//No device: one line opening on each symbol, those the
				//search finds
			QList<Device> symbols;
			for (const Device &symbol : line.symbols) {
				QStringList numbers;
				addArticles(symbol.parts, numbers);
				if (search.isEmpty() || found(symbol.description)
						|| found(numbers.join(QStringLiteral(", ")))) {
					symbols << symbol;
				}
			}
			if (symbols.isEmpty()) {
				continue;
			}
			QStringList sheets;
			for (const Device &symbol : std::as_const(symbols)) {
				const QString sheet = QString::number(symbol.folio);
				if (!sheets.contains(sheet)) {
					sheets << sheet;
				}
			}
			auto *group = new QTreeWidgetItem(m_tree, {
				QStringLiteral("%1 (%2)").arg(tr("(No label)")).arg(symbols.size()),
				QString(), QString(), sheets.join(QStringLiteral(", "))});
			group->setData(0, uuid_role, symbols.first().uuid);
			group->setData(0, diagram_role, symbols.first().diagram_uuid);
			QList<int> group_folios;
			for (const Device &symbol : std::as_const(symbols)) {
				QStringList numbers;
				addArticles(symbol.parts, numbers);
				auto *child = new QTreeWidgetItem(group, {
					tr("(No label)"), symbol.description,
					numbers.join(QStringLiteral(", ")),
					QStringLiteral("%1-%2").arg(symbol.folio).arg(symbol.cell)});
				child->setData(0, uuid_role, symbol.uuid);
				child->setData(0, diagram_role, symbol.diagram_uuid);
				child->setData(0, folio_role, symbol.folio);
				child->setData(0, cell_role, symbol.cell);
				const QList<int> child_folios = placed_symbols.value(symbol.uuid);
				group_folios << child_folios;
				mark(child, child_folios);
			}
			mark(group, group_folios);
			group->setExpanded(unlabelled_open);
			unlabelled_line = true;
			continue;
		}

		if (!search.isEmpty() && !found(line.label) && !found(line.article)) {
			bool described = false;
			for (const Device &symbol : line.symbols) {
				described = described || found(symbol.description);
			}
			if (!described) {
				continue;
			}
		}
			//One line for the device, whatever the number of its symbols
		const Device &main = line.symbols.at(line.main);
		auto *item = new QTreeWidgetItem(m_tree, {line.label, line.description,
												  line.article, line.sheets});
		item->setData(0, uuid_role, main.uuid);
		item->setData(0, diagram_role, main.diagram_uuid);
		item->setData(0, label_role, line.label);
		item->setData(0, folio_role, main.folio);
		item->setData(0, cell_role, main.cell);
		QStringList uuids;
		QStringList folios;
		QList<int> placed_folios = placed_labels.value(line.label);
		for (const Device &symbol : line.symbols) {
			uuids << symbol.uuid;
			folios << symbol.diagram_uuid;
			placed_folios << placed_symbols.value(symbol.uuid);
		}
		item->setData(0, symbols_role, uuids);
		item->setData(0, symbol_folios_role, folios);
			//The whole of what is cut to the width of a narrow panel
		if (!line.description.isEmpty()) {
			item->setToolTip(1, line.description);
		}
		const QString parts = partsText(line.symbols);
		if (!parts.isEmpty()) {
			item->setToolTip(2, parts);
		}
		if (line.symbols.size() > 1) {
			item->setToolTip(3, tr("%n symbol(s) of this device", nullptr,
								   line.symbols.size()));
		}
		mark(item, placed_folios);
	}
		//The arrows only for the line of the symbols without a label
	m_tree->setRootIsDecorated(unlabelled_line);
	m_tree->resizeColumnToContents(0);
	m_tree->resizeColumnToContents(2);
	m_tree->resizeColumnToContents(3);
}

/**
	@brief DevicesPanel::showContextMenu
	Show, draw on a cabinet sheet, number or delete the devices under the
	mouse.
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
	addDrawingActions(menu);
	QAction *number = menu.addAction(tr("Number..."));
	number->setEnabled(m_project && !m_project->isReadOnly());
	connect(number, &QAction::triggered, this, &DevicesPanel::numberSelected);
	menu.addSeparator();
	QAction *remove = menu.addAction(tr("Delete"));
	remove->setShortcut(QKeySequence::Delete);
	connect(remove, &QAction::triggered, this, &DevicesPanel::deleteSelected);
	menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

/**
	@brief DevicesPanel::selectedLines
	@return the selected lines, each a device (or a symbol without a label),
	in the order of the list. The line of the symbols without a label
	stands for each of them.
*/
QList<QTreeWidgetItem *> DevicesPanel::selectedLines() const
{
	QList<QTreeWidgetItem *> result;
	for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
		QTreeWidgetItem *line = m_tree->topLevelItem(i);
		const bool whole = line->isSelected();
		for (int j = 0; j < line->childCount(); ++j) {
			if (whole || line->child(j)->isSelected()) {
				result << line->child(j);
			}
		}
		if (whole && !line->childCount()) {
			result << line;
		}
	}
	return result;
}

/**
	@brief DevicesPanel::addDrawingActions
	Add to @p menu the command drawing the selected devices on a cabinet
	sheet: a sub-menu naming each sheet when the project has several, one
	command otherwise (which offers to add a cabinet sheet if there is
	none).
*/
void DevicesPanel::addDrawingActions(QMenu &menu)
{
	const QString text = tr("Insert the DXF on a cabinet sheet");
	const bool enabled = m_project && !m_project->isReadOnly()
			&& !selectedLines().isEmpty();
	const QList<Diagram *> folios = CabinetDevices::cabinetFolios(m_project);
	if (folios.size() > 1) {
		QMenu *sheets = menu.addMenu(CabinetDialogs::dxfIcon(), text);
		sheets->setEnabled(enabled);
		for (Diagram *folio : folios) {
			QAction *action = sheets->addAction(CabinetDialogs::folioIcon(),
												folioName(m_project, folio));
			connect(action, &QAction::triggered, this,
					[this, folio]() {insertDrawings(folio);});
		}
		return;
	}
	QAction *action = menu.addAction(CabinetDialogs::dxfIcon(), text);
	action->setEnabled(enabled);
	Diagram *folio = folios.value(0);
	connect(action, &QAction::triggered, this,
			[this, folio]() {insertDrawings(folio);});
}

/**
	@brief DevicesPanel::insertDrawings
	Draw the selected devices on the cabinet sheet @p folio, one after the
	other in the order of the list: the drawing of the first one follows
	the mouse, a click puts it down and the next one comes
	(placeNextDrawing()). A device drawn already (its green tick) is left
	out. With no cabinet sheet (@p folio null), one is added at the end of
	the project if the user wants.
*/
void DevicesPanel::insertDrawings(Diagram *folio)
{
	if (!m_project || m_project->isReadOnly()) {
		return;
	}
	m_queue.clear();
	int drawn = 0;
	for (QTreeWidgetItem *line : selectedLines()) {
		if (line->data(0, placed_role).toBool()) {
			++drawn;
			continue;
		}
		m_queue.append({line->data(0, label_role).toString(), symbolsOf(line)});
	}
	if (m_queue.isEmpty()) {
		if (drawn) {
			QMessageBox::information(this, tr("Insert the DXF on a cabinet sheet"),
									 tr("The selected devices are on a cabinet"
										" sheet already."));
		}
		return;
	}

	if (!folio) {
		if (QMessageBox::question(this, tr("Insert the DXF on a cabinet sheet"),
								  tr("The project has no cabinet construction sheet."
									 " Add one at the end of the project?"))
				!= QMessageBox::Yes) {
			m_queue.clear();
			return;
		}
		folio = m_project->addNewCabinetDiagram();
		if (!folio) {
			m_queue.clear();
			return;
		}
	}
	m_queue_folio = folio;
	m_queue_total = m_queue.size();
	m_queue_position = 0;
	folio->ensureLoaded();
	folio->showMe();
	placeNextDrawing();
}

/**
	@brief DevicesPanel::placeNextDrawing
	Let the user place the drawing of the next device of the queue: it
	follows the mouse on the cabinet sheet, a click puts it down
	(DiagramEventAddCabinetItem, one step of undo each) and the next one
	comes; a right click skips it, Escape stops the queue. A device whose
	drawing cannot be had (no DXF, and no file chosen) is skipped.
*/
void DevicesPanel::placeNextDrawing()
{
	while (!m_queue.isEmpty()) {
		Diagram *folio = m_queue_folio;
		if (!folio || !m_project) {
			m_queue.clear();
			return;
		}
		const Pending device = m_queue.takeFirst();
		++m_queue_position;
		QUuid symbol;
		CabinetItem::Properties properties;
		if (!drawingOf(device, symbol, properties)) {
			continue;
		}
		properties.device = symbol;
		properties.device_label = device.label;

		auto *tool = new DiagramEventAddCabinetItem(folio, properties);
		if (m_queue_total > 1) {
			tool->setHint(tr("%1 (%2 of %3): click where it goes | Right click"
							 " to skip it | Esc to stop")
						  .arg(device.label.isEmpty() ? tr("(No label)") : device.label)
						  .arg(m_queue_position).arg(m_queue_total));
		}
			//Connected before the folio's own connection, which deletes
			//the tool: the next one is started once that is done
		connect(tool, &DiagramEventInterface::finish, this, [this, tool]() {
			if (tool->ending() == DiagramEventAddCabinetItem::Ending::Cancelled) {
				m_queue.clear();
				return;
			}
			QTimer::singleShot(0, this, &DevicesPanel::placeNextDrawing);
		});
		folio->setEventInterface(tool);
		for (QGraphicsView *view : folio->views()) {
			view->setFocus();
		}
		return;
	}
}

/**
	@brief DevicesPanel::drawingOf
	Read the drawing of @p device into @p properties: the first DXF of its
	parts in the parts database, or, if it has none, a DXF file the user
	chooses. @p symbol is told which of its symbols the drawing stands
	for: the first one made of parts, or the first one if none is.
	@return false if there is no drawing: the user chose no file
*/
bool DevicesPanel::drawingOf(const Pending &device, QUuid &symbol,
							 CabinetItem::Properties &properties)
{
		//The folios of its symbols built to read their parts
	QSet<QUuid> uuids;
	for (const auto &each : device.symbols) {
		uuids.insert(QUuid(each.first));
	}
	m_project->buildFoliosHoldingSymbols(uuids);

	symbol = QUuid();
	QString parts;
	for (const auto &each : device.symbols) {
		Element *element = elementOf(m_project->diagramByUuid(QUuid(each.second)),
									 QUuid(each.first));
		if (!element) {
			continue;
		}
		if (symbol.isNull()) {
			symbol = element->uuid();
		}
		const QString value = element->elementInformations()
				.value(PartsLinks::infoKey()).toString();
		if (!value.trimmed().isEmpty()) {
			symbol = element->uuid();
			parts = value;
			break;
		}
	}
	if (symbol.isNull()) {
		return false;
	}

	QString error;
	if (CabinetDevices::drawingFromParts(parts, properties, &error)) {
		return true;
	}
	if (!error.isEmpty()) {
		QMessageBox::warning(this, tr("Insert the DXF on a cabinet sheet"), error);
	}
	const QString name = device.label.isEmpty() ? tr("(No label)") : device.label;
	return CabinetDialogs::askDxfDrawing(
				properties, this,
				tr("%1 has no DXF in its parts: choose a DXF file").arg(name));
}

/**
	@brief DevicesPanel::numberedLabels
	@return @p count labels made from @p first: its last number goes up by
	@p step from one label to the next, keeping its zeros (-K08, -K09,
	-K10); a label without a number gets one, from 1 (-K -> -K1, -K2).
*/
QStringList DevicesPanel::numberedLabels(const QString &first, int count, int step)
{
	static const QRegularExpression last_number(QStringLiteral("^(.*?)(\\d+)(\\D*)$"));
	QString head = first;
	QString tail;
	qlonglong start = 1;
	int width = 1;
	const QRegularExpressionMatch match = last_number.match(first);
	if (match.hasMatch()) {
		head = match.captured(1);
		start = match.captured(2).toLongLong();
		width = match.captured(2).size();
		tail = match.captured(3);
	}
	QStringList labels;
	for (int i = 0; i < count; ++i) {
		const qlonglong value = start + qlonglong(i) * step;
		labels << head + QStringLiteral("%1").arg(value, width, 10, QLatin1Char('0')) + tail;
	}
	return labels;
}

/**
	@brief DevicesPanel::positionLessThan
	@return true if @p a stands before @p b on the sheets: on an earlier
	sheet, or in an earlier column of the same sheet, or higher in the
	same column. A cell is a row letter and a column number (C5).
*/
bool DevicesPanel::positionLessThan(const Device &a, const Device &b)
{
	if (a.folio != b.folio) {
		return a.folio < b.folio;
	}
	static const QRegularExpression cell(QStringLiteral("^([A-Za-z]*)(\\d*)$"));
	const QRegularExpressionMatch ma = cell.match(a.cell);
	const QRegularExpressionMatch mb = cell.match(b.cell);
	const int column_a = ma.captured(2).toInt();
	const int column_b = mb.captured(2).toInt();
	if (column_a != column_b) {
		return column_a < column_b;
	}
	return labelLessThan(ma.captured(1), mb.captured(1));
}

/**
	@brief DevicesPanel::numberSelected
	Rename the selected devices from a first label the user gives, the
	number going up by a step, in the order they stand on the sheets
	(positionLessThan(), each device by the symbol its line stands for).
	Every symbol of a device takes its new label; its contacts and its
	drawing on a cabinet sheet follow it. A new label another device has
	already is refused. One step of undo.
*/
void DevicesPanel::numberSelected()
{
	if (!m_project || m_project->isReadOnly()) {
		return;
	}
		//The devices, by where their main symbol stands
	struct Target {
		QString label;
		Device position;
		QList<QPair<QString, QString>> symbols;
	};
	QList<Target> targets;
	QSet<QString> selected_symbols;
	for (QTreeWidgetItem *line : selectedLines()) {
		Target target;
		target.label = line->data(0, label_role).toString();
		target.position.folio = line->data(0, folio_role).toInt();
		target.position.cell = line->data(0, cell_role).toString();
		target.symbols = symbolsOf(line);
		for (const auto &symbol : std::as_const(target.symbols)) {
			selected_symbols.insert(symbol.first);
		}
		targets << target;
	}
	if (targets.isEmpty()) {
		return;
	}
	std::stable_sort(targets.begin(), targets.end(), [](const Target &a, const Target &b) {
		return positionLessThan(a.position, b.position);
	});

		//The first label and the step, with what they give, line by line
	QDialog dialog(this);
	dialog.setWindowTitle(tr("Number devices"));
	auto *layout = new QVBoxLayout(&dialog);
	auto *form = new QFormLayout();
	auto *first = new QLineEdit(&dialog);
	first->setText(targets.first().label.isEmpty() ? QStringLiteral("-K1")
												   : targets.first().label);
	auto *step = new QSpinBox(&dialog);
	step->setRange(1, 1000);
	step->setValue(1);
	form->addRow(tr("First label:"), first);
	form->addRow(tr("Step:"), step);
	layout->addLayout(form);
	auto *preview = new QTreeWidget(&dialog);
	preview->setRootIsDecorated(false);
	preview->setHeaderLabels({tr("Label"), tr("New label"), tr("Sheet")});
	layout->addWidget(preview, 1);
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
										 &dialog);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	layout->addWidget(buttons);
	auto update_preview = [&]() {
		const QStringList labels = numberedLabels(first->text().trimmed(),
												  targets.size(), step->value());
		preview->clear();
		for (int i = 0; i < targets.size(); ++i) {
			const Target &target = targets.at(i);
			new QTreeWidgetItem(preview, {
				target.label.isEmpty() ? tr("(No label)") : target.label,
				labels.at(i),
				QStringLiteral("%1-%2").arg(target.position.folio)
						.arg(target.position.cell)});
		}
		preview->resizeColumnToContents(0);
		preview->resizeColumnToContents(1);
		buttons->button(QDialogButtonBox::Ok)->setEnabled(!first->text().trimmed().isEmpty());
	};
	connect(first, &QLineEdit::textChanged, &dialog, update_preview);
	connect(step, qOverload<int>(&QSpinBox::valueChanged), &dialog, update_preview);
	update_preview();
	dialog.resize(420, 360);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}
	const QStringList labels = numberedLabels(first->text().trimmed(),
											  targets.size(), step->value());

		//A label of a device left out of the numbering is not given twice
	QStringList taken;
	for (const Device &device : devices(m_project)) {
		if (!selected_symbols.contains(device.uuid) && labels.contains(device.label)
				&& !taken.contains(device.label)) {
			taken << device.label;
		}
	}
	if (!taken.isEmpty()) {
		QMessageBox::warning(this, tr("Number devices"),
							 tr("These labels belong to other devices already: %1."
								" Nothing was renamed.")
							 .arg(taken.join(QStringLiteral(", "))));
		return;
	}

		//Every symbol of each device, its folio built
	QSet<QUuid> uuids;
	for (const QString &uuid : std::as_const(selected_symbols)) {
		uuids.insert(QUuid(uuid));
	}
	m_project->buildFoliosHoldingSymbols(uuids);
	QMap<QPointer<Element>, QPair<DiagramContext, DiagramContext>> changes;
	for (int i = 0; i < targets.size(); ++i) {
		for (const auto &symbol : targets.at(i).symbols) {
			Element *element = elementOf(m_project->diagramByUuid(QUuid(symbol.second)),
										 QUuid(symbol.first));
			if (!element) {
				continue;
			}
			const DiagramContext old_information = element->elementInformations();
			DiagramContext new_information = old_information;
				//A label of its own: no formula to make it again
			new_information.addValue(QETInformation::ELMT_LABEL, labels.at(i));
			new_information.addValue(QETInformation::ELMT_FORMULA, QString());
			if (new_information != old_information) {
				changes.insert(element, {old_information, new_information});
			}
		}
	}
	if (changes.isEmpty()) {
		return;
	}
	auto *command = new ChangeElementInformationCommand(changes);
	command->setText(tr("Number %n device(s)", nullptr, targets.size()));
	m_project->undoStack()->push(command);
	scheduleRefresh();
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
		//A device's line stands for all its symbols
	for (QTreeWidgetItem *item : m_tree->selectedItems()) {
		for (const auto &symbol : symbolsOf(item)) {
			QSet<QString> &uuids = by_diagram[symbol.second];
			if (!uuids.contains(symbol.first)) {
				uuids.insert(symbol.first);
				++count;
			}
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
			//A bridged terminal leaves its strip and its bridge like any
			//other (DeleteQGraphicsItemCommand)
		stack->push(new DeleteQGraphicsItemCommand(diagram, content));
	}
	stack->endMacro();
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
