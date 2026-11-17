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
#ifndef CABLENUMBERINGDIALOG_H
#define CABLENUMBERINGDIALOG_H

#include <QDialog>

class QDialogButtonBox;
class QETProject;
class QPushButton;
class QRadioButton;

/**
	@brief The window behind the "Cable numbering" entry of the
	numbering menu: the axis the cables of the project are laid out
	along and the button which names every one of them again, -W1,
	-W2... (CableManager::baseName()) in that order.

	A new cable takes the first number free in the project by itself
	(CableManager::nextDesignation()); renumbering closes the holes the
	deleted cables leave, and only happens when he asks for it.
*/
class CableNumberingDialog : public QDialog
{
		Q_OBJECT

	public:
		explicit CableNumberingDialog(QETProject *project, QWidget *parent = nullptr);

	private slots:
			///Put the chosen axis into the project
		void saveAxis();
			///Name every cable of the project again
		void renumber();

	private:
			/**
				Ask what to do with the names he typed in himself.
				@param count how many such names the project holds
				@return 1 to number them too, 0 to leave them alone,
				-1 to do nothing at all
			*/
		int askAboutHandWritten(int count);

	private:
		QETProject *m_project = nullptr;
		QRadioButton *m_axis_x = nullptr;
		QRadioButton *m_axis_y = nullptr;
		QPushButton *m_renumber_pb = nullptr;
		QDialogButtonBox *m_box = nullptr;
};

#endif // CABLENUMBERINGDIALOG_H
