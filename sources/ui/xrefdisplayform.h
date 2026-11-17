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
#ifndef XREFDISPLAYFORM_H
#define XREFDISPLAYFORM_H

#include "../properties/elementdata.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;

/**
	@brief The fields of the contact display of one master (a relay, a
	contactor): cross or contact symbols, where it stands, how its
	contact symbols are turned and how thick its lines are. Shown by the
	"Relay contact display" dialog and by the "Contact mirror" tab of
	the master's properties.

	A manual position is relative to the symbol, which the display then
	follows, or absolute on the folio, where it stays put; switching
	from one to the other keeps the display where it is (Brandon,
	2026-11-16).
*/
class XRefDisplayForm : public QWidget
{
	Q_OBJECT

	public:
		explicit XRefDisplayForm(QWidget *parent = nullptr);

			/**
				@param display what to show; the fields start from it
				even when it is not enabled
				@param symbol_pos where the master stands on the folio
				@param suggested_manual the offset from the symbol a
				manual position starts at when the display has none
			*/
		void setDisplay(const ElementData::XRefDisplay &display,
						const QPointF &symbol_pos,
						const QPointF &suggested_manual);
			/// What the fields say, enabled
		ElementData::XRefDisplay display() const;

	signals:
		void changed();

	private:
		void updateEnabled();
		void anchorChanged();
		void emitChanged();

		QComboBox *m_appearance = nullptr;
		QComboBox *m_position = nullptr;
		QComboBox *m_anchor = nullptr;
		QLabel *m_x_label = nullptr;
		QLabel *m_y_label = nullptr;
		QDoubleSpinBox *m_x = nullptr;
		QDoubleSpinBox *m_y = nullptr;
		QComboBox *m_angle = nullptr;
		QDoubleSpinBox *m_width = nullptr;
		QDoubleSpinBox *m_gap = nullptr;
		QCheckBox *m_show_unlinked = nullptr;
		QPointF m_symbol_pos;
			/// Whether m_x and m_y hold a point of the folio
		bool m_absolute = false;
		bool m_updating = false;
};

#endif // XREFDISPLAYFORM_H
