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
#ifndef WIRENUMBERINGDIALOG_H
#define WIRENUMBERINGDIALOG_H

#include "../wirenumbering.h"

#include <QDialog>
#include <QPointer>
#include <QTimer>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTableWidget;

/**
	@brief The WireNumberingDialog class numbers the wires of a project
	(Brandon, 2026-11-19): one of the examples or a formula of one's own,
	the wires of the project, of the sheet or the selected ones, and a
	preview of every name that changes before anything is done. The own
	formulas are kept in the settings, for every project.
*/
class WireNumberingDialog : public QDialog
{
	Q_OBJECT

	public:
		WireNumberingDialog(QETProject *project, Diagram *sheet,
							const QList<Conductor *> &selection,
							QWidget *parent = nullptr);

		WireNumbering::Options options() const;
		QList<WireNumbering::Change> changes() const {return m_changes;}

	private:
		void fillSchemes(const QString &select = QString());
		void schemeChosen();
		void formulaEdited();
		void scheduleRefresh();
		void refresh();
		void saveScheme();
		void deleteScheme();

		QETProject *m_project;
		QPointer<Diagram> m_sheet;
		QList<Conductor *> m_selection;
		QList<WireNumbering::Scheme> m_examples;
		QList<WireNumbering::Scheme> m_own;
		QList<WireNumbering::Change> m_changes;

		QComboBox *m_scheme = nullptr;
		QPushButton *m_save = nullptr;
		QPushButton *m_delete = nullptr;
		QLineEdit *m_formula = nullptr;
		QCheckBox *m_per_wire = nullptr;
		QRadioButton *m_whole_project = nullptr;
		QRadioButton *m_this_sheet = nullptr;
		QRadioButton *m_selected = nullptr;
		QComboBox *m_order = nullptr;
		QSpinBox *m_start = nullptr;
		QCheckBox *m_keep_fixed = nullptr;
		QTableWidget *m_preview = nullptr;
		QLabel *m_count = nullptr;
		QDialogButtonBox *m_buttons = nullptr;
		QTimer m_refresh;
		bool m_filling = false;
};

#endif // WIRENUMBERINGDIALOG_H
