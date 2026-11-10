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
#include "cablemanagerdialog.h"

#include "cablechecks.h"
#include "cablelistrows.h"
#include "../cable/cable.h"
#include "../cable/cablemanager.h"
#include "../cable/cablepart.h"
#include "../cable/cablepropertiesdialog.h"
#include "../cable/editcablecommand.h"
#include "../diagram.h"
#include "../qetproject.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStyle>
#include <QTableWidget>
#include <QTimer>
#include <QUndoStack>
#include <QVBoxLayout>

namespace {

enum CableColumn {
	NameColumn,
	TypeColumn,
	CoresColumn,
	SpareColumn,
	StartColumn,
	EndColumn,
	LengthColumn,
	InstallationColumn,
	LocationColumn,
	ProblemsColumn,
	CableColumnCount
};

enum CoreColumn {
	CoreNumberColumn,
	CoreColorColumn,
	CoreStateColumn,
	CoreWireColumn,
	CoreFromColumn,
	CoreToColumn,
	CoreSheetColumn,
	CoreColumnCount
};

const int uuid_role = Qt::UserRole;

QHash<QETProject *, QPointer<CableManagerDialog>> &openDialogs()
{
	static QHash<QETProject *, QPointer<CableManagerDialog>> dialogs;
	return dialogs;
}

	///"-X1 (3)" : the label of a cable end and its sheet.
QString endText(const QString &label, const QString &sheet)
{
	if (sheet.isEmpty()) {
		return label;
	}
	return label.isEmpty() ? QStringLiteral("(%1)").arg(sheet)
						   : QStringLiteral("%1 (%2)").arg(label, sheet);
}

} // namespace

/**
	@brief CableManagerDialog::showFor
	Show the cable manager of @p project, opening it when it is not open.
*/
void CableManagerDialog::showFor(QETProject *project, QWidget *parent)
{
	if (!project) {
		return;
	}
	QPointer<CableManagerDialog> &dialog = openDialogs()[project];
	if (!dialog) {
		dialog = new CableManagerDialog(project, parent);
		dialog->setAttribute(Qt::WA_DeleteOnClose);
	}
	dialog->show();
	dialog->raise();
	dialog->activateWindow();
}

CableManagerDialog::CableManagerDialog(QETProject *project, QWidget *parent) :
	QDialog(parent),
	m_project(project)
{
	setWindowTitle(tr("Cable manager - %1").arg(project->title()));
	buildUi();

	m_refresh_timer = new QTimer(this);
	m_refresh_timer->setSingleShot(true);
	m_refresh_timer->setInterval(0);
	connect(m_refresh_timer, &QTimer::timeout,
			this, &CableManagerDialog::refresh);

		//A cable drawn, changed or deleted, a label renamed, an undo :
		//the table shows the project as it is now
	connect(project, &QETProject::cableAdded, this, [this]() {
		watchCables();
		scheduleRefresh();
	});
	connect(project, &QETProject::cableRemoved,
			this, &CableManagerDialog::scheduleRefresh);
	connect(project->undoStack(), &QUndoStack::indexChanged,
			this, &CableManagerDialog::scheduleRefresh);
	connect(project, &QObject::destroyed, this, &QWidget::close);

	QSettings settings;
	restoreGeometry(settings.value(
			QStringLiteral("cablemanager/geometry")).toByteArray());

	watchCables();
	refresh();
}

CableManagerDialog::~CableManagerDialog()
{
	QSettings().setValue(QStringLiteral("cablemanager/geometry"),
						 saveGeometry());
}

void CableManagerDialog::buildUi()
{
	resize(1100, 650);
	auto *layout = new QVBoxLayout(this);

	auto *filters = new QHBoxLayout();
	m_search_le = new QLineEdit(this);
	m_search_le->setPlaceholderText(tr("Search a cable"));
	m_search_le->setClearButtonEnabled(true);
	m_problems_cb = new QCheckBox(tr("Only cables with problems"), this);
	m_spare_sb = new QSpinBox(this);
	m_spare_sb->setRange(0, 99);
	m_spare_sb->setValue(QSettings().value(
			QStringLiteral("cablemanager/minimum_spare"), 1).toInt());
	m_spare_sb->setToolTip(tr("A cable with fewer free cores than this is"
							  " marked. 0 checks nothing."));
	filters->addWidget(m_search_le, 1);
	filters->addWidget(m_problems_cb);
	filters->addWidget(new QLabel(tr("Spare cores wanted:"), this));
	filters->addWidget(m_spare_sb);
	layout->addLayout(filters);
	connect(m_search_le, &QLineEdit::textChanged,
			this, &CableManagerDialog::scheduleRefresh);
	connect(m_problems_cb, &QCheckBox::toggled,
			this, &CableManagerDialog::scheduleRefresh);
	connect(m_spare_sb, qOverload<int>(&QSpinBox::valueChanged),
			this, [this](int value) {
		QSettings().setValue(QStringLiteral("cablemanager/minimum_spare"),
							 value);
		scheduleRefresh();
	});

	auto *splitter = new QSplitter(Qt::Vertical, this);
	layout->addWidget(splitter, 1);

	auto *cables_widget = new QWidget(splitter);
	auto *cables_layout = new QVBoxLayout(cables_widget);
	cables_layout->setContentsMargins(0, 0, 0, 0);
	m_cables_table = new QTableWidget(0, CableColumnCount, cables_widget);
	m_cables_table->setHorizontalHeaderLabels({
		tr("Cable"), tr("Type"), tr("Cores used"), tr("Spare"),
		tr("Start"), tr("End"), tr("Length"), tr("Installation"),
		tr("Location"), tr("Problems")});
	m_cables_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_cables_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_cables_table->setEditTriggers(QAbstractItemView::DoubleClicked
									| QAbstractItemView::EditKeyPressed);
	m_cables_table->verticalHeader()->hide();
	m_cables_table->horizontalHeader()->setStretchLastSection(true);
	m_cables_table->setToolTip(tr("Double-click a name, a length, an"
								  " installation or a location to change"
								  " it."));
	connect(m_cables_table, &QTableWidget::itemChanged,
			this, &CableManagerDialog::cellEdited);
	connect(m_cables_table, &QTableWidget::itemSelectionChanged,
			this, &CableManagerDialog::refreshCores);

	auto *buttons = new QHBoxLayout();
	m_edit_pb = new QPushButton(tr("Edit the selected cables..."),
								cables_widget);
	m_properties_pb = new QPushButton(tr("Properties..."), cables_widget);
	m_go_pb = new QPushButton(tr("Show on the sheet"), cables_widget);
	m_summary_label = new QLabel(cables_widget);
	buttons->addWidget(m_edit_pb);
	buttons->addWidget(m_properties_pb);
	buttons->addWidget(m_go_pb);
	buttons->addStretch();
	buttons->addWidget(m_summary_label);
	connect(m_edit_pb, &QPushButton::clicked,
			this, &CableManagerDialog::editSelected);
	connect(m_properties_pb, &QPushButton::clicked,
			this, &CableManagerDialog::openProperties);
	connect(m_go_pb, &QPushButton::clicked,
			this, &CableManagerDialog::goToCable);

	cables_layout->addWidget(m_cables_table, 1);
	cables_layout->addLayout(buttons);

	auto *cores_widget = new QWidget(splitter);
	auto *cores_layout = new QVBoxLayout(cores_widget);
	cores_layout->setContentsMargins(0, 0, 0, 0);
	m_cores_label = new QLabel(cores_widget);
	m_cores_table = new QTableWidget(0, CoreColumnCount, cores_widget);
	m_cores_table->setHorizontalHeaderLabels({
		tr("Core"), tr("Colour"), tr("State"), tr("Wire"), tr("From"),
		tr("To"), tr("Sheet")});
	m_cores_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_cores_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_cores_table->verticalHeader()->hide();
	m_cores_table->horizontalHeader()->setStretchLastSection(true);
	cores_layout->addWidget(m_cores_label);
	cores_layout->addWidget(m_cores_table, 1);

	splitter->setStretchFactor(0, 3);
	splitter->setStretchFactor(1, 2);

	auto *button_box = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::close);
	layout->addWidget(button_box);
}

void CableManagerDialog::scheduleRefresh()
{
	m_refresh_timer->start();
}

/**
	@brief CableManagerDialog::watchCables
	Follow every cable of the project, so a change made anywhere -- the
	sheet, the selection panel, this window -- shows here.
*/
void CableManagerDialog::watchCables()
{
	if (!m_project) {
		return;
	}
	for (Cable *cable : m_project->cables()) {
		connect(cable, &Cable::changed, this,
				&CableManagerDialog::scheduleRefresh, Qt::UniqueConnection);
	}
}

/**
	@brief CableManagerDialog::refresh
	Fill the table of cables, in the order of the cable list, keeping
	the selection.
*/
void CableManagerDialog::refresh()
{
	if (!m_project) {
		return;
	}
	QList<QUuid> selected;
	for (Cable *cable : selectedCables()) {
		selected << cable->uuid();
	}

	QHash<QString, Cable *> by_uuid;
	QHash<QString, int> names;
	for (Cable *cable : m_project->cables()) {
		by_uuid.insert(cable->uuid().toString(), cable);
		const QString name = cable->designation().trimmed().toLower();
		if (!name.isEmpty()) {
			names[name]++;
		}
	}

	const int minimum_spare = m_spare_sb->value();
	const QString search = m_search_le->text().trimmed();
	int total = 0;
	int with_problems = 0;
	int cores = 0;
	int used = 0;

	m_filling = true;
	m_cables_table->setRowCount(0);
	const QIcon warning = style()->standardIcon(QStyle::SP_MessageBoxWarning);
	for (const QMap<QString, QString> &row : CableList::rows(m_project)) {
		Cable *cable = by_uuid.value(row.value(QStringLiteral("uuid")));
		if (!cable) {
			continue;
		}
		CableChecks::Facts facts;
		facts.designation = cable->designation();
		facts.core_count = cable->coreCount();
		facts.used_cores = cable->usedCoreCount();
		facts.duplicate_designation =
				names.value(cable->designation().trimmed().toLower()) > 1;
		const QStringList problems = CableChecks::problems(facts,
														   minimum_spare);
		++total;
		cores += cable->coreCount();
		used += cable->usedCoreCount();
		if (!problems.isEmpty()) {
			++with_problems;
		}

		const QStringList cells = {
			cable->designation(),
			cable->type(),
			QStringLiteral("%1/%2").arg(cable->usedCoreCount())
					.arg(cable->coreCount()),
			cable->coreCount() > 0 ? QString::number(cable->freeCoreCount())
								   : QString(),
			endText(row.value(QStringLiteral("bmk_start")),
					row.value(QStringLiteral("folio_start"))),
			endText(row.value(QStringLiteral("bmk_end")),
					row.value(QStringLiteral("folio_end"))),
			cable->length(),
			cable->installation(),
			cable->location(),
			problems.join(QStringLiteral(", "))
		};

		if (m_problems_cb->isChecked() && problems.isEmpty()) {
			continue;
		}
		if (!search.isEmpty()) {
			bool found = false;
			for (const QString &cell : cells) {
				if (cell.contains(search, Qt::CaseInsensitive)) {
					found = true;
					break;
				}
			}
			if (!found) {
				continue;
			}
		}

		const int table_row = m_cables_table->rowCount();
		m_cables_table->insertRow(table_row);
		for (int column = 0; column < cells.size(); ++column) {
			auto *item = new QTableWidgetItem(cells.at(column));
			item->setData(uuid_role, cable->uuid());
			const bool editable = column == NameColumn
					|| column == LengthColumn
					|| column == InstallationColumn
					|| column == LocationColumn;
			if (!editable) {
				item->setFlags(item->flags() & ~Qt::ItemIsEditable);
			}
			if (column == ProblemsColumn && !problems.isEmpty()) {
				item->setIcon(warning);
			}
			m_cables_table->setItem(table_row, column, item);
		}
	}
		//No sorting : the rows keep the order of the cable list, by
		//sheet and from the top of the sheet down
	m_cables_table->resizeColumnsToContents();
	m_filling = false;

	m_summary_label->setText(
				tr("%n cable(s)", nullptr, total)
				+ QStringLiteral(" - ")
				+ tr("%1 of %2 cores used").arg(used).arg(cores)
				+ QStringLiteral(" - ")
				+ tr("%n with problems", nullptr, with_problems));

	selectCables(selected);
	refreshCores();
}

/**
	@brief CableManagerDialog::refreshCores
	Show every core of the selected cable : its colour, whether it is used
	or spare, and the wire it lands on, from where to where.
*/
void CableManagerDialog::refreshCores()
{
	const QList<Cable *> cables = selectedCables();
	m_edit_pb->setEnabled(!cables.isEmpty());
	m_properties_pb->setEnabled(cables.size() == 1);
	m_go_pb->setEnabled(cables.size() == 1);

	m_cores_table->setRowCount(0);
	if (cables.size() != 1 || !m_project) {
		m_cores_label->setText(cables.isEmpty()
							   ? tr("Select a cable to see its cores.")
							   : tr("Select one cable to see its cores."));
		return;
	}

	Cable *cable = cables.first();
	m_cores_label->setText(tr("Cores of %1: %2 used, %3 spare")
						   .arg(cable->designation())
						   .arg(cable->usedCoreCount())
						   .arg(cable->freeCoreCount()));
		//The same rows the cable plan shows on a sheet
	const QStringList keys = {
		QStringLiteral("plan_core"), QStringLiteral("plan_color"),
		QStringLiteral("plan_state"), QStringLiteral("plan_wire"),
		QStringLiteral("plan_from"), QStringLiteral("plan_to"),
		QStringLiteral("plan_folio")};
	for (const QMap<QString, QString> &core
		 : CableList::coreRows(m_project, cable->uuid())) {
		const bool is_used = core.value(QStringLiteral("used"))
				== QLatin1String("1");
		const int row = m_cores_table->rowCount();
		m_cores_table->insertRow(row);
		for (int column = 0; column < keys.size(); ++column) {
			auto *item = new QTableWidgetItem(core.value(keys.at(column)));
			if (!is_used) {
				item->setForeground(palette().color(QPalette::Disabled,
													QPalette::Text));
			}
			m_cores_table->setItem(row, column, item);
		}
	}
	m_cores_table->resizeColumnsToContents();
}

QList<Cable *> CableManagerDialog::selectedCables() const
{
	QList<Cable *> result;
	if (!m_cables_table->selectionModel()) {
		return result;
	}
	for (const QModelIndex &index
		 : m_cables_table->selectionModel()->selectedRows()) {
		if (Cable *cable = cableOfRow(index.row())) {
			result << cable;
		}
	}
	return result;
}

Cable *CableManagerDialog::cableOfRow(int row) const
{
	QTableWidgetItem *item = m_cables_table->item(row, NameColumn);
	if (!item || !m_project) {
		return nullptr;
	}
	return CableManager::cableByUuid(m_project,
									 item->data(uuid_role).toUuid());
}

void CableManagerDialog::selectCables(const QList<QUuid> &uuids)
{
	const QSignalBlocker blocker(m_cables_table);
	m_cables_table->clearSelection();
	for (int row = 0; row < m_cables_table->rowCount(); ++row) {
		const QUuid uuid = m_cables_table->item(row, NameColumn)
				->data(uuid_role).toUuid();
		if (uuids.contains(uuid)) {
			m_cables_table->selectionModel()->select(
						m_cables_table->model()->index(row, 0),
						QItemSelectionModel::Select
						| QItemSelectionModel::Rows);
		}
	}
}

/**
	@brief CableManagerDialog::cellEdited
	Write a name, a length, an installation or a location typed in the
	table into its cable, as one step of the project's undo stack.
*/
void CableManagerDialog::cellEdited(QTableWidgetItem *item)
{
	if (m_filling || !m_project) {
		return;
	}
	Cable *cable = CableManager::cableByUuid(m_project,
											 item->data(uuid_role).toUuid());
	if (!cable) {
		return;
	}
	const CableProperties before = cable->properties();
	CableProperties after = before;
	const QString text = item->text().trimmed();
	switch (item->column()) {
		case NameColumn:
			after.designation = text;
				//Typed in : numbering the project again asks before
				//replacing it
			after.designation_by_hand = true;
			break;
		case LengthColumn: after.length = text; break;
		case InstallationColumn: after.installation = text; break;
		case LocationColumn: after.location = text; break;
		default: return;
	}
	if (after != before) {
		m_project->undoStack()->push(
					new ChangeCablePropertiesCommand(cable, before, after));
	}
}

/**
	@brief CableManagerDialog::editSelected
	Give the selected cables the same length, installation or location,
	as one step of undo. Only the fields ticked are written.
*/
void CableManagerDialog::editSelected()
{
	const QList<Cable *> cables = selectedCables();
	if (cables.isEmpty() || !m_project) {
		return;
	}

	QDialog dialog(this);
	dialog.setWindowTitle(tr("Edit %n cable(s)", nullptr, cables.size()));
	auto *form = new QFormLayout(&dialog);
	struct Field {
		QCheckBox *apply;
		QLineEdit *edit;
	};
	auto add_field = [&](const QString &label, const QString &value) {
		auto *apply = new QCheckBox(label, &dialog);
		auto *edit = new QLineEdit(value, &dialog);
		connect(edit, &QLineEdit::textEdited, apply, [apply]() {
			apply->setChecked(true);
		});
		form->addRow(apply, edit);
		return Field{apply, edit};
	};
	const Cable *first = cables.first();
	const Field length = add_field(tr("Length"), first->length());
	const Field installation = add_field(tr("Installation"),
										 first->installation());
	const Field location = add_field(tr("Location"), first->location());
	auto *box = new QDialogButtonBox(QDialogButtonBox::Ok
									 | QDialogButtonBox::Cancel, &dialog);
	connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	form->addRow(box);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	QUndoStack *stack = m_project->undoStack();
	stack->beginMacro(tr("Edit %n cable(s)", nullptr, cables.size()));
	for (Cable *cable : cables) {
		const CableProperties before = cable->properties();
		CableProperties after = before;
		if (length.apply->isChecked()) {
			after.length = length.edit->text().trimmed();
		}
		if (installation.apply->isChecked()) {
			after.installation = installation.edit->text().trimmed();
		}
		if (location.apply->isChecked()) {
			after.location = location.edit->text().trimmed();
		}
		if (after != before) {
			stack->push(new ChangeCablePropertiesCommand(cable, before,
														 after));
		}
	}
	stack->endMacro();
}

/**
	@brief CableManagerDialog::openProperties
	Open the dialog the sheet opens on a double-click of the cable, on the
	sheet the cable is drawn on, so its edit goes to the undo stack.
*/
void CableManagerDialog::openProperties()
{
	const QList<Cable *> cables = selectedCables();
	if (cables.size() != 1 || !m_project) {
		return;
	}
	Cable *cable = cables.first();
	Diagram *diagram = nullptr;
	if (!cable->parts().isEmpty()) {
		diagram = m_project->diagramByUuid(cable->parts().first().diagram);
	}
	CablePropertiesDialog dialog(cable, diagram, nullptr, this);
	dialog.exec();
	scheduleRefresh();
}

/**
	@brief CableManagerDialog::goToCable
	Show the sheet the selected cable is drawn on, with the cable selected
	in the middle of the view.
*/
void CableManagerDialog::goToCable()
{
	const QList<Cable *> cables = selectedCables();
	if (cables.size() != 1 || !m_project || cables.first()->parts().isEmpty()) {
		return;
	}
	const CablePartData part = cables.first()->parts().first();
	Diagram *diagram = m_project->diagramByUuid(part.diagram);
	if (!diagram) {
		return;
	}
	diagram->showMe();
	diagram->clearSelection();
	for (QGraphicsItem *item : diagram->items()) {
		auto *cable_part = qgraphicsitem_cast<CablePart *>(item);
		if (cable_part && cable_part->partUuid() == part.uuid) {
			cable_part->setSelected(true);
			for (QGraphicsView *view : diagram->views()) {
				view->centerOn(cable_part);
			}
			break;
		}
	}
}
