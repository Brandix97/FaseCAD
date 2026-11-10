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
#include "partsmanagerdialog.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleValidator>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

	///Roles of the items of the tree
const int group_value_role = Qt::UserRole;
const int group_all_role = Qt::UserRole + 1;
	///Role holding the id of a part, or of an accessory, or of a file
const int id_role = Qt::UserRole;

QPointer<PartsManagerDialog> open_dialog;

QString sizeText(qint64 size)
{
	return QLocale().formattedDataSize(size, 1, QLocale::DataSizeSIFormat);
}

	///Every image format Qt reads, for the file dialog.
const char *image_filter = "*.png *.jpg *.jpeg *.bmp *.gif *.svg *.webp";

} // namespace

/**
	@brief PartsManagerDialog::showManager
	Show the parts manager, opening it when it is not open yet.
*/
void PartsManagerDialog::showManager(QWidget *parent)
{
	if (!open_dialog) {
		open_dialog = new PartsManagerDialog(parent);
		open_dialog->setAttribute(Qt::WA_DeleteOnClose);
	}
	open_dialog->show();
	open_dialog->raise();
	open_dialog->activateWindow();
}

PartsManagerDialog::PartsManagerDialog(QWidget *parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Parts manager"));
	buildUi();

	QString error;
	m_db_ok = m_db.open(PartsDatabase::path(), &error);
	m_path_label->setText(tr("Database: %1")
						  .arg(QDir::toNativeSeparators(PartsDatabase::path())));
	if (!m_db_ok) {
		m_splitter->setEnabled(false);
		showError(error);
	}

	QSettings settings;
	restoreGeometry(settings.value(
			QStringLiteral("parts-database/manager-geometry")).toByteArray());
	m_splitter->restoreState(settings.value(
			QStringLiteral("parts-database/manager-splitter")).toByteArray());
	m_group_cb->setCurrentIndex(settings.value(
			QStringLiteral("parts-database/manager-group"), 0).toInt());

	reloadTree();
	reloadParts();
}

PartsManagerDialog::~PartsManagerDialog()
{
	QSettings settings;
	settings.setValue(QStringLiteral("parts-database/manager-geometry"),
					  saveGeometry());
	settings.setValue(QStringLiteral("parts-database/manager-splitter"),
					  m_splitter->saveState());
	settings.setValue(QStringLiteral("parts-database/manager-group"),
					  m_group_cb->currentIndex());
}

/**
	@brief PartsManagerDialog::reject
	The Escape key closes the window the same way the Close button does,
	asking about unsaved changes first.
*/
void PartsManagerDialog::reject()
{
	close();
}

void PartsManagerDialog::closeEvent(QCloseEvent *event)
{
	if (maySwitchPart()) {
		event->accept();
	} else {
		event->ignore();
	}
}

void PartsManagerDialog::buildUi()
{
	resize(1100, 650);
	auto *main_layout = new QVBoxLayout(this);

	m_path_label = new QLabel(this);
	m_path_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
	main_layout->addWidget(m_path_label);

	m_splitter = new QSplitter(Qt::Horizontal, this);
	main_layout->addWidget(m_splitter, 1);

		//Tree of categories or manufacturers
	auto *tree_widget = new QWidget(m_splitter);
	auto *tree_layout = new QVBoxLayout(tree_widget);
	tree_layout->setContentsMargins(0, 0, 0, 0);
	m_group_cb = new QComboBox(tree_widget);
	m_group_cb->addItem(tr("By category"));
	m_group_cb->addItem(tr("By manufacturer"));
	m_tree = new QTreeWidget(tree_widget);
	m_tree->setHeaderHidden(true);
	tree_layout->addWidget(m_group_cb);
	tree_layout->addWidget(m_tree);
	connect(m_group_cb, qOverload<int>(&QComboBox::currentIndexChanged),
			this, [this]() {
		reloadTree();
		reloadParts(selectedPartId());
	});
	connect(m_tree, &QTreeWidget::itemSelectionChanged,
			this, &PartsManagerDialog::treeSelectionChanged);

		//Searchable table of parts
	auto *parts_widget = new QWidget(m_splitter);
	auto *parts_layout = new QVBoxLayout(parts_widget);
	parts_layout->setContentsMargins(0, 0, 0, 0);
	m_search_le = new QLineEdit(parts_widget);
	m_search_le->setPlaceholderText(tr("Search a part"));
	m_search_le->setClearButtonEnabled(true);
	connect(m_search_le, &QLineEdit::textChanged, this, [this]() {
		reloadParts(selectedPartId());
	});

	m_parts_table = new QTableWidget(0, 6, parts_widget);
	m_parts_table->setHorizontalHeaderLabels({
		QObject::tr("Manufacturer"), QObject::tr("Order number"),
		QObject::tr("Textual description"), QObject::tr("Category"),
		QObject::tr("Internal number"), tr("Supplier reference")});
	m_parts_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_parts_table->setSelectionMode(QAbstractItemView::SingleSelection);
	m_parts_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_parts_table->verticalHeader()->hide();
	m_parts_table->setObjectName(QStringLiteral("parts_table"));
	m_parts_table->horizontalHeader()->setStretchLastSection(true);
		//A to Z by manufacturer, as the database gives them
	m_parts_table->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);
	m_parts_table->setSortingEnabled(true);
	connect(m_parts_table, &QTableWidget::itemSelectionChanged,
			this, &PartsManagerDialog::partSelectionChanged);

	auto *parts_buttons = new QHBoxLayout();
	m_new_pb = new QPushButton(tr("New"), parts_widget);
	m_duplicate_pb = new QPushButton(tr("Duplicate"), parts_widget);
	m_remove_pb = new QPushButton(tr("Delete"), parts_widget);
	parts_buttons->addWidget(m_new_pb);
	parts_buttons->addWidget(m_duplicate_pb);
	parts_buttons->addWidget(m_remove_pb);
	parts_buttons->addStretch();
	connect(m_new_pb, &QPushButton::clicked,
			this, &PartsManagerDialog::newPart);
	connect(m_duplicate_pb, &QPushButton::clicked,
			this, &PartsManagerDialog::duplicatePart);
	connect(m_remove_pb, &QPushButton::clicked,
			this, &PartsManagerDialog::removePart);

	parts_layout->addWidget(m_search_le);
	parts_layout->addWidget(m_parts_table, 1);
	parts_layout->addLayout(parts_buttons);

		//Form of the selected part
	m_form = new QWidget(m_splitter);
	auto *form_layout = new QVBoxLayout(m_form);
	form_layout->setContentsMargins(0, 0, 0, 0);
	m_tabs = new QTabWidget(m_form);
	m_tabs->addTab(buildGeneralTab(), tr("General"));
	m_tabs->addTab(buildTechnicalTab(), tr("Technical data"));
	m_tabs->addTab(buildAccessoriesTab(), tr("Accessories"));
	m_tabs->addTab(buildCableTab(), tr("Cable"));
	m_tabs->addTab(buildFilesTab(), tr("Image and DXF"));
	form_layout->addWidget(m_tabs, 1);

	auto *form_buttons = new QHBoxLayout();
	form_buttons->addStretch();
	m_revert_pb = new QPushButton(tr("Discard changes"), m_form);
	m_save_pb = new QPushButton(tr("Save"), m_form);
	m_save_pb->setDefault(true);
	form_buttons->addWidget(m_revert_pb);
	form_buttons->addWidget(m_save_pb);
	form_layout->addLayout(form_buttons);
	connect(m_save_pb, &QPushButton::clicked, this, [this]() {save();});
	connect(m_revert_pb, &QPushButton::clicked,
			this, &PartsManagerDialog::revert);

	m_splitter->setStretchFactor(0, 1);
	m_splitter->setStretchFactor(1, 3);
	m_splitter->setStretchFactor(2, 2);

	auto *button_box = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(button_box, &QDialogButtonBox::rejected,
			this, &QDialog::close);
	main_layout->addWidget(button_box);

	m_form->setEnabled(false);
	setModified(false);
}

QWidget *PartsManagerDialog::buildGeneralTab()
{
	auto *widget = new QWidget();
	auto *layout = new QFormLayout(widget);

	struct TextField {
		QString label;
		QString Part::*member;
	};
	const QList<TextField> text_fields = {
		{QObject::tr("Manufacturer"), &Part::manufacturer},
		{QObject::tr("Order number"), &Part::order_number},
		{QObject::tr("Internal number"), &Part::internal_reference},
		{QObject::tr("Supplier"), &Part::supplier},
		{tr("Supplier reference"), &Part::supplier_reference},
		{QObject::tr("Textual description"), &Part::description},
		{QObject::tr("Article number"), &Part::designation},
		{QObject::tr("Category"), &Part::category},
		{QObject::tr("Model"), &Part::model},
		{QObject::tr("Rated voltage"), &Part::voltage},
		{QObject::tr("Rated current"), &Part::current},
		{QObject::tr("Unity"), &Part::unit},
		{tr("Datasheet"), &Part::datasheet_url}
	};
	for (const TextField &field : text_fields) {
		auto *edit = new QLineEdit(widget);
		layout->addRow(field.label, edit);
		m_text_edits.append({field.member, edit});
		connect(edit, &QLineEdit::textEdited,
				this, [this]() {setModified(true);});
	}
	m_text_edits.at(1).edit->setPlaceholderText(
				tr("Identifies the part, with the manufacturer"));
	m_text_edits.last().edit->setPlaceholderText(QStringLiteral("https://"));

	struct NumberField {
		QString label;
		std::optional<double> Part::*member;
	};
	const QList<NumberField> number_fields = {
		{tr("Width (mm)"), &Part::width},
		{tr("Height (mm)"), &Part::height},
		{tr("Depth (mm)"), &Part::depth},
		{tr("Weight (kg)"), &Part::weight},
		{tr("Price"), &Part::price}
	};
	for (const NumberField &field : number_fields) {
		auto *edit = new QLineEdit(widget);
		auto *validator = new QDoubleValidator(0, 1e12, 6, edit);
		validator->setNotation(QDoubleValidator::StandardNotation);
		edit->setValidator(validator);
		layout->addRow(field.label, edit);
		m_number_edits.append({field.member, edit});
		connect(edit, &QLineEdit::textEdited,
				this, [this]() {setModified(true);});
	}

	m_notes_te = new QPlainTextEdit(widget);
	m_notes_te->setTabChangesFocus(true);
	layout->addRow(QObject::tr("Notes"), m_notes_te);
	connect(m_notes_te, &QPlainTextEdit::textChanged, this, [this]() {
		if (!m_filling) {
			setModified(true);
		}
	});
	return widget;
}

QWidget *PartsManagerDialog::buildTechnicalTab()
{
	auto *widget = new QWidget();
	auto *layout = new QVBoxLayout(widget);
	m_technical_table = new QTableWidget(0, 2, widget);
	m_technical_table->setHorizontalHeaderLabels({tr("Name"), tr("Value")});
	m_technical_table->horizontalHeader()->setStretchLastSection(true);
	m_technical_table->verticalHeader()->hide();
	connect(m_technical_table, &QTableWidget::itemChanged, this, [this]() {
		if (!m_filling) {
			setModified(true);
		}
	});

	auto *buttons = new QHBoxLayout();
	auto *add_pb = new QPushButton(tr("Add"), widget);
	auto *remove_pb = new QPushButton(tr("Remove"), widget);
	buttons->addWidget(add_pb);
	buttons->addWidget(remove_pb);
	buttons->addStretch();
	connect(add_pb, &QPushButton::clicked,
			this, &PartsManagerDialog::addTechnicalRow);
	connect(remove_pb, &QPushButton::clicked,
			this, &PartsManagerDialog::removeTechnicalRow);

	layout->addWidget(m_technical_table, 1);
	layout->addLayout(buttons);
	return widget;
}

QWidget *PartsManagerDialog::buildAccessoriesTab()
{
	auto *widget = new QWidget();
	auto *layout = new QVBoxLayout(widget);
	m_accessories_table = new QTableWidget(0, 3, widget);
	m_accessories_table->setHorizontalHeaderLabels({
		QObject::tr("Manufacturer"), QObject::tr("Order number"),
		QObject::tr("Quantity")});
	m_accessories_table->horizontalHeader()->setStretchLastSection(true);
	m_accessories_table->verticalHeader()->hide();
	m_accessories_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	connect(m_accessories_table, &QTableWidget::itemChanged, this, [this]() {
		if (!m_filling) {
			setModified(true);
		}
	});

	auto *buttons = new QHBoxLayout();
	auto *add_pb = new QPushButton(tr("Add an accessory..."), widget);
	auto *remove_pb = new QPushButton(tr("Remove"), widget);
	buttons->addWidget(add_pb);
	buttons->addWidget(remove_pb);
	buttons->addStretch();
	connect(add_pb, &QPushButton::clicked,
			this, &PartsManagerDialog::addAccessory);
	connect(remove_pb, &QPushButton::clicked,
			this, &PartsManagerDialog::removeAccessory);

	layout->addWidget(m_accessories_table, 1);
	layout->addLayout(buttons);
	return widget;
}

/**
	@brief PartsManagerDialog::buildCableTab
	The fields of a part which is a cable : a part with cores is offered
	as a cable type where a cable is drawn.
*/
QWidget *PartsManagerDialog::buildCableTab()
{
	auto *widget = new QWidget();
	auto *layout = new QFormLayout(widget);

	auto *hint = new QLabel(tr("A part with cores is offered as a cable type"
							   " when a cable is drawn, if the parts"
							   " database is used (Settings)."), widget);
	hint->setWordWrap(true);
	layout->addRow(hint);

	m_cores_sb = new QSpinBox(widget);
	m_cores_sb->setRange(0, 999);
	m_cores_sb->setSpecialValueText(tr("Not a cable"));
	layout->addRow(tr("Number of cores"), m_cores_sb);
	connect(m_cores_sb, qOverload<int>(&QSpinBox::valueChanged),
			this, [this]() {
		if (!m_filling) {
			setModified(true);
		}
	});

	struct TextField {
		QString label;
		QString Part::*member;
		QString placeholder;
	};
	const QList<TextField> fields = {
		{tr("Core colors"), &Part::core_colors,
		 QStringLiteral("BK|BN|GY|BU|GNYE")},
		{tr("Cross-section"), &Part::section, QStringLiteral("2,5")},
		{tr("Sheath color"), &Part::sheath_color, QString()}
	};
	for (const TextField &field : fields) {
		auto *edit = new QLineEdit(widget);
		edit->setPlaceholderText(field.placeholder);
		layout->addRow(field.label, edit);
		m_text_edits.append({field.member, edit});
		connect(edit, &QLineEdit::textEdited,
				this, [this]() {setModified(true);});
	}
	return widget;
}

QWidget *PartsManagerDialog::buildFilesTab()
{
	auto *widget = new QWidget();
	auto *layout = new QVBoxLayout(widget);

	m_files_hint = new QLabel(tr("Save the part first to attach files."),
							  widget);
	m_files_list = new QListWidget(widget);
	connect(m_files_list, &QListWidget::itemSelectionChanged,
			this, &PartsManagerDialog::fileSelectionChanged);

	m_preview_label = new QLabel(widget);
	m_preview_label->setAlignment(Qt::AlignCenter);
	m_preview_label->setMinimumHeight(160);

	auto *buttons = new QHBoxLayout();
	auto *add_image_pb = new QPushButton(tr("Add an image..."), widget);
	auto *add_dxf_pb = new QPushButton(tr("Add a DXF..."), widget);
	auto *link_pb = new QPushButton(tr("Link a file..."), widget);
	link_pb->setToolTip(tr("Keep only the path of the file instead of a"
						   " copy. The file must stay where it is and be"
						   " reachable from every computer using this"
						   " database."));
	m_save_file_pb = new QPushButton(tr("Save as..."), widget);
	m_remove_file_pb = new QPushButton(tr("Remove"), widget);
	for (QPushButton *button : {add_image_pb, add_dxf_pb, link_pb,
								m_save_file_pb, m_remove_file_pb}) {
		buttons->addWidget(button);
	}
	buttons->addStretch();
	m_file_buttons = {add_image_pb, add_dxf_pb, link_pb};
	connect(add_image_pb, &QPushButton::clicked,
			this, [this]() {addFile(PartFileKind::Image);});
	connect(add_dxf_pb, &QPushButton::clicked,
			this, [this]() {addFile(PartFileKind::Dxf);});
	connect(link_pb, &QPushButton::clicked,
			this, &PartsManagerDialog::linkFile);
	connect(m_save_file_pb, &QPushButton::clicked,
			this, &PartsManagerDialog::saveFileAs);
	connect(m_remove_file_pb, &QPushButton::clicked,
			this, &PartsManagerDialog::removeFile);

	layout->addWidget(m_files_hint);
	layout->addWidget(m_files_list, 1);
	layout->addWidget(m_preview_label);
	layout->addLayout(buttons);
	return widget;
}

/**
	@brief PartsManagerDialog::reloadTree
	Fill the tree with the categories or the manufacturers in use, keeping
	the selected group when it still exists.
*/
void PartsManagerDialog::reloadTree()
{
	QString selected;
	bool had_selection = false;
	if (QTreeWidgetItem *item = m_tree->currentItem()) {
		if (!item->data(0, group_all_role).toBool()) {
			selected = item->data(0, group_value_role).toString();
			had_selection = true;
		}
	}

	const bool by_category = m_group_cb->currentIndex() == 0;
	QStringList groups;
	bool has_empty = false;
	if (m_db_ok) {
		groups = by_category ? m_db.categories() : m_db.manufacturers();
		for (const Part &part : m_db.parts()) {
			if ((by_category ? part.category : part.manufacturer).isEmpty()) {
				has_empty = true;
				break;
			}
		}
	}

	const QSignalBlocker blocker(m_tree);
	m_tree->clear();
	auto *all = new QTreeWidgetItem(m_tree, {tr("All parts")});
	all->setData(0, group_all_role, true);
	QTreeWidgetItem *to_select = all;
	for (const QString &group : groups) {
		auto *item = new QTreeWidgetItem(all, {group});
		item->setData(0, group_value_role, group);
		if (had_selection && group == selected) {
			to_select = item;
		}
	}
	if (has_empty) {
		auto *item = new QTreeWidgetItem(all, {by_category
											   ? tr("(No category)")
											   : tr("(No manufacturer)")});
		item->setData(0, group_value_role, QString());
		if (had_selection && selected.isEmpty()) {
			to_select = item;
		}
	}
	all->setExpanded(true);
	m_tree->setCurrentItem(to_select);
}

void PartsManagerDialog::treeSelectionChanged()
{
	reloadParts(selectedPartId());
}

/**
	@brief PartsManagerDialog::reloadParts
	Fill the table with the parts of the selected group matching the
	search, and select @p select_id when it is among them.
*/
void PartsManagerDialog::reloadParts(qint64 select_id)
{
	if (!m_db_ok) {
		return;
	}
	const bool by_category = m_group_cb->currentIndex() == 0;
	bool filter = false;
	QString group;
	if (QTreeWidgetItem *item = m_tree->currentItem()) {
		filter = !item->data(0, group_all_role).toBool();
		group = item->data(0, group_value_role).toString();
	}

	{
		const QSignalBlocker blocker(m_parts_table);
		m_parts_table->setSortingEnabled(false);
		m_parts_table->setRowCount(0);
		for (const Part &part : m_db.parts(m_search_le->text())) {
			if (filter && (by_category ? part.category
									   : part.manufacturer) != group) {
				continue;
			}
			const int row = m_parts_table->rowCount();
			m_parts_table->insertRow(row);
			const QStringList cells = {
				part.manufacturer, part.order_number, part.description,
				part.category, part.internal_reference,
				part.supplier_reference};
			for (int column = 0; column < cells.size(); ++column) {
				auto *item = new QTableWidgetItem(cells.at(column));
				item->setData(id_role, part.id);
				m_parts_table->setItem(row, column, item);
			}
		}
		m_parts_table->setSortingEnabled(true);
		m_parts_table->resizeColumnsToContents();
	}

	if (select_id && !m_is_new) {
		selectPart(select_id);
	}
	if (!m_is_new && selectedPartId() != m_current.id) {
		partSelectionChanged();
	}
}

qint64 PartsManagerDialog::selectedPartId() const
{
	const QList<QTableWidgetItem *> items = m_parts_table->selectedItems();
	return items.isEmpty() ? 0 : items.first()->data(id_role).toLongLong();
}

void PartsManagerDialog::selectPart(qint64 id)
{
	const QSignalBlocker blocker(m_parts_table);
	m_parts_table->clearSelection();
	for (int row = 0; row < m_parts_table->rowCount(); ++row) {
		if (m_parts_table->item(row, 0)->data(id_role).toLongLong() == id) {
			m_parts_table->selectRow(row);
			m_parts_table->scrollToItem(m_parts_table->item(row, 0));
			return;
		}
	}
}

void PartsManagerDialog::partSelectionChanged()
{
	const qint64 id = selectedPartId();
	if (id == m_current.id && !m_is_new) {
		return;
	}
	if (!maySwitchPart()) {
		selectPart(m_is_new ? 0 : m_current.id);
		return;
	}
		//Saving reloads the table : select again what the user clicked
	selectPart(id);
	if (const std::optional<Part> part = id ? m_db.part(id) : std::nullopt) {
		showPart(*part, false);
	} else {
		showPart(Part(), false);
		m_form->setEnabled(false);
	}
}

/**
	@brief PartsManagerDialog::maySwitchPart
	Ask what to do with the changes of the form, if any, before showing
	another part.
	@return false when the user wants to keep editing.
*/
bool PartsManagerDialog::maySwitchPart()
{
	if (!m_modified) {
		return true;
	}
	const auto answer = QMessageBox::question(
				this, tr("Parts manager"),
				tr("The part has unsaved changes. Save them?"),
				QMessageBox::Save | QMessageBox::Discard
				| QMessageBox::Cancel, QMessageBox::Save);
	if (answer == QMessageBox::Cancel) {
		return false;
	}
	if (answer == QMessageBox::Save) {
		return save();
	}
	m_is_new = false;
	setModified(false);
	return true;
}

void PartsManagerDialog::showPart(const Part &part, bool is_new)
{
	m_filling = true;
	m_current = part;
	m_is_new = is_new;

	for (const TextEdit &field : m_text_edits) {
		field.edit->setText(part.*field.member);
	}
	for (const NumberEdit &field : m_number_edits) {
		field.edit->setText(numberText(part.*field.member));
	}
	m_notes_te->setPlainText(part.notes);
	m_cores_sb->setValue(part.core_count);

	m_technical_table->setRowCount(0);
	for (const auto &data : part.technical_data) {
		const int row = m_technical_table->rowCount();
		m_technical_table->insertRow(row);
		m_technical_table->setItem(row, 0, new QTableWidgetItem(data.first));
		m_technical_table->setItem(row, 1, new QTableWidgetItem(data.second));
	}

	m_accessories_table->setRowCount(0);
	for (const PartAccessory &accessory : part.accessories) {
		appendAccessoryRow(accessory);
	}

		//Completion with what the database already holds
	if (m_db_ok) {
		for (const TextEdit &field : m_text_edits) {
			if (field.member == &Part::category) {
				delete field.edit->completer();
				field.edit->setCompleter(
							new QCompleter(m_db.categories(), field.edit));
			} else if (field.member == &Part::manufacturer) {
				delete field.edit->completer();
				field.edit->setCompleter(
							new QCompleter(m_db.manufacturers(), field.edit));
			}
		}
	}

	m_form->setEnabled(true);
	m_duplicate_pb->setEnabled(!is_new && part.id);
	m_remove_pb->setEnabled(!is_new && part.id);
	reloadFiles();
	m_filling = false;
	setModified(is_new);
}

Part PartsManagerDialog::partFromForm() const
{
	Part part;
	part.id = m_is_new ? 0 : m_current.id;
	for (const TextEdit &field : m_text_edits) {
		part.*field.member = field.edit->text().trimmed();
	}
	for (const NumberEdit &field : m_number_edits) {
		bool ok = true;
		part.*field.member = numberFromText(field.edit->text(), &ok);
	}
	part.notes = m_notes_te->toPlainText().trimmed();
	part.core_count = m_cores_sb->value();

	for (int row = 0; row < m_technical_table->rowCount(); ++row) {
		const QTableWidgetItem *key = m_technical_table->item(row, 0);
		const QTableWidgetItem *value = m_technical_table->item(row, 1);
		if (key && !key->text().trimmed().isEmpty()) {
			part.technical_data.append({key->text().trimmed(),
										value ? value->text().trimmed()
											  : QString()});
		}
	}

	for (int row = 0; row < m_accessories_table->rowCount(); ++row) {
		PartAccessory accessory;
		accessory.accessory_id = m_accessories_table->item(row, 0)
				->data(id_role).toLongLong();
		bool ok = true;
		const QTableWidgetItem *quantity = m_accessories_table->item(row, 2);
		const std::optional<double> value =
				numberFromText(quantity ? quantity->text() : QString(), &ok);
		accessory.quantity = value.value_or(1);
		part.accessories.append(accessory);
	}
	return part;
}

void PartsManagerDialog::setModified(bool modified)
{
	if (m_filling) {
		return;
	}
	m_modified = modified;
	m_save_pb->setEnabled(modified);
	m_revert_pb->setEnabled(modified);
}

/**
	@brief PartsManagerDialog::save
	Write the form to the database.
	@return true on success, otherwise the error is shown and the form is
	kept as it is.
*/
bool PartsManagerDialog::save()
{
	for (const NumberEdit &field : m_number_edits) {
		bool ok = true;
		numberFromText(field.edit->text(), &ok);
		if (!ok) {
			m_tabs->setCurrentIndex(0);
			field.edit->setFocus();
			showError(tr("This is not a number: %1")
					  .arg(field.edit->text()));
			return false;
		}
	}

	const Part part = partFromForm();
	if (part.manufacturer.isEmpty() && part.order_number.isEmpty()
			&& part.description.isEmpty()) {
		m_tabs->setCurrentIndex(0);
		showError(tr("Give at least a manufacturer, an order number or a"
					 " description."));
		return false;
	}

	QString error;
	qint64 id = part.id;
	if (m_is_new) {
		id = m_db.addPart(part, &error);
		if (!id) {
			showError(error);
			return false;
		}
	} else if (!m_db.updatePart(part, &error)) {
		showError(error);
		return false;
	}

	m_is_new = false;
	setModified(false);
	reloadTree();
	reloadParts(id);
	if (const std::optional<Part> saved = m_db.part(id)) {
		showPart(*saved, false);
	}
	selectPart(id);
	return true;
}

void PartsManagerDialog::revert()
{
	if (m_is_new) {
		m_is_new = false;
		setModified(false);
		showPart(Part(), false);
		m_form->setEnabled(false);
		partSelectionChanged();
		return;
	}
	if (const std::optional<Part> part = m_db.part(m_current.id)) {
		showPart(*part, false);
	}
}

void PartsManagerDialog::newPart()
{
	if (!maySwitchPart()) {
		return;
	}
	Part part;
		//Ready in the group the user is looking at
	if (QTreeWidgetItem *item = m_tree->currentItem()) {
		if (!item->data(0, group_all_role).toBool()) {
			const QString group = item->data(0, group_value_role).toString();
			if (m_group_cb->currentIndex() == 0) {
				part.category = group;
			} else {
				part.manufacturer = group;
			}
		}
	}
	{
		const QSignalBlocker blocker(m_parts_table);
		m_parts_table->clearSelection();
	}
	showPart(part, true);
	m_tabs->setCurrentIndex(0);
	m_text_edits.first().edit->setFocus();
}

/**
	@brief PartsManagerDialog::duplicatePart
	Start a new part from the selected one. The order number is left empty,
	since it identifies a part, and files are not copied.
*/
void PartsManagerDialog::duplicatePart()
{
	if (m_is_new || !m_current.id || !maySwitchPart()) {
		return;
	}
	std::optional<Part> part = m_db.part(m_current.id);
	if (!part) {
		return;
	}
	part->id = 0;
	part->order_number.clear();
	{
		const QSignalBlocker blocker(m_parts_table);
		m_parts_table->clearSelection();
	}
	showPart(*part, true);
	m_tabs->setCurrentIndex(0);
	m_text_edits.at(1).edit->setFocus();
}

void PartsManagerDialog::removePart()
{
	if (m_is_new || !m_current.id) {
		return;
	}
	const QString name = m_current.order_number.isEmpty()
			? m_current.description
			: m_current.manufacturer + QLatin1Char(' ')
			  + m_current.order_number;
	if (QMessageBox::question(
				this, tr("Parts manager"),
				tr("Delete the part %1, with its image and DXF files?"
				   " Projects using it keep their element informations.")
				.arg(name.trimmed()))
			!= QMessageBox::Yes) {
		return;
	}
	QString error;
	if (!m_db.removePart(m_current.id, &error)) {
		showError(error);
		return;
	}
	setModified(false);
	showPart(Part(), false);
	m_form->setEnabled(false);
	reloadTree();
	reloadParts();
}

void PartsManagerDialog::addTechnicalRow()
{
	const int row = m_technical_table->rowCount();
	m_filling = true;
	m_technical_table->insertRow(row);
	auto *key = new QTableWidgetItem();
	m_technical_table->setItem(row, 0, key);
	m_technical_table->setItem(row, 1, new QTableWidgetItem());
	m_filling = false;
	m_technical_table->setCurrentItem(key);
	m_technical_table->editItem(key);
	setModified(true);
}

void PartsManagerDialog::removeTechnicalRow()
{
	const int row = m_technical_table->currentRow();
	if (row >= 0) {
		m_technical_table->removeRow(row);
		setModified(true);
	}
}

/**
	@brief PartsManagerDialog::addAccessory
	Choose another part of the database as an accessory of this one.
*/
void PartsManagerDialog::addAccessory()
{
	QStringList labels;
	QList<qint64> ids;
	for (const Part &part : m_db.parts()) {
		if (!m_is_new && part.id == m_current.id) {
			continue;
		}
		labels << QStringLiteral("%1 %2 - %3")
				  .arg(part.manufacturer, part.order_number,
					   part.description).trimmed();
		ids << part.id;
	}
	if (labels.isEmpty()) {
		showError(tr("The database has no other part to use as an"
					 " accessory."));
		return;
	}
	bool ok = false;
	const QString chosen = QInputDialog::getItem(
				this, tr("Add an accessory"), tr("Accessory:"), labels, 0,
				false, &ok);
	if (!ok) {
		return;
	}
	PartAccessory accessory;
	accessory.accessory_id = ids.at(labels.indexOf(chosen));
	accessory.quantity = 1;
	m_filling = true;
	appendAccessoryRow(accessory);
	m_filling = false;
	setModified(true);
}

void PartsManagerDialog::appendAccessoryRow(const PartAccessory &accessory)
{
	const std::optional<Part> part = m_db.part(accessory.accessory_id);
	const int row = m_accessories_table->rowCount();
	m_accessories_table->insertRow(row);
	auto *manufacturer = new QTableWidgetItem(part ? part->manufacturer
												   : QString());
	manufacturer->setData(id_role, accessory.accessory_id);
	manufacturer->setFlags(manufacturer->flags() & ~Qt::ItemIsEditable);
	auto *order = new QTableWidgetItem(part ? part->order_number
											: QString());
	order->setFlags(order->flags() & ~Qt::ItemIsEditable);
	m_accessories_table->setItem(row, 0, manufacturer);
	m_accessories_table->setItem(row, 1, order);
	m_accessories_table->setItem(
				row, 2, new QTableWidgetItem(numberText(accessory.quantity)));
}

void PartsManagerDialog::removeAccessory()
{
	const int row = m_accessories_table->currentRow();
	if (row >= 0) {
		m_accessories_table->removeRow(row);
		setModified(true);
	}
}

void PartsManagerDialog::reloadFiles()
{
	const bool saved = !m_is_new && m_current.id;
	m_files_hint->setVisible(!saved);
	for (QPushButton *button : m_file_buttons) {
		button->setEnabled(saved);
	}

	m_files_list->clear();
	if (saved && m_db_ok) {
		for (const PartFile &file : m_db.files(m_current.id)) {
			const QString kind = file.kind == PartFileKind::Dxf
					? QStringLiteral("DXF") : tr("Image");
			const QString text = file.isLinked()
					? tr("%1: %2 (linked: %3)")
					  .arg(kind, file.file_name,
						   QDir::toNativeSeparators(file.linked_path))
					: tr("%1: %2 (%3, copied in the database)")
					  .arg(kind, file.file_name, sizeText(file.size));
			auto *item = new QListWidgetItem(text, m_files_list);
			item->setData(id_role, file.id);
			item->setData(id_role + 1, file.kind == PartFileKind::Image);
		}
	}
		//The image is shown at once
	for (int row = 0; row < m_files_list->count(); ++row) {
		if (m_files_list->item(row)->data(id_role + 1).toBool()) {
			m_files_list->setCurrentRow(row);
			break;
		}
	}
	fileSelectionChanged();
}

void PartsManagerDialog::fileSelectionChanged()
{
	QListWidgetItem *item = m_files_list->currentItem();
	m_save_file_pb->setEnabled(item);
	m_remove_file_pb->setEnabled(item);
	m_preview_label->clear();
	if (!item || !item->data(id_role + 1).toBool()) {
		return;
	}
	QString error;
	QPixmap pixmap;
	if (pixmap.loadFromData(m_db.fileContent(
								item->data(id_role).toLongLong(), &error))) {
		m_preview_label->setPixmap(
					pixmap.scaled(QSize(240, 160), Qt::KeepAspectRatio,
								  Qt::SmoothTransformation));
	} else {
		m_preview_label->setText(error.isEmpty()
								 ? tr("This image cannot be shown.")
								 : error);
	}
}

void PartsManagerDialog::addFile(PartFileKind kind)
{
	const QString filter = kind == PartFileKind::Dxf
			? tr("DXF drawings (*.dxf)")
			: tr("Images (%1)").arg(QLatin1String(image_filter));
	const QString path = QFileDialog::getOpenFileName(
				this, kind == PartFileKind::Dxf ? tr("Add a DXF")
												: tr("Add an image"),
				QString(), filter);
	if (path.isEmpty()) {
		return;
	}
	QString error;
	if (!m_db.addFileFromDisk(m_current.id, kind, path, &error)) {
		showError(error);
		return;
	}
	reloadFiles();
}

void PartsManagerDialog::linkFile()
{
	const QString path = QFileDialog::getOpenFileName(
				this, tr("Link a file"), QString(),
				tr("Images and DXF drawings (%1 *.dxf)")
				.arg(QLatin1String(image_filter)));
	if (path.isEmpty()) {
		return;
	}
	const PartFileKind kind = path.endsWith(QLatin1String(".dxf"),
											Qt::CaseInsensitive)
			? PartFileKind::Dxf : PartFileKind::Image;
	QString error;
	if (!m_db.linkFile(m_current.id, kind, path, &error)) {
		showError(error);
		return;
	}
	reloadFiles();
}

void PartsManagerDialog::saveFileAs()
{
	QListWidgetItem *item = m_files_list->currentItem();
	if (!item) {
		return;
	}
	const qint64 id = item->data(id_role).toLongLong();
	QString file_name;
	for (const PartFile &file : m_db.files(m_current.id)) {
		if (file.id == id) {
			file_name = file.file_name;
		}
	}
	const QString path = QFileDialog::getSaveFileName(
				this, tr("Save as"), file_name);
	if (path.isEmpty()) {
		return;
	}
	QString error;
	if (!m_db.saveFileAs(id, path, &error)) {
		showError(error);
	}
}

void PartsManagerDialog::removeFile()
{
	QListWidgetItem *item = m_files_list->currentItem();
	if (!item) {
		return;
	}
	if (QMessageBox::question(this, tr("Parts manager"),
							  tr("Remove this file from the part?"))
			!= QMessageBox::Yes) {
		return;
	}
	QString error;
	if (!m_db.removeFile(item->data(id_role).toLongLong(), &error)) {
		showError(error);
		return;
	}
	reloadFiles();
}

void PartsManagerDialog::showError(const QString &error)
{
	QMessageBox::warning(this, tr("Parts manager"), error);
}

QString PartsManagerDialog::numberText(const std::optional<double> &value)
{
	return value ? QLocale().toString(*value, 'g', 12) : QString();
}

/**
	@brief PartsManagerDialog::numberFromText
	@return the number written in @p text, in the language of the user or
	with a dot, or no value when @p text is empty. @p ok is false when the
	text is not a number.
*/
std::optional<double> PartsManagerDialog::numberFromText(const QString &text,
														 bool *ok)
{
	*ok = true;
	const QString trimmed = text.trimmed();
	if (trimmed.isEmpty()) {
		return std::nullopt;
	}
	double value = QLocale().toDouble(trimmed, ok);
	if (!*ok) {
		value = QLocale::c().toDouble(trimmed, ok);
	}
	if (!*ok) {
		return std::nullopt;
	}
	return value;
}
