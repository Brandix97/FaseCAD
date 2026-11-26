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
#ifndef ELEMENTTERMINALTEXTSWIDGET_H
#define ELEMENTTERMINALTEXTSWIDGET_H

#include "abstractelementpropertieseditorwidget.h"

#include <QMap>

class QTableWidget;

/**
	@brief The ElementTerminalTextsWidget class is the "Terminal texts" tab
	of the properties of a placed symbol: a text for each of its
	terminals, shown and listed instead of the name the symbol gives it
	(Element::setTerminalTexts()), in one step of undo. A text left as
	the symbol's name, or emptied, gives the terminal that name back.
*/
class ElementTerminalTextsWidget : public AbstractElementPropertiesEditorWidget
{
	Q_OBJECT

	public:
		explicit ElementTerminalTextsWidget(Element *element,
											QWidget *parent = nullptr);

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
		QMap<QString, QString> editedTexts() const;

		QTableWidget *m_table = nullptr;
		QMetaObject::Connection m_live;
		QMetaObject::Connection m_watch;
		bool m_updating = false;
};

#endif // ELEMENTTERMINALTEXTSWIDGET_H
