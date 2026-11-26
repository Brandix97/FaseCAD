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
#ifndef TAKEOUTOFSTRIPSCOMMAND_H
#define TAKEOUTOFSTRIPSCOMMAND_H

#include "../terminalstrip.h"

#include <QList>
#include <QPair>
#include <QPointer>
#include <QUndoCommand>
#include <QVector>

class Element;

/**
	@brief The TakeOutOfStripsCommand class
	The symbols of @a elements are deleted from the folio, by the parent
	command (a DeleteQGraphicsItemCommand, a RemoveDiagramCommand): the
	terminals among them leave their strips, which list and draw only
	what is on the folios, and come back to the same place, with their
	levels and their bridges, when the deletion is undone (Brandon,
	2026-11-16).
*/
class TakeOutOfStripsCommand : public QUndoCommand
{
	public:
		TakeOutOfStripsCommand(const QList<Element *> &elements,
							   QUndoCommand *parent = nullptr);
		~TakeOutOfStripsCommand() override;

			///True when a terminal of a strip is among @a elements
		static bool isNeededFor(const QList<Element *> &elements);

		void undo() override;
		void redo() override;

	private:
		QVector<QPointer<Element>> m_elements;
		QVector<QPair<QPointer<TerminalStrip>, TerminalStrip::Hole>> m_holes;
};

#endif // TAKEOUTOFSTRIPSCOMMAND_H
