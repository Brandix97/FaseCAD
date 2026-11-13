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
#ifndef FOLIOINDEX_H
#define FOLIOINDEX_H

#include <QString>

class Diagram;
class QETProject;
class QetGraphicsTableItem;

/**
	@brief The FolioIndex namespace makes the folio index of a project in
	one click: an "Index" folio at the start of the project with a table
	of the number and title of every folio.

	The table is a summary table (ProjectDBModel "summary") reading its
	query from the project database, so it follows the folios as they are
	added, renamed or moved. A summary table whose query is query() is the
	index: when its rows no longer fit, more index folios are added after
	the last one instead of the warning other tables give.
*/
namespace FolioIndex
{
	QString query();
	bool isIndex(const QetGraphicsTableItem *table);
	QetGraphicsTableItem *headTable(QETProject *project);
	Diagram *create(QETProject *project);
	void grow(QetGraphicsTableItem *head);
	void scheduleGrow(QetGraphicsTableItem *head);
}

#endif // FOLIOINDEX_H
