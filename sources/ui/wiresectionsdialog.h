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
#ifndef WIRESECTIONSDIALOG_H
#define WIRESECTIONSDIALOG_H

#include "../wiresections.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;

/**
	@brief The WireSectionsDialog class shows, for every protection of the
	project, its rated current, the section IEC 60204-1 asks for in the
	conditions chosen, and the wires it feeds without a section or with a
	smaller one; a button gives them that section, one step of undo
	(Brandon, 2026-11-19). The conditions are kept in the settings.
*/
class WireSectionsDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit WireSectionsDialog(QETProject *project, QWidget *parent = nullptr);

		WireSections::Conditions conditions() const;
		QList<WireSections::Row> rows() const {return m_rows;}

	private:
		void refresh();
		void giveSections();

		QETProject *m_project;
		QList<WireSections::Row> m_rows;
		QComboBox *m_method = nullptr;
		QComboBox *m_ambient = nullptr;
		QSpinBox *m_grouped = nullptr;
		QTableWidget *m_table = nullptr;
		QLabel *m_summary = nullptr;
		QPushButton *m_give = nullptr;
};

#endif // WIRESECTIONSDIALOG_H
