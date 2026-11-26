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
#ifndef PARTPICKERDIALOG_H
#define PARTPICKERDIALOG_H

#include "partsdatabase.h"

#include <QDialog>

#include <optional>

class PartThumbnails;
class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLineEdit;
class QTableWidget;

/**
	@brief The PartPickerDialog class lets the user browse the parts
	database and choose one part: a search over every word, the categories
	of the database, and the table of the parts. The parts manager can be
	opened from it, to add the part that is missing; coming back to this
	window shows it.
*/
class PartPickerDialog : public QDialog
{
	Q_OBJECT

	public:
			/// @p purpose: what the part is chosen for, each purpose
			/// remembering its own category; "enclosure" starts on the
			/// category of the enclosures (Brandon, 2026-11-21)
		explicit PartPickerDialog(PartsDatabase *database,
								  QWidget *parent = nullptr,
								  const QString &purpose = QString());
			/// The part chosen, with its accessories, when accepted
		std::optional<Part> selectedPart() const;
			/// Offer to list only the parts with a STEP model, on by
			/// default: for the enclosure whose plate is clicked in 3D
			/// (Brandon, 2026-11-26)
		void offerOnlyWithStep();

	protected:
		void changeEvent(QEvent *event) override;

	private:
		void refresh();
		qint64 selectedId() const;

		PartsDatabase *m_db;
		QLineEdit *m_search = nullptr;
		QComboBox *m_category = nullptr;
		QCheckBox *m_with_step = nullptr;
		bool m_step_offered = false;
		QTableWidget *m_table = nullptr;
		QDialogButtonBox *m_buttons = nullptr;
		QString m_settings_key;
			/// The picture of each part, first column
		PartThumbnails *m_thumbnails = nullptr;
};

#endif // PARTPICKERDIALOG_H
