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
#ifndef CABLEMANAGERDIALOG_H
#define CABLEMANAGERDIALOG_H

#include <QDialog>
#include <QPointer>
#include <QUuid>

class Cable;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTableWidgetItem;
class QETProject;
class QTimer;

/**
	@brief The CableManagerDialog class is the window listing every cable
	of a project : a table of the cables with their type, cores, ends and
	problems, and under it the cores of the selected cable, used or
	spare, with the wire each one lands on.

	Names, lengths, installations and locations are edited in the table
	itself, one cable or several at once, and every edit is one step of
	the project's undo stack. The window follows the project : a cable
	drawn, changed or deleted shows at once.

	One window per project : use showFor().
*/
class CableManagerDialog : public QDialog
{
	Q_OBJECT

	public:
		static void showFor(QETProject *project, QWidget *parent);
		~CableManagerDialog() override;

	private:
		CableManagerDialog(QETProject *project, QWidget *parent);

		void buildUi();
		void scheduleRefresh();
		void refresh();
		void refreshCores();
		void watchCables();
		QList<Cable *> selectedCables() const;
		Cable *cableOfRow(int row) const;
		void selectCables(const QList<QUuid> &uuids);
		void cellEdited(QTableWidgetItem *item);
		void editSelected();
		void openProperties();
		void goToCable();

		QPointer<QETProject> m_project;
		QTimer *m_refresh_timer = nullptr;
		bool m_filling = false;

		QLineEdit *m_search_le = nullptr;
		QCheckBox *m_problems_cb = nullptr;
		QSpinBox *m_spare_sb = nullptr;
		QTableWidget *m_cables_table = nullptr;
		QLabel *m_summary_label = nullptr;
		QLabel *m_cores_label = nullptr;
		QTableWidget *m_cores_table = nullptr;
		QPushButton *m_edit_pb = nullptr;
		QPushButton *m_properties_pb = nullptr;
		QPushButton *m_go_pb = nullptr;
};

#endif // CABLEMANAGERDIALOG_H
