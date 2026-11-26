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
#ifndef TERMINALPROPERTIESWIDGET_H
#define TERMINALPROPERTIESWIDGET_H

#include "../ui/abstractelementpropertieseditorwidget.h"

class TerminalSettingsWidget;
class QVBoxLayout;

/**
	@brief The TerminalPropertiesWidget class is the "Terminal" tab of the
	properties of a terminal: its strip, number, colour, type and
	cross-section (FaseTerminal), given in one step of undo.
*/
class TerminalPropertiesWidget : public AbstractElementPropertiesEditorWidget
{
	Q_OBJECT

	public:
		explicit TerminalPropertiesWidget(Element *element,
										  QWidget *parent = nullptr);

		void setElement(Element *element) override;
		QString title() const override {return tr("Terminal");}
		void updateUi() override;
		void apply() override;
		QUndoCommand *associatedUndo() const override;
		bool setLiveEdit(bool live_edit) override;

	protected:
		void enableLiveEdit() override;
		void disableLiveEdit() override;

	private:
		QVBoxLayout *m_layout = nullptr;
		TerminalSettingsWidget *m_form = nullptr;
		QMetaObject::Connection m_live;
		QMetaObject::Connection m_watch;
};

#endif // TERMINALPROPERTIESWIDGET_H
