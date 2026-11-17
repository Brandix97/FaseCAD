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
#ifndef PARTFIELDDIALOG_H
#define PARTFIELDDIALOG_H

#include "parttexts.h"

#include <QDialog>

class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QTableWidget;

/**
	@brief The PartFieldDialog class is the search window where a text of
	a symbol chooses the data of a part it shows (Brandon, 2026-11-16): a
	large search bar over every piece of data of every part of the
	element -- its name, its value and the part it belongs to -- and the
	table of what matches.
*/
class PartFieldDialog : public QDialog
{
	Q_OBJECT

	public:
		PartFieldDialog(const QList<PartTexts::Field> &fields,
						const QString &current_reference,
						QWidget *parent = nullptr);

			/// True when a piece of data is chosen
		bool hasSelection() const;
		PartTexts::Field selectedField() const;

	private:
		void filter();
		int selectedIndex() const;

		QList<PartTexts::Field> m_fields;
		QLineEdit *m_search = nullptr;
		QTableWidget *m_table = nullptr;
		QLabel *m_count = nullptr;
		QDialogButtonBox *m_buttons = nullptr;
};

#endif // PARTFIELDDIALOG_H
