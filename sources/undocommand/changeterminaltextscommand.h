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
#ifndef CHANGETERMINALTEXTSCOMMAND_H
#define CHANGETERMINALTEXTSCOMMAND_H

#include <QList>
#include <QMap>
#include <QPointer>
#include <QUndoCommand>

class DynamicElementTextItem;
class Element;

/**
	@brief The ChangeTerminalTextsCommand class
	Give the terminals of a placed symbol other texts instead of their
	names (Element::setTerminalTexts()), in one step of undo.

	Most symbols draw the number of a terminal as a text of their own
	beside it, not as the terminal's name: such a text, which says the
	terminal's name and is the nearest of those to it, takes the new
	name too. The terminal strip plans of the project are drawn again, as
	they show those names.
*/
class ChangeTerminalTextsCommand : public QUndoCommand
{
	public:
		ChangeTerminalTextsCommand(Element *element,
								   const QMap<QString, QString> &texts,
								   QUndoCommand *parent = nullptr);
		void undo() override;
		void redo() override;

			/// How far from its terminal a text drawing its number may be
		static constexpr qreal number_distance = 40;

	private:
		struct DrawnNumber {
			QPointer<DynamicElementTextItem> text;
			QString old_text;
			QString new_text;
		};

		void findDrawnNumbers();
		void apply(const QMap<QString, QString> &texts, bool forward);

		QPointer<Element> m_element;
		QMap<QString, QString> m_old_texts, m_new_texts;
		QList<DrawnNumber> m_numbers;
};

#endif // CHANGETERMINALTEXTSCOMMAND_H
