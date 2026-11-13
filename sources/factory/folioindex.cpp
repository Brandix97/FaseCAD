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
#include "folioindex.h"

#include "../dataBase/ui/summaryquerywidget.h"
#include "../diagram.h"
#include "../qetapp.h"
#include "../qetgraphicsitem/ViewItem/projectdbmodel.h"
#include "../qetgraphicsitem/ViewItem/qetgraphicstableitem.h"
#include "../qetproject.h"
#include "../titleblockproperties.h"
#include "../utils/qetutils.h"

#include <QCoreApplication>
#include <QPointer>
#include <QTimer>

namespace {

QString tr(const char *text)
{
	return QCoreApplication::translate("FolioIndex", text);
}

	/// The index folios are titled "Index"
void setIndexTitle(Diagram *folio)
{
	TitleBlockProperties properties = folio->border_and_titleblock.exportTitleBlock();
	properties.title = tr("Index");
	folio->border_and_titleblock.importTitleBlock(properties);
}

	/// Set while grow() adds folios: adding one changes the index again
bool s_growing = false;

}

/**
	@brief FolioIndex::query
	@return the query of the index: the number and the title of every
	folio, in the order of the project
*/
QString FolioIndex::query()
{
	return QStringLiteral("SELECT folio, title FROM project_summary_view ORDER BY pos");
}

/**
	@brief FolioIndex::isIndex
	@return true if @p table, or the table it continues, is a folio index
*/
bool FolioIndex::isIndex(const QetGraphicsTableItem *table)
{
	if (!table) {
		return false;
	}
	const auto *model = qobject_cast<const ProjectDBModel *>(table->model());
	return model
			&& model->identifier() == SummaryQueryWidget::modelIdentifier()
			&& model->queryString() == query();
}

/**
	@brief FolioIndex::headTable
	@return the first table of the index of @p project, among its built
	folios, or nullptr
*/
QetGraphicsTableItem *FolioIndex::headTable(QETProject *project)
{
	if (!project) {
		return nullptr;
	}
	const QList<Diagram *> folios = project->diagrams();
	for (Diagram *folio : folios) {
		if (!folio || !folio->isLoaded()) {
			continue;
		}
		const QList<QGraphicsItem *> items = folio->items();
		for (QGraphicsItem *item : items) {
			auto *table = qgraphicsitem_cast<QetGraphicsTableItem *>(item);
			if (table && !table->previousTable() && isIndex(table)) {
				return table;
			}
		}
	}
	return nullptr;
}

/**
	@brief FolioIndex::create
	Make the index of @p project: a first folio titled "Index" with the
	table, and as many folios more as its rows need. A project which has
	one already keeps it.
	@return the first folio of the index
*/
Diagram *FolioIndex::create(QETProject *project)
{
	if (!project) {
		return nullptr;
	}
	if (QetGraphicsTableItem *head = headTable(project)) {
		return head->diagram();
	}

	Diagram *folio = project->addNewDiagram(0);
	if (!folio) {
		return nullptr;
	}
	setIndexTitle(folio);

		//As a summary added from its dialog with the default settings
	auto *model = new ProjectDBModel(project, project);
	model->setIdentifier(SummaryQueryWidget::modelIdentifier());
	model->setQuery(query());
	const QString margins = QETUtils::marginsToString(QMargins(5, 5, 10, 5));
	model->setData(model->index(0, 0), int(Qt::AlignLeft), Qt::TextAlignmentRole);
	model->setData(model->index(0, 0), QETApp::diagramTextsFont(), Qt::FontRole);
	model->setData(model->index(0, 0), margins, Qt::UserRole + 1);
	model->setHeaderData(0, Qt::Horizontal, int(Qt::AlignLeft), Qt::TextAlignmentRole);
	model->setHeaderData(0, Qt::Horizontal, QETApp::diagramTextsFont(), Qt::FontRole);
	model->setHeaderData(0, Qt::Horizontal, margins, Qt::UserRole + 1);

	auto *table = new QetGraphicsTableItem();
	table->setTableName(tr("Folio index"));
	table->setModel(model);
	folio->addItem(table);
	table->setPos(50, 50);
	QetGraphicsTableItem::adjustTableToFolio(table);
	grow(table);
	return folio;
}

/**
	@brief FolioIndex::grow
	Add index folios after the last table of the index whose first table
	is @p head, each with a table continuing the one before, until every
	folio of the project has its row.
*/
void FolioIndex::grow(QetGraphicsTableItem *head)
{
	if (s_growing || !head || !head->model() || !head->diagram()
		|| !head->diagram()->project()) {
		return;
	}
	s_growing = true;
	QETProject *project = head->diagram()->project();

	int shown = 0;
	int count = 0;
	QetGraphicsTableItem *last = head;
	for (QetGraphicsTableItem *table = head ; table ; table = table->nextTable()) {
		if (table->displayNRow() <= 0) {
				//A table showing every row needs no other
			s_growing = false;
			return;
		}
		shown += table->displayNRow();
		last = table;
		++count;
	}

	while (shown < head->model()->rowCount() && last->diagram())
	{
		Diagram *folio = project->addNewDiagram(
					project->folioIndex(last->diagram()) + 1);
		if (!folio) {
			break;
		}
		setIndexTitle(folio);
		auto *table = new QetGraphicsTableItem();
		table->setTableName(tr("Folio index %1").arg(++count));
		table->setPreviousTable(last);
		folio->addItem(table);
		table->setPos(50, 50);
		QetGraphicsTableItem::adjustTableToFolio(table);
		if (table->displayNRow() <= 0) {
			break;
		}
		shown += table->displayNRow();
		last = table;
	}
	s_growing = false;
}

/**
	@brief FolioIndex::scheduleGrow
	grow() once the project database has finished telling of its change:
	the index then counts the folios as they are.
*/
void FolioIndex::scheduleGrow(QetGraphicsTableItem *head)
{
	if (s_growing) {
		return;
	}
	QPointer<QetGraphicsTableItem> table(head);
	QTimer::singleShot(0, [table]() {
		if (table) {
			grow(table);
		}
	});
}
