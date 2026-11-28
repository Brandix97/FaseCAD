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
#ifndef STRUCTUREBOXES_H
#define STRUCTUREBOXES_H

#include <QList>
#include <QPointF>

class Diagram;
class DiagramView;
class Element;
class QetShapeItem;
class QUndoCommand;
class QWidget;

/**
	@brief The StructureBoxes namespace makes the structure boxes work
	(Brandon, 2026-11-28): a dash-dotted rectangle (QetShapeItem::
	isStructureBox()) with a plant (=) and a location (+). Every symbol
	whose middle lies in it takes them, written in its information, over
	what it had: when the box is put down, moved, resized or edited, and
	when a symbol is placed or moved into it. In nested boxes the
	smallest one holding the symbol counts. A symbol taken out of a box
	keeps what it was given.
*/
namespace StructureBoxes
{
		/// The smallest structure box of @p folio holding @p point
	QetShapeItem *boxAt(Diagram *folio, const QPointF &point);
		/// Give the symbols of @p folio (only @p only when not empty) the
		/// plant and location of the box they lie in, as children of
		/// @p parent
	void apply(Diagram *folio, QUndoCommand *parent,
			   const QList<Element *> &only = QList<Element *>());
		/// Ask the plant and the location, then put a structure box on the
		/// folio of @p view, in one step of undo
	void insert(DiagramView *view);
		/// Ask the plant and location of @p box again and give them to
		/// what it holds, in one step of undo
	void edit(QetShapeItem *box, QWidget *parent);
}

#endif // STRUCTUREBOXES_H
