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
#ifndef SUMMARYGENERATORDIALOG_H
#define SUMMARYGENERATORDIALOG_H

#include <QDialog>
#include <QStringList>

class QComboBox;
class QLabel;
class QLineEdit;
class QRadioButton;

/**
	@brief The SummaryGeneratorDialog class asks how a new PLC summary
	symbol is drawn (Brandon, 2026-11-28): a terminal for each terminal of
	a device of an open project, or for each name of a pattern
	(X1:1-X1:32, I0.0-I3.7), or the example of four; in one column or two.
*/
class SummaryGeneratorDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit SummaryGeneratorDialog(QWidget *parent = nullptr);

		QStringList pins() const;
		bool twoColumns() const;

	private:
		void updatePreview();

		QRadioButton *m_from_device, *m_from_pattern, *m_example;
		QComboBox *m_device;
		QLineEdit *m_pattern;
		QComboBox *m_columns;
		QLabel *m_preview;
};

#endif // SUMMARYGENERATORDIALOG_H
