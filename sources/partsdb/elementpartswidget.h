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
#ifndef ELEMENTPARTSWIDGET_H
#define ELEMENTPARTSWIDGET_H

#include "partslinks.h"
#include "../ui/abstractelementpropertieseditorwidget.h"

#include <QPointer>

class ElementInfoWidget;
class QLabel;
class QPushButton;
class QTableWidget;

/**
	@brief The ElementPartsWidget class is the "Parts" tab of the element
	properties : the parts of the parts database the element is made of,
	with how many units of each.

	A part is chosen in the parts database; once added, the tab asks
	whether its data should overwrite the information of the element : the
	first part writes the main article, the next ones the auxiliary
	articles 1 to 4.

	The tab edits the element through its Information tab : the list is
	kept in the information "parts" (PartsLinks), so the whole edit is
	the Information tab's own step of undo, and Apply, Cancel and the live
	edit work for both tabs alike.
*/
class ElementPartsWidget : public AbstractElementPropertiesEditorWidget
{
	Q_OBJECT

	public:
		ElementPartsWidget(Element *element, ElementInfoWidget *info,
						   QWidget *parent = nullptr);

		void setElement(Element *element) override;
		QString title() const override {return tr("Parts");}
		void updateUi() override;
			/// Everything goes through the Information tab
		QUndoCommand *associatedUndo() const override {return nullptr;}

	private:
		void addPart();
		void removePart();
		void store();
		QList<PartsLinks::Link> links() const;

		QPointer<ElementInfoWidget> m_info;
		QTableWidget *m_table = nullptr;
		QLabel *m_hint = nullptr;
		QPushButton *m_add = nullptr;
		QPushButton *m_remove = nullptr;
		bool m_filling = false;
};

#endif // ELEMENTPARTSWIDGET_H
