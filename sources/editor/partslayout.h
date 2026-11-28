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
#ifndef PARTSLAYOUT_H
#define PARTSLAYOUT_H

#include <QList>
#include <QPointF>
#include <QRectF>
#include <QUndoCommand>

class QGraphicsItem;

/**
	@brief The PartsLayout namespace
	Align and distribute the parts selected in the element editor.
	Aligning puts their edges or centres on the edge or centre of all of
	them together; distributing leaves the first and the last in place and
	puts the same gap between each part and the next, along a row or down
	a column.
*/
namespace PartsLayout
{
	enum class Mode {
		AlignLeft, AlignHCenter, AlignRight,
		AlignTop, AlignVCenter, AlignBottom,
		DistributeHorizontally, DistributeVertically
	};

		/// How far each of @p rects moves for @p mode, in the same order;
		/// all zero when there are too few (2 to align, 3 to distribute)
	QList<QPointF> offsets(const QList<QRectF> &rects, Mode mode);
		/// How many parts @p mode needs
	int minimumCount(Mode mode);
}

/**
	@brief The PartsLayoutCommand class
	Moves parts of the element editor by their own offsets, in one step of
	undo. isValid() is false when nothing moves.
*/
class PartsLayoutCommand : public QUndoCommand
{
	public:
		PartsLayoutCommand(const QList<QGraphicsItem *> &parts,
						   PartsLayout::Mode mode,
						   QUndoCommand *parent = nullptr);
		void undo() override;
		void redo() override;
		bool isValid() const {return !m_parts.isEmpty();}

	private:
		QList<QGraphicsItem *> m_parts;
		QList<QPointF> m_offsets;
};

#endif // PARTSLAYOUT_H
