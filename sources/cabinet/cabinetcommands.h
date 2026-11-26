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
#ifndef CABINETCOMMANDS_H
#define CABINETCOMMANDS_H

#include "cabinetitem.h"

#include <QPointer>
#include <QUndoCommand>

class Diagram;

/**
	@brief The ChangeCabinetItemCommand class
	Undo command changing the measures of a part of a cabinet.
*/
class ChangeCabinetItemCommand : public QUndoCommand
{
	public:
		ChangeCabinetItemCommand(CabinetItem *item,
								 const CabinetItem::Properties &properties,
								 QUndoCommand *parent = nullptr);

		void undo() override;
		void redo() override;

	private:
		QPointer<CabinetItem> m_item;
		CabinetItem::Properties m_old;
		CabinetItem::Properties m_new;
};

/**
	@brief The ChangeCabinetScaleCommand class
	Undo command changing the scale a cabinet folio is drawn at.
*/
class ChangeCabinetScaleCommand : public QUndoCommand
{
	public:
		ChangeCabinetScaleCommand(Diagram *diagram, qreal scale,
								  QUndoCommand *parent = nullptr);

		void undo() override;
		void redo() override;

	private:
		QPointer<Diagram> m_diagram;
		qreal m_old;
		qreal m_new;
};

#endif // CABINETCOMMANDS_H
