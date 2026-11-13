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
#include <QMenu>
#include <QLocale>
#include <QMessageBox>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSet>
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
	///Role of the first cell of a connection point holding its direction,
	///which has no column
const int direction_role = Qt::UserRole + 2;

	///Columns of the table of the connection points
enum ConnectionColumn {
	PlugColumn,
	PinColumn,
	SectionMinColumn,
	SectionMaxColumn,
	WiresColumn,
	StripColumn,
	TorqueColumn,
	ScrewdriverColumn,
	XColumn,
	YColumn,
	ZColumn,
	ConnectionColumnCount
};

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
PartsDatabaseChanges *PartsDatabaseChanges::instance()
{
	static PartsDatabaseChanges changes;
	return &changes;
}

namespace {
	///The window asking for the symbol of a part (setSymbolChooser())
std::function<QString(QWidget *)> symbol_chooser;
	///The name and icon of a symbol (setSymbolDescriber())
std::function<QPair<QIcon, QString>(const QString &)> symbol_describer;
	///The symbols of a .faspart (setSymbolArchiver())
PartsArchive::SymbolReader symbol_reader;
PartsArchive::SymbolWriter symbol_writer;
	///Where the last .faspart files were exported or imported
const QString faspart_dir_key = QStringLiteral("partsdb/faspart-dir");
}

/**
	@brief PartsManagerDialog::setSymbolArchiver
	The application gives how symbols are read from the collection and put
	in it for the .faspart files.
*/
void PartsManagerDialog::setSymbolArchiver(PartsArchive::SymbolReader reader,
										   PartsArchive::SymbolWriter writer)
{
	symbol_reader = std::move(reader);
	symbol_writer = std::move(writer);
}

/**
	@brief PartsManagerDialog::setSymbolDescriber
	The application gives the name and the icon of a symbol of the
	collection: the parts manager itself knows nothing of the collection.
*/
void PartsManagerDialog::setSymbolDescriber(
		std::function<QPair<QIcon, QString>(const QString &location)> describer)
{
	symbol_describer = std::move(describer);
}

/**
	@brief PartsManagerDialog::setSymbolChooser
	The application gives the window choosing a symbol in the collection:
	the parts manager itself knows nothing of the collection.
*/
void PartsManagerDialog::setSymbolChooser(
		std::function<QString(QWidget *parent)> chooser)
{
	symbol_chooser = std::move(chooser);
}

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

		//The .faspart files: one part each, to give to another computer
	auto *export_pb = new QPushButton(tr("Export"), parts_widget);
	export_pb->setObjectName(QStringLiteral("export_parts_pb"));
	export_pb->setToolTip(tr("Write parts to .faspart files, one file per part,"
							 " with their files and symbols"));
	auto *export_menu = new QMenu(export_pb);
	QAction *export_selected = export_menu->addAction(tr("The selected part..."));
	QAction *export_shown = export_menu->addAction(tr("All the parts of the list..."));
	export_pb->setMenu(export_menu);
	connect(export_selected, &QAction::triggered, this, [this]() {
		if (const qint64 id = selectedPartId()) {
			exportParts({id});
		}
	});
	connect(export_shown, &QAction::triggered, this, [this]() {
		QList<qint64> ids;
		for (int row = 0; row < m_parts_table->rowCount(); ++row) {
			ids << m_parts_table->item(row, 0)->data(id_role).toLongLong();
		}
		exportParts(ids);
	});
	auto *import_pb = new QPushButton(tr("Import..."), parts_widget);
	import_pb->setObjectName(QStringLiteral("import_parts_pb"));
	import_pb->setToolTip(tr("Add the parts of .faspart files to the parts database"));
	connect(import_pb, &QPushButton::clicked, this, &PartsManagerDialog::importParts);
	parts_buttons->addWidget(export_pb);
	parts_buttons->addWidget(import_pb);
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
	m_tabs->setUsesScrollButtons(true);
	m_tabs->addTab(buildGeneralTab(), tr("General"));
	m_tabs->addTab(buildMountingTab(), tr("Mounting"));
	m_tabs->addTab(buildTechnicalTab(), tr("Technical data"));
	m_tabs->addTab(buildFunctionsTab(), tr("Functions"));
	m_tabs->addTab(buildConnectionsTab(), tr("Connection points"));
	m_tabs->addTab(buildAccessoriesTab(), tr("Accessories"));
	m_tabs->addTab(buildCableTab(), tr("Cable"));
	m_tabs->addTab(buildSymbolsTab(), tr("Symbols"));
	m_tabs->addTab(buildFilesTab(), tr("Files"));
	form_layout->addWidget(m_tabs, 1);

	auto *form_buttons = new QHBoxLayout();
	form_buttons->addStretch();
	m_revert_pb = new QPushButton(tr("Discard changes"), m_form);
	m_save_pb = new QPushButton(tr("Save"), m_form);
	m_save_pb->setObjectName(QStringLiteral("save_part_pb"));
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
		{tr("GTIN / EAN"), &Part::gtin},
		{QObject::tr("Category"), &Part::category},
		{tr("Device letter"), &Part::code_letter},
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
		if (field.member == &Part::code_letter) {
			edit->setMaxLength(8);
			edit->setPlaceholderText(QStringLiteral("K, Q, F, X..."));
			edit->setToolTip(tr("Letter of the tag a placed device gets, for"
								" instance K for -K1."));
		}
	}
	m_text_edits.at(1).edit->setPlaceholderText(
				tr("Identifies the part, with the manufacturer"));
	m_text_edits.last().edit->setPlaceholderText(QStringLiteral("https://"));


	addNumberEdit(layout, tr("Weight (kg)"), &Part::weight);
	addNumberEdit(layout, tr("Price"), &Part::price);

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

/**
	@brief PartsManagerDialog::addNumberEdit
	Add to @p layout a field for the number @p member of the part : empty
	or a positive number, in the language of the user.
*/
QLineEdit *PartsManagerDialog::addNumberEdit(
		QFormLayout *layout, const QString &label,
		std::optional<double> Part::*member)
{
	auto *edit = new QLineEdit(layout->parentWidget());
	auto *validator = new QDoubleValidator(0, 1e12, 6, edit);
	validator->setNotation(QDoubleValidator::StandardNotation);
	edit->setValidator(validator);
	layout->addRow(label, edit);
	m_number_edits.append({member, edit});
	connect(edit, &QLineEdit::textEdited,
			this, [this]() {setModified(true);});
	return edit;
}

/**
	@brief PartsManagerDialog::buildMountingTab
	The size of the device, the room it needs around it in the cabinet and
	the heat it gives off.
*/
QWidget *PartsManagerDialog::buildMountingTab()
{
	auto *widget = new QWidget();
	auto *layout = new QFormLayout(widget);
	addNumberEdit(layout, tr("Width (mm)"), &Part::width);
	addNumberEdit(layout, tr("Height (mm)"), &Part::height);
	addNumberEdit(layout, tr("Depth (mm)"), &Part::depth);

	auto *hint = new QLabel(tr("Room to leave free around the device in the"
							   " cabinet, for the air and the wires."),
							widget);
	hint->setWordWrap(true);
	layout->addRow(hint);
	addNumberEdit(layout, tr("Free above (mm)"), &Part::clearance_top);
	addNumberEdit(layout, tr("Free below (mm)"), &Part::clearance_bottom);
	addNumberEdit(layout, tr("Free on the left (mm)"), &Part::clearance_left);
	addNumberEdit(layout, tr("Free on the right (mm)"),
				  &Part::clearance_right);
	addNumberEdit(layout, tr("Free in front (mm)"), &Part::clearance_front);
	addNumberEdit(layout, tr("Free behind (mm)"), &Part::clearance_rear);
	addNumberEdit(layout, tr("Power loss (W)"), &Part::power_loss)
			->setToolTip(tr("Heat the device gives off, for the heat balance"
							" of the cabinet."));
	return widget;
}

/**
	@brief PartsManagerDialog::buildFunctionsTab
	What each pin of the device does : the table of the functions.
*/
QWidget *PartsManagerDialog::buildFunctionsTab()
{
	auto *widget = new QWidget();
	auto *layout = new QVBoxLayout(widget);
	auto *hint = new QLabel(tr("What each pin of the device does, in the"
							   " order of the manufacturer. Add copies the"
							   " selected row with the next pin and"
							   " number."), widget);
	hint->setWordWrap(true);

	m_functions_table = new QTableWidget(0, 5, widget);
	m_functions_table->setObjectName(QStringLiteral("functions_table"));
	m_functions_table->setHorizontalHeaderLabels({
		tr("Function"), tr("Plug"), tr("Pin"), tr("Description"),
		tr("PLC address")});
	m_functions_table->horizontalHeader()->setStretchLastSection(true);
	m_functions_table->verticalHeader()->hide();
	m_functions_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_functions_table->setSelectionMode(QAbstractItemView::SingleSelection);
	connect(m_functions_table, &QTableWidget::itemChanged, this, [this]() {
		if (!m_filling) {
			setModified(true);
		}
	});

	auto *buttons = new QHBoxLayout();
	auto *add_pb = new QPushButton(tr("Add"), widget);
	auto *remove_pb = new QPushButton(tr("Remove"), widget);
	add_pb->setObjectName(QStringLiteral("add_function_pb"));
	auto *up_pb = new QPushButton(tr("Move up"), widget);
	auto *down_pb = new QPushButton(tr("Move down"), widget);
	for (QPushButton *button : {add_pb, remove_pb, up_pb, down_pb}) {
		buttons->addWidget(button);
	}
	buttons->addStretch();
	connect(add_pb, &QPushButton::clicked,
			this, &PartsManagerDialog::addFunctionRow);
	connect(remove_pb, &QPushButton::clicked,
			this, &PartsManagerDialog::removeFunctionRow);
	connect(up_pb, &QPushButton::clicked,
			this, [this]() {moveFunctionRow(-1);});
	connect(down_pb, &QPushButton::clicked,
			this, [this]() {moveFunctionRow(1);});

	layout->addWidget(hint);
	layout->addWidget(m_functions_table, 1);
	layout->addLayout(buttons);
	return widget;
}

/**
	@brief PartsManagerDialog::buildConnectionsTab
	Where the wires go on the real device and what they take : the table of
	the connection points.
*/
QWidget *PartsManagerDialog::buildConnectionsTab()
{
	auto *widget = new QWidget();
	auto *layout = new QVBoxLayout(widget);
	auto *hint = new QLabel(tr("Where the wires go on the real device and"
							   " what they take. The position is in mm from"
							   " the top left corner of the front."), widget);
	hint->setWordWrap(true);

	m_connections_table = new QTableWidget(0, ConnectionColumnCount, widget);
	m_connections_table->setObjectName(QStringLiteral("connections_table"));
	m_connections_table->setHorizontalHeaderLabels({
		tr("Plug"), tr("Pin"), tr("Min. section (mm²)"),
		tr("Max. section (mm²)"), tr("Wires"), tr("Stripping (mm)"),
		tr("Torque (N·m)"), tr("Screwdriver"), tr("X (mm)"), tr("Y (mm)"),
		tr("Z (mm)")});
	m_connections_table->horizontalHeaderItem(WiresColumn)->setToolTip(
				tr("How many wires the connection point takes"));
	m_connections_table->horizontalHeader()->setStretchLastSection(true);
	m_connections_table->verticalHeader()->hide();
	m_connections_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_connections_table->setSelectionMode(
				QAbstractItemView::SingleSelection);
	connect(m_connections_table, &QTableWidget::itemChanged, this, [this]() {
		if (!m_filling) {
			setModified(true);
		}
	});

	auto *buttons = new QHBoxLayout();
	auto *add_pb = new QPushButton(tr("Add"), widget);
	auto *remove_pb = new QPushButton(tr("Remove"), widget);
	add_pb->setObjectName(QStringLiteral("add_connection_pb"));
	auto *from_functions_pb = new QPushButton(tr("From the functions"),
											  widget);
	from_functions_pb->setObjectName(QStringLiteral("from_functions_pb"));
	from_functions_pb->setToolTip(tr("Add a connection point for every pin"
									 " of the Functions tab that has none"
									 " yet."));
	for (QPushButton *button : {add_pb, remove_pb, from_functions_pb}) {
		buttons->addWidget(button);
	}
	buttons->addStretch();
	connect(add_pb, &QPushButton::clicked,
			this, &PartsManagerDialog::addConnectionRow);
	connect(remove_pb, &QPushButton::clicked,
			this, &PartsManagerDialog::removeConnectionRow);
	connect(from_functions_pb, &QPushButton::clicked,
			this, &PartsManagerDialog::connectionsFromFunctions);

	layout->addWidget(hint);
	layout->addWidget(m_connections_table, 1);
	layout->addLayout(buttons);
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

/**
	@brief PartsManagerDialog::exportParts
	Write each part of @p ids to its own .faspart in a folder the user
	chooses, asking before replacing a file
*/
void PartsManagerDialog::exportParts(const QList<qint64> &ids)
{
	if (ids.isEmpty() || !m_db_ok || !maySwitchPart()) {
		return;
	}
	QSettings settings;
	const QString folder = QFileDialog::getExistingDirectory(
				this, tr("Export the parts to"),
				settings.value(faspart_dir_key, QDir::homePath()).toString());
	if (folder.isEmpty()) {
		return;
	}
	settings.setValue(faspart_dir_key, folder);

	int written = 0;
	bool replace_all = false;
	bool keep_all = false;
	QStringList failed;
	for (const qint64 id : ids) {
		QString error;
		const auto content = PartsArchive::contentOf(m_db, id, symbol_reader, &error);
		if (!content) {
			failed << error;
			continue;
		}
		const QString path = QDir(folder).filePath(PartsArchive::fileNameFor(content->part));
		if (QFile::exists(path) && !replace_all) {
			if (keep_all) {
				continue;
			}
			const auto answer = QMessageBox::question(
						this, tr("Export the parts"),
						tr("%1 already exists. Replace it?")
						.arg(QDir::toNativeSeparators(path)),
						QMessageBox::Yes | QMessageBox::YesToAll
						| QMessageBox::No | QMessageBox::NoToAll,
						QMessageBox::No);
			replace_all = answer == QMessageBox::YesToAll;
			keep_all = answer == QMessageBox::NoToAll;
			if (answer == QMessageBox::No || keep_all) {
				continue;
			}
		}
		if (PartsArchive::write(path, *content, &error)) {
			++written;
		} else {
			failed << QStringLiteral("%1: %2").arg(QFileInfo(path).fileName(), error);
		}
	}

	QString text = tr("%n part(s) exported to %1.", nullptr, written)
			.arg(QDir::toNativeSeparators(folder));
	if (!failed.isEmpty()) {
		text += QLatin1Char('\n') + tr("Not exported:") + QLatin1Char('\n')
				+ failed.join(QLatin1Char('\n'));
	}
	QMessageBox::information(this, tr("Export the parts"), text);
}

/**
	@brief PartsManagerDialog::importParts
	Add the parts of the .faspart files the user chooses to the database,
	their symbols to the collection. For a part the database has already
	(same manufacturer and order number), ask whether to replace it.
*/
void PartsManagerDialog::importParts()
{
	if (!m_db_ok || !maySwitchPart()) {
		return;
	}
	QSettings settings;
	const QStringList paths = QFileDialog::getOpenFileNames(
				this, tr("Import parts"),
				settings.value(faspart_dir_key, QDir::homePath()).toString(),
				tr("FaseCAD parts (*.faspart)"));
	if (paths.isEmpty()) {
		return;
	}
	settings.setValue(faspart_dir_key, QFileInfo(paths.first()).absolutePath());

	enum class Choice {Ask, ReplaceAll, KeepAll};
	Choice choice = Choice::Ask;
	int imported = 0;
	int kept = 0;
	qint64 last = 0;
	QStringList failed;
	for (const QString &path : paths) {
		QString error;
		const auto content = PartsArchive::read(path, &error);
		if (!content) {
			failed << QStringLiteral("%1: %2").arg(QFileInfo(path).fileName(), error);
			continue;
		}
		const Part &part = content->part;
		const qint64 existing = part.order_number.isEmpty()
				? 0 : m_db.findPart(part.manufacturer, part.order_number);
		qint64 replace_id = 0;
		if (existing) {
			bool replace = choice == Choice::ReplaceAll;
			if (choice == Choice::Ask) {
				QMessageBox box(QMessageBox::Question, tr("Import parts"),
								tr("The parts database has %1 %2 already.\n"
								   "Replace it with the part of the file?")
								.arg(part.manufacturer, part.order_number),
								QMessageBox::NoButton, this);
				QPushButton *replace_pb = box.addButton(tr("Replace"), QMessageBox::YesRole);
				QPushButton *keep_pb = box.addButton(tr("Keep mine"), QMessageBox::NoRole);
				QPushButton *replace_all_pb = box.addButton(tr("Replace all"), QMessageBox::YesRole);
				box.addButton(tr("Keep all mine"), QMessageBox::NoRole);
				box.setDefaultButton(keep_pb);
				box.exec();
				const QAbstractButton *clicked = box.clickedButton();
				replace = clicked == replace_pb || clicked == replace_all_pb;
				if (clicked == replace_all_pb) {
					choice = Choice::ReplaceAll;
				} else if (clicked != replace_pb && clicked != keep_pb) {
					choice = Choice::KeepAll;
				}
			}
			if (!replace) {
				++kept;
				continue;
			}
			replace_id = existing;
		}
		const qint64 id = PartsArchive::store(m_db, *content, replace_id,
											  symbol_writer, &error);
		if (id) {
			++imported;
			last = id;
		} else {
			failed << QStringLiteral("%1: %2").arg(QFileInfo(path).fileName(), error);
		}
	}

	if (imported) {
		emit PartsDatabaseChanges::instance()->changed();
		reloadTree();
		reloadParts(last);
		selectPart(last);
		if (const std::optional<Part> shown = m_db.part(last)) {
			showPart(*shown, false);
		}
	}
	QString text = tr("%n part(s) imported.", nullptr, imported);
	if (kept) {
		text += QLatin1Char(' ') + tr("%n part(s) kept as they were.", nullptr, kept);
	}
	if (!failed.isEmpty()) {
		text += QLatin1Char('\n') + tr("Not imported:") + QLatin1Char('\n')
				+ failed.join(QLatin1Char('\n'));
	}
	QMessageBox::information(this, tr("Import parts"), text);
}

/**
	@brief PartsManagerDialog::buildSymbolsTab
	The symbols the part is placed with from the parts collection of the
	collection panel: the first one comes with the part, Tab goes to the
	next one while it is placed.
*/
QWidget *PartsManagerDialog::buildSymbolsTab()
{
	auto *widget = new QWidget();
	auto *layout = new QVBoxLayout(widget);
	auto *help = new QLabel(tr("Dragged from the parts collection of the"
							   " collection panel, the part comes with the"
							   " first symbol; Tab goes to the next one,"
							   " Shift+Tab back, while it is placed."),
							widget);
	help->setWordWrap(true);
	layout->addWidget(help);

	m_symbols_list = new QListWidget(widget);
	m_symbols_list->setObjectName(QStringLiteral("symbols_list"));
	m_symbols_list->setIconSize(QSize(32, 32));
	layout->addWidget(m_symbols_list, 1);

	auto *buttons = new QHBoxLayout();
	auto *add = new QPushButton(tr("Add..."), widget);
	add->setObjectName(QStringLiteral("add_symbol_pb"));
	add->setToolTip(tr("Choose a symbol of the collection"));
	auto *remove = new QPushButton(tr("Remove"), widget);
	remove->setObjectName(QStringLiteral("remove_symbol_pb"));
	auto *up = new QPushButton(tr("Move up"), widget);
	up->setObjectName(QStringLiteral("symbol_up_pb"));
	auto *down = new QPushButton(tr("Move down"), widget);
	down->setObjectName(QStringLiteral("symbol_down_pb"));
	for (QPushButton *button : {add, remove, up, down}) {
		buttons->addWidget(button);
	}
	buttons->addStretch();
	layout->addLayout(buttons);

	connect(add, &QPushButton::clicked, this, [this]() {
		QString symbol;
		if (symbol_chooser) {
			symbol = symbol_chooser(this);
		} else {
			symbol = QInputDialog::getText(this, tr("Symbol"),
										   tr("Location of the symbol:"));
		}
		symbol = symbol.trimmed();
		if (symbol.isEmpty()) {
			return;
		}
		addSymbolRow(symbol);
		m_symbols_list->setCurrentRow(m_symbols_list->count() - 1);
		setModified(true);
	});
	connect(remove, &QPushButton::clicked, this, [this]() {
		const int row = m_symbols_list->currentRow();
		if (row >= 0) {
			delete m_symbols_list->takeItem(row);
			setModified(true);
		}
	});
	connect(up, &QPushButton::clicked, this, [this]() {moveSymbol(-1);});
	connect(down, &QPushButton::clicked, this, [this]() {moveSymbol(1);});
	return widget;
}

/**
	@brief PartsManagerDialog::addSymbolRow
	Add @p location at the end of the symbols, with the name and icon the
	application gives it
*/
void PartsManagerDialog::addSymbolRow(const QString &location)
{
	auto *item = new QListWidgetItem(location, m_symbols_list);
	item->setData(Qt::UserRole, location);
	item->setToolTip(location);
	if (symbol_describer) {
		const QPair<QIcon, QString> described = symbol_describer(location);
		if (!described.second.isEmpty()) {
			item->setText(described.second);
		}
		item->setIcon(described.first);
	}
}

/**
	@brief PartsManagerDialog::moveSymbol
	Move the selected symbol @p step rows up (negative) or down
*/
void PartsManagerDialog::moveSymbol(int step)
{
	const int row = m_symbols_list->currentRow();
	const int target = row + step;
	if (row < 0 || target < 0 || target >= m_symbols_list->count()) {
		return;
	}
	QListWidgetItem *item = m_symbols_list->takeItem(row);
	m_symbols_list->insertItem(target, item);
	m_symbols_list->setCurrentRow(target);
	setModified(true);
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

		//Two rows of buttons : the window stays narrow enough
	auto *add_buttons = new QHBoxLayout();
	auto *buttons = new QHBoxLayout();
	auto *add_image_pb = new QPushButton(tr("Add an image..."), widget);
	auto *add_dxf_pb = new QPushButton(tr("Add a DXF..."), widget);
	auto *add_step_pb = new QPushButton(tr("Add a 3D model..."), widget);
	add_step_pb->setToolTip(tr("A STEP file (.stp, .step), for the cabinet"
							   " in 3D."));
	auto *link_pb = new QPushButton(tr("Link a file..."), widget);
	link_pb->setToolTip(tr("Keep only the path of the file instead of a"
						   " copy. The file must stay where it is and be"
						   " reachable from every computer using this"
						   " database."));
	m_save_file_pb = new QPushButton(tr("Save as..."), widget);
	m_remove_file_pb = new QPushButton(tr("Remove"), widget);
	for (QPushButton *button : {add_image_pb, add_dxf_pb, add_step_pb}) {
		add_buttons->addWidget(button);
	}
	add_buttons->addStretch();
	for (QPushButton *button : {link_pb, m_save_file_pb, m_remove_file_pb}) {
		buttons->addWidget(button);
	}
	buttons->addStretch();
	m_file_buttons = {add_image_pb, add_dxf_pb, add_step_pb, link_pb};
	connect(add_image_pb, &QPushButton::clicked,
			this, [this]() {addFile(PartFileKind::Image);});
	connect(add_dxf_pb, &QPushButton::clicked,
			this, [this]() {addFile(PartFileKind::Dxf);});
	connect(add_step_pb, &QPushButton::clicked,
			this, [this]() {addFile(PartFileKind::Step);});
	connect(link_pb, &QPushButton::clicked,
			this, &PartsManagerDialog::linkFile);
	connect(m_save_file_pb, &QPushButton::clicked,
			this, &PartsManagerDialog::saveFileAs);
	connect(m_remove_file_pb, &QPushButton::clicked,
			this, &PartsManagerDialog::removeFile);

	layout->addWidget(m_files_hint);
	layout->addWidget(m_files_list, 1);
	layout->addWidget(m_preview_label);
	layout->addLayout(add_buttons);
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

	m_functions_table->setRowCount(0);
	for (const PartFunction &function : part.functions) {
		const int row = m_functions_table->rowCount();
		m_functions_table->insertRow(row);
		setFunctionRow(row, function);
	}
	m_functions_table->resizeColumnsToContents();

	m_connections_table->setRowCount(0);
	for (const PartConnection &connection : part.connections) {
		const int row = m_connections_table->rowCount();
		m_connections_table->insertRow(row);
		setConnectionRow(row, connection);
	}
	m_connections_table->resizeColumnsToContents();

	m_accessories_table->setRowCount(0);
	for (const PartAccessory &accessory : part.accessories) {
		appendAccessoryRow(accessory);
	}

	m_symbols_list->clear();
	for (const QString &symbol : part.symbols) {
		addSymbolRow(symbol);
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
	for (int row = 0; row < m_symbols_list->count(); ++row) {
		part.symbols.append(m_symbols_list->item(row)->data(Qt::UserRole).toString());
	}

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

		//Rows left empty say nothing
	for (int row = 0; row < m_functions_table->rowCount(); ++row) {
		const PartFunction function = functionAt(row);
		if (!(function == PartFunction())) {
			part.functions.append(function);
		}
	}
	for (int row = 0; row < m_connections_table->rowCount(); ++row) {
		PartConnection connection;
		int bad_column = -1;
		if (connectionAt(row, &connection, &bad_column)
				&& !(connection == PartConnection())) {
			part.connections.append(connection);
		}
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
			showField(field.edit);
			showError(tr("This is not a number: %1")
					  .arg(field.edit->text()));
			return false;
		}
	}
	for (int row = 0; row < m_connections_table->rowCount(); ++row) {
		PartConnection connection;
		int bad_column = -1;
		if (!connectionAt(row, &connection, &bad_column)) {
			showField(m_connections_table);
			m_connections_table->setCurrentCell(row, bad_column);
			const QTableWidgetItem *item =
					m_connections_table->item(row, bad_column);
			showError(tr("This is not a number: %1")
					  .arg(item ? item->text() : QString()));
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
	emit PartsDatabaseChanges::instance()->changed();
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
				tr("Delete the part %1, with its files? Projects using it"
				   " keep their element informations.")
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
	emit PartsDatabaseChanges::instance()->changed();
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
	@brief PartsManagerDialog::addFunctionRow
	Add a function under the selected one (or at the end), copied from it
	with the next pin, description and address : the 8 inputs of a card
	are the first one and seven clicks.
*/
void PartsManagerDialog::addFunctionRow()
{
	int base = m_functions_table->currentRow();
	if (base < 0) {
		base = m_functions_table->rowCount() - 1;
	}
	PartFunction function;
	if (base >= 0) {
		function = functionAt(base);
		function.pin = nextText(function.pin);
		function.description = nextText(function.description);
		function.address = nextText(function.address);
	}
	const int row = base + 1;
	m_functions_table->insertRow(row);
	setFunctionRow(row, function);
	m_functions_table->setCurrentCell(row, 2);
	setModified(true);
}

void PartsManagerDialog::removeFunctionRow()
{
	const int row = m_functions_table->currentRow();
	if (row >= 0) {
		m_functions_table->removeRow(row);
		setModified(true);
	}
}

void PartsManagerDialog::moveFunctionRow(int delta)
{
	const int row = m_functions_table->currentRow();
	const int other = row + delta;
	if (row < 0 || other < 0 || other >= m_functions_table->rowCount()) {
		return;
	}
	const PartFunction moved = functionAt(row);
	setFunctionRow(row, functionAt(other));
	setFunctionRow(other, moved);
	m_functions_table->setCurrentCell(
				other, qMax(1, m_functions_table->currentColumn()));
	setModified(true);
}

/**
	@brief PartsManagerDialog::setFunctionRow
	Show @p function in the row @p row, which must exist : the kind in a
	list, the texts in cells.
*/
void PartsManagerDialog::setFunctionRow(int row, const PartFunction &function)
{
	const bool filling = m_filling;
	m_filling = true;
	auto *kind = new QComboBox();
	for (const PartFunctionKind each : PartFunction::kinds()) {
		kind->addItem(PartFunction::kindName(each),
					  PartFunction::kindId(each));
	}
	kind->setCurrentIndex(kind->findData(PartFunction::kindId(function.kind)));
	connect(kind, qOverload<int>(&QComboBox::currentIndexChanged),
			this, [this]() {
		if (!m_filling) {
			setModified(true);
		}
	});
	m_functions_table->setCellWidget(row, 0, kind);
	const QStringList texts = {function.plug, function.pin,
							   function.description, function.address};
	for (int column = 0; column < texts.size(); ++column) {
		m_functions_table->setItem(row, column + 1,
								   new QTableWidgetItem(texts.at(column)));
	}
	m_filling = filling;
}

PartFunction PartsManagerDialog::functionAt(int row) const
{
	PartFunction function;
	if (const auto *kind = qobject_cast<QComboBox *>(
				m_functions_table->cellWidget(row, 0))) {
		function.kind = PartFunction::kindFromId(
					kind->currentData().toString());
	}
	const auto text = [this, row](int column) {
		const QTableWidgetItem *item = m_functions_table->item(row, column);
		return item ? item->text().trimmed() : QString();
	};
	function.plug = text(1);
	function.pin = text(2);
	function.description = text(3);
	function.address = text(4);
	return function;
}

/**
	@brief PartsManagerDialog::addConnectionRow
	Add a connection point under the selected one (or at the end), copied
	from it with the next pin : the terminals of a device mostly take the
	same wires.
*/
void PartsManagerDialog::addConnectionRow()
{
	int base = m_connections_table->currentRow();
	if (base < 0) {
		base = m_connections_table->rowCount() - 1;
	}
	PartConnection connection;
	int bad_column = -1;
	if (base >= 0 && connectionAt(base, &connection, &bad_column)) {
		connection.pin = nextText(connection.pin);
	}
	const int row = base + 1;
	m_connections_table->insertRow(row);
	setConnectionRow(row, connection);
	m_connections_table->setCurrentCell(row, PinColumn);
	setModified(true);
}

void PartsManagerDialog::removeConnectionRow()
{
	const int row = m_connections_table->currentRow();
	if (row >= 0) {
		m_connections_table->removeRow(row);
		setModified(true);
	}
}

/**
	@brief PartsManagerDialog::connectionsFromFunctions
	Add a connection point for each pin of the functions which has none yet,
	in the order of the functions.
*/
void PartsManagerDialog::connectionsFromFunctions()
{
	QSet<QPair<QString, QString>> known;
	for (int row = 0; row < m_connections_table->rowCount(); ++row) {
		PartConnection connection;
		int bad_column = -1;
		connectionAt(row, &connection, &bad_column);
		known.insert({connection.plug, connection.pin});
	}
	int added = 0;
	for (int row = 0; row < m_functions_table->rowCount(); ++row) {
		const PartFunction function = functionAt(row);
		if (function.pin.isEmpty()
				|| known.contains({function.plug, function.pin})) {
			continue;
		}
		known.insert({function.plug, function.pin});
		PartConnection connection;
		connection.plug = function.plug;
		connection.pin = function.pin;
		const int new_row = m_connections_table->rowCount();
		m_connections_table->insertRow(new_row);
		setConnectionRow(new_row, connection);
		++added;
	}
	if (!added) {
		QMessageBox::information(this, tr("Parts manager"),
								 tr("Every pin of the functions already has"
									" its connection point."));
		return;
	}
	m_connections_table->resizeColumnsToContents();
	setModified(true);
}

/**
	@brief PartsManagerDialog::setConnectionRow
	Show @p connection in the row @p row, which must exist. Its direction
	has no column : the first cell keeps it.
*/
void PartsManagerDialog::setConnectionRow(int row,
										  const PartConnection &connection)
{
	const bool filling = m_filling;
	m_filling = true;
	QStringList texts;
	texts << connection.plug << connection.pin
		  << numberText(connection.section_min)
		  << numberText(connection.section_max)
		  << (connection.max_wires ? QLocale().toString(*connection.max_wires)
								   : QString())
		  << numberText(connection.strip_length)
		  << numberText(connection.torque)
		  << connection.screwdriver
		  << numberText(connection.x) << numberText(connection.y)
		  << numberText(connection.z);
	for (int column = 0; column < texts.size(); ++column) {
		m_connections_table->setItem(row, column,
									 new QTableWidgetItem(texts.at(column)));
	}
	m_connections_table->item(row, PlugColumn)->setData(direction_role,
														connection.direction);
	m_filling = filling;
}

/**
	@brief PartsManagerDialog::connectionAt
	Read the connection point of the row @p row into @p connection.
	@return false when a cell which must hold a number does not, and
	@p bad_column says which one.
*/
bool PartsManagerDialog::connectionAt(int row, PartConnection *connection,
									  int *bad_column) const
{
	const auto text = [this, row](int column) {
		const QTableWidgetItem *item = m_connections_table->item(row, column);
		return item ? item->text().trimmed() : QString();
	};
	connection->plug = text(PlugColumn);
	connection->pin = text(PinColumn);
	connection->screwdriver = text(ScrewdriverColumn);
	if (const QTableWidgetItem *item = m_connections_table->item(row,
																 PlugColumn)) {
		connection->direction = item->data(direction_role).toString();
	}

	struct NumberCell {
		int column;
		std::optional<double> PartConnection::*member;
	};
	const NumberCell cells[] = {
		{SectionMinColumn, &PartConnection::section_min},
		{SectionMaxColumn, &PartConnection::section_max},
		{StripColumn, &PartConnection::strip_length},
		{TorqueColumn, &PartConnection::torque},
		{XColumn, &PartConnection::x},
		{YColumn, &PartConnection::y},
		{ZColumn, &PartConnection::z}
	};
	for (const NumberCell &cell : cells) {
		bool ok = true;
		connection->*cell.member = numberFromText(text(cell.column), &ok);
		if (!ok) {
			*bad_column = cell.column;
			return false;
		}
	}

	connection->max_wires.reset();
	const QString wires = text(WiresColumn);
	if (!wires.isEmpty()) {
		bool ok = false;
		int count = QLocale().toInt(wires, &ok);
		if (!ok) {
			count = wires.toInt(&ok);
		}
		if (!ok || count < 0) {
			*bad_column = WiresColumn;
			return false;
		}
		connection->max_wires = count;
	}
	return true;
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
			QString kind = tr("Image");
			if (file.kind == PartFileKind::Dxf) {
				kind = QStringLiteral("DXF");
			} else if (file.kind == PartFileKind::Step) {
				kind = tr("3D model (STEP)");
			}
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
	QString filter = tr("Images (%1)").arg(QLatin1String(image_filter));
	QString title = tr("Add an image");
	if (kind == PartFileKind::Dxf) {
		filter = tr("DXF drawings (*.dxf)");
		title = tr("Add a DXF");
	} else if (kind == PartFileKind::Step) {
		filter = tr("STEP models (*.stp *.step)");
		title = tr("Add a 3D model");
	}
	const QString path = QFileDialog::getOpenFileName(this, title, QString(),
													  filter);
	if (path.isEmpty()) {
		return;
	}
	QString error;
	if (!m_db.addFileFromDisk(m_current.id, kind, path, &error)) {
		showError(error);
		return;
	}
	emit PartsDatabaseChanges::instance()->changed();
	reloadFiles();
}

void PartsManagerDialog::linkFile()
{
	const QString path = QFileDialog::getOpenFileName(
				this, tr("Link a file"), QString(),
				tr("Images, DXF drawings and STEP models"
				   " (%1 *.dxf *.stp *.step)")
				.arg(QLatin1String(image_filter)));
	if (path.isEmpty()) {
		return;
	}
	const QString suffix = QFileInfo(path).suffix().toLower();
	PartFileKind kind = PartFileKind::Image;
	if (suffix == QLatin1String("dxf")) {
		kind = PartFileKind::Dxf;
	} else if (suffix == QLatin1String("stp")
			   || suffix == QLatin1String("step")) {
		kind = PartFileKind::Step;
	}
	QString error;
	if (!m_db.linkFile(m_current.id, kind, path, &error)) {
		showError(error);
		return;
	}
	emit PartsDatabaseChanges::instance()->changed();
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
	emit PartsDatabaseChanges::instance()->changed();
	reloadFiles();
}

void PartsManagerDialog::showError(const QString &error)
{
	QMessageBox::warning(this, tr("Parts manager"), error);
}

/**
	@brief PartsManagerDialog::showField
	Bring the tab holding @p field to the front and give it the focus.
*/
void PartsManagerDialog::showField(QWidget *field)
{
	for (int index = 0; index < m_tabs->count(); ++index) {
		if (m_tabs->widget(index)->isAncestorOf(field)) {
			m_tabs->setCurrentIndex(index);
			break;
		}
	}
	field->setFocus();
}

/**
	@brief PartsManagerDialog::nextText
	@return @p text with the number it ends with plus one, keeping its
	leading zeros : DI a.0 gives DI a.1, X09 gives X10. A text which does
	not end with a number is kept.
*/
QString PartsManagerDialog::nextText(const QString &text)
{
	int start = text.size();
	while (start > 0 && text.at(start - 1).isDigit()) {
		--start;
	}
	const QString digits = text.mid(start);
	bool ok = false;
	const qlonglong number = digits.toLongLong(&ok);
	if (digits.isEmpty() || !ok) {
		return text;
	}
	return text.left(start)
			+ QStringLiteral("%1").arg(number + 1, digits.size(), 10,
									   QLatin1Char('0'));
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
