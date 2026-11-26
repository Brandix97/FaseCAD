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
#ifndef ELEMENTXREFDISPLAYWIDGET_H
#define ELEMENTXREFDISPLAYWIDGET_H

#include "abstractelementpropertieseditorwidget.h"
#include "../properties/elementdata.h"

class QCheckBox;
class XRefDisplayForm;

/**
	@brief The "Contact mirror" tab of the properties of a master (a
	relay, a contactor): how its contacts are shown, where (relative to
	the symbol or absolute on the folio) and turned by which angle --
	the fields of the "Relay contact display" dialog (XRefDisplayForm),
	applied as they are changed, in steps of undo (Brandon, 2026-11-16).

	"Use the project setting" leaves the master to the cross references
	page of the project properties; unticked, the master has a display
	of its own, starting from what the project shows now.
*/
class ElementXRefDisplayWidget : public AbstractElementPropertiesEditorWidget
{
	Q_OBJECT

	public:
		explicit ElementXRefDisplayWidget(Element *element, QWidget *parent = nullptr);
		static bool isUsefulFor(const Element *element);
		void setElement(Element *element) override;
		QString title() const override;
		void updateUi() override;
		void apply() override;
		QUndoCommand *associatedUndo() const override;
		bool setLiveEdit(bool live_edit) override;

	protected:
		void enableLiveEdit() override;
		void disableLiveEdit() override;

	private:
		ElementData::XRefDisplay startingDisplay() const;
		ElementData::XRefDisplay editedDisplay() const;
		void changedByTheUser();

		QCheckBox *m_project_cb = nullptr;
		XRefDisplayForm *m_form = nullptr;
		QMetaObject::Connection m_watch;
		bool m_updating = false;
};

#endif // ELEMENTXREFDISPLAYWIDGET_H
