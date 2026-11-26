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
#include "titleblockdatadialog.h"

#include "../qetproject.h"
#include "../titleblock/titleblockdesign.h"

#include <QCoreApplication>
#include <QDate>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPointer>
#include <QPushButton>
#include <QTableWidget>
#include <QUndoCommand>
#include <QVBoxLayout>

namespace
{
	/**
		Gives a project other properties, and its old ones back on undo
	*/
	class ChangeProjectPropertiesCommand : public QUndoCommand
	{
		public:
			ChangeProjectPropertiesCommand(QETProject *project,
										   const DiagramContext &before,
										   const DiagramContext &after) :
				m_project(project), m_before(before), m_after(after)
			{
				setText(QCoreApplication::translate("TitleBlockDataDialog",
													"Change the title block data"));
			}
			void redo() override {if (m_project) m_project->setProjectProperties(m_after);}
			void undo() override {if (m_project) m_project->setProjectProperties(m_before);}

		private:
			QPointer<QETProject> m_project;
			DiagramContext m_before;
			DiagramContext m_after;
	};

	enum Column {Index, Date, Description, Author};
}

QStringList TitleBlockDataDialog::keys()
{
	return {QStringLiteral("company"), QStringLiteral("company-address"),
			QStringLiteral("company-contact"), QStringLiteral("client"),
			QStringLiteral("site"), QStringLiteral("site-address"),
			QStringLiteral("checked-by"), QStringLiteral("checked-date"),
			QStringLiteral("approved-by"), QStringLiteral("approved-date"),
			QStringLiteral("standard")};
}

TitleBlockDataDialog::TitleBlockDataDialog(QETProject *project, QWidget *parent) :
	QDialog(parent),
	m_project(project)
{
	setWindowTitle(tr("Title block data"));
	resize(980, 560);
	const DiagramContext properties = project ? project->projectProperties() : DiagramContext();

	auto field = [this, &properties](QFormLayout *form, const QString &key,
									 const QString &label, const QString &placeholder = QString()) {
		auto *edit = new QLineEdit(properties.value(key).toString(), this);
		edit->setObjectName(key);
		edit->setPlaceholderText(placeholder);
		form->addRow(label, edit);
		m_fields.insert(key, edit);
	};

	auto *company = new QGroupBox(tr("Company"), this);
	auto *company_form = new QFormLayout(company);
	field(company_form, QStringLiteral("company"), tr("Name:"));
	field(company_form, QStringLiteral("company-address"), tr("Address:"));
	field(company_form, QStringLiteral("company-contact"), tr("Contact:"),
		  tr("Phone, e-mail, web"));

	auto *client = new QGroupBox(tr("Client and site"), this);
	auto *client_form = new QFormLayout(client);
	field(client_form, QStringLiteral("client"), tr("Client:"));
	field(client_form, QStringLiteral("site"), tr("Site:"));
	field(client_form, QStringLiteral("site-address"), tr("Address:"));

	auto *signatures = new QGroupBox(tr("Signatures"), this);
	auto *signatures_form = new QFormLayout(signatures);
	auto *drawn = new QLabel(tr("Drawn by: the author and the date of each sheet, in its properties."), this);
	drawn->setWordWrap(true);
	signatures_form->addRow(drawn);
	field(signatures_form, QStringLiteral("checked-by"), tr("Checked by:"));
	field(signatures_form, QStringLiteral("checked-date"), tr("Checked on:"));
	field(signatures_form, QStringLiteral("approved-by"), tr("Approved by:"));
	field(signatures_form, QStringLiteral("approved-date"), tr("Approved on:"));

	auto *drawing = new QGroupBox(tr("Drawing"), this);
	auto *drawing_form = new QFormLayout(drawing);
	field(drawing_form, QStringLiteral("standard"), tr("Standard:"),
		  QStringLiteral("IEC 81346, IEC 60617"));
	auto *worked_out = new QLabel(tr("The paper size and the scale are worked out for each sheet."), this);
	worked_out->setWordWrap(true);
	drawing_form->addRow(worked_out);

	auto *left = new QVBoxLayout();
	left->addWidget(company);
	left->addWidget(client);
	left->addWidget(signatures);
	left->addWidget(drawing);
	left->addStretch();

	auto *revisions = new QGroupBox(tr("Revisions, the latest at the top"), this);
	auto *revisions_layout = new QVBoxLayout(revisions);
	m_revisions = new QTableWidget(0, 4, this);
	m_revisions->setObjectName(QStringLiteral("m_revisions"));
	m_revisions->setHorizontalHeaderLabels({tr("Rev"), tr("Date"), tr("Description"), tr("By")});
	m_revisions->verticalHeader()->hide();
	m_revisions->horizontalHeader()->setSectionResizeMode(Description, QHeaderView::Stretch);
	m_revisions->setSelectionBehavior(QAbstractItemView::SelectRows);
	const QList<TitleBlockDesign::Revision> list = TitleBlockDesign::revisions(properties);
	m_revisions->setRowCount(list.size());
	for (int row = 0 ; row < list.size() ; ++row)
	{
		m_revisions->setItem(row, Index, new QTableWidgetItem(list.at(row).index));
		m_revisions->setItem(row, Date, new QTableWidgetItem(list.at(row).date));
		m_revisions->setItem(row, Description, new QTableWidgetItem(list.at(row).description));
		m_revisions->setItem(row, Author, new QTableWidgetItem(list.at(row).author));
	}
	revisions_layout->addWidget(m_revisions, 1);
	auto *revision_buttons = new QHBoxLayout();
	auto *add = new QPushButton(tr("New revision"), this);
	add->setObjectName(QStringLiteral("add_revision"));
	auto *remove = new QPushButton(tr("Remove"), this);
	revision_buttons->addWidget(add);
	revision_buttons->addWidget(remove);
	revision_buttons->addStretch();
	revisions_layout->addLayout(revision_buttons);
	connect(add, &QPushButton::clicked, this, &TitleBlockDataDialog::addRevision);
	connect(remove, &QPushButton::clicked, this, &TitleBlockDataDialog::removeRevision);

	auto *columns = new QHBoxLayout();
	columns->addLayout(left, 2);
	columns->addWidget(revisions, 3);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	buttons->button(QDialogButtonBox::Ok)->setEnabled(project && !project->isReadOnly());

	auto *layout = new QVBoxLayout(this);
	layout->addLayout(columns, 1);
	layout->addWidget(buttons);
}

void TitleBlockDataDialog::addRevision()
{
	const QString last = m_revisions->rowCount() && m_revisions->item(0, Index)
			? m_revisions->item(0, Index)->text() : QString();
	m_revisions->insertRow(0);
	m_revisions->setItem(0, Index, new QTableWidgetItem(TitleBlockDesign::nextIndex(last)));
	m_revisions->setItem(0, Date, new QTableWidgetItem(
			QLocale().toString(QDate::currentDate(), QLocale::ShortFormat)));
	m_revisions->setItem(0, Description, new QTableWidgetItem());
	m_revisions->setItem(0, Author, new QTableWidgetItem(
			m_revisions->rowCount() > 1 && m_revisions->item(1, Author)
			? m_revisions->item(1, Author)->text() : QString()));
	m_revisions->setCurrentCell(0, Description);
	m_revisions->editItem(m_revisions->item(0, Description));
}

void TitleBlockDataDialog::removeRevision()
{
	const int row = m_revisions->currentRow();
	if (row >= 0) m_revisions->removeRow(row);
}

DiagramContext TitleBlockDataDialog::properties() const
{
	DiagramContext properties = m_project ? m_project->projectProperties() : DiagramContext();
	for (const QString &key : keys())
	{
		const QString text = m_fields.value(key)->text().trimmed();
		if (text.isEmpty()) {
			properties.remove(key);
		} else {
			properties.addValue(key, text);
		}
	}
	QList<TitleBlockDesign::Revision> list;
	auto cell = [this](int row, int column) {
		const QTableWidgetItem *item = m_revisions->item(row, column);
		return item ? item->text().trimmed() : QString();
	};
	for (int row = 0 ; row < m_revisions->rowCount() ; ++row)
	{
		TitleBlockDesign::Revision revision;
		revision.index = cell(row, Index);
		revision.date = cell(row, Date);
		revision.description = cell(row, Description);
		revision.author = cell(row, Author);
		if (revision == TitleBlockDesign::Revision()) continue;
		list << revision;
	}
	TitleBlockDesign::setRevisions(properties, list);
	return properties;
}

QUndoCommand *TitleBlockDataDialog::command() const
{
	if (!m_project) return nullptr;
	const DiagramContext before = m_project->projectProperties();
	const DiagramContext after = properties();
	if (before == after) return nullptr;
	return new ChangeProjectPropertiesCommand(m_project, before, after);
}
