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
#ifndef TITLEBLOCKDATADIALOG_H
#define TITLEBLOCKDATADIALOG_H

#include "../diagramcontext.h"

#include <QDialog>
#include <QHash>

class QETProject;
class QLineEdit;
class QTableWidget;
class QUndoCommand;

/**
	@brief The TitleBlockDataDialog class gives the project the data its
	title blocks show (Brandon, 2026-11-19): the company, the client and
	the site, who checked and approved it, the standard, and its
	revisions, the latest first. They are project properties (company,
	client, site, checked-by, revision1-index...), so every title block
	of the project can show them, those of the designer and others.
*/
class TitleBlockDataDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit TitleBlockDataDialog(QETProject *project, QWidget *parent = nullptr);

			/// The project's properties with what the dialog shows
		DiagramContext properties() const;
			/// The undo step giving them to the project, nullptr when
			/// nothing changed
		QUndoCommand *command() const;

			/// The project properties the dialog edits, in its order
		static QStringList keys();

	private:
		void addRevision();
		void removeRevision();

		QETProject *m_project;
		QHash<QString, QLineEdit *> m_fields;
		QTableWidget *m_revisions = nullptr;
};

#endif // TITLEBLOCKDATADIALOG_H
