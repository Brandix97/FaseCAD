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
#include "cabinetcommands.h"

#include "../diagram.h"

ChangeCabinetItemCommand::ChangeCabinetItemCommand(
		CabinetItem *item, const CabinetItem::Properties &properties,
		QUndoCommand *parent) :
	QUndoCommand(parent),
	m_item(item),
	m_old(item->properties()),
	m_new(properties)
{
	setText(QObject::tr("Change %1", "undo caption - %1 is a part of a cabinet")
			.arg(item->name()));
}

void ChangeCabinetItemCommand::undo()
{
	if (!m_item) return;
	if (m_item->diagram()) m_item->diagram()->showMe();
	m_item->setProperties(m_old);
}

void ChangeCabinetItemCommand::redo()
{
	if (!m_item) return;
	if (m_item->diagram()) m_item->diagram()->showMe();
	m_item->setProperties(m_new);
}

ChangeCabinetScaleCommand::ChangeCabinetScaleCommand(
		Diagram *diagram, qreal scale, QUndoCommand *parent) :
	QUndoCommand(parent),
	m_diagram(diagram),
	m_old(diagram->cabinetScale()),
	m_new(scale)
{
	setText(QObject::tr("Change the scale of the cabinet to 1:%1", "undo caption")
			.arg(QString::number(scale)));
}

void ChangeCabinetScaleCommand::undo()
{
	if (!m_diagram) return;
	m_diagram->showMe();
	m_diagram->setCabinetScale(m_old);
}

void ChangeCabinetScaleCommand::redo()
{
	if (!m_diagram) return;
	m_diagram->showMe();
	m_diagram->setCabinetScale(m_new);
}
