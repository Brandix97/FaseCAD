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
#include "takeoutofstripscommand.h"

#include "../../qetgraphicsitem/terminalelement.h"
#include "../realterminal.h"

namespace
{
		/// The terminal of @a element, when it is a terminal in a strip
	QSharedPointer<RealTerminal> stripTerminalOf(const Element *element)
	{
		const auto *terminal = dynamic_cast<const TerminalElement *>(element);
		if (terminal && terminal->parentTerminalStrip()) {
			return terminal->realTerminal();
		}
		return QSharedPointer<RealTerminal>();
	}
}

TakeOutOfStripsCommand::TakeOutOfStripsCommand(const QList<Element *> &elements,
											   QUndoCommand *parent) :
	QUndoCommand(parent)
{
	for (Element *element : elements) {
		m_elements.append(element);
	}
}

TakeOutOfStripsCommand::~TakeOutOfStripsCommand()
{}

bool TakeOutOfStripsCommand::isNeededFor(const QList<Element *> &elements)
{
	for (const Element *element : elements) {
		if (stripTerminalOf(element)) {
			return true;
		}
	}
	return false;
}

void TakeOutOfStripsCommand::redo()
{
		//Strip by strip, where they stand now
	m_holes.clear();
	QVector<TerminalStrip *> strips;
	QVector<QVector<QSharedPointer<RealTerminal>>> terminals;
	for (const auto &element : std::as_const(m_elements))
	{
		const auto real_t = stripTerminalOf(element);
		if (!real_t) {
			continue;
		}
		TerminalStrip *strip = real_t->parentStrip();
		int i = strips.indexOf(strip);
		if (i < 0) {
			strips.append(strip);
			terminals.append(QVector<QSharedPointer<RealTerminal>>());
			i = strips.size() - 1;
		}
		terminals[i].append(real_t);
	}

	for (int i = 0 ; i < strips.size() ; ++i)
	{
		const TerminalStrip::Hole hole = strips.at(i)->takeOut(terminals.at(i));
		if (!hole.isEmpty()) {
			m_holes.append(qMakePair(QPointer<TerminalStrip>(strips.at(i)), hole));
		}
	}

	QUndoCommand::redo();
}

void TakeOutOfStripsCommand::undo()
{
	QUndoCommand::undo();

	for (int i = m_holes.size() - 1 ; i >= 0 ; --i) {
		if (const auto strip = m_holes.at(i).first) {
			strip->putBack(m_holes.at(i).second);
		}
	}
	m_holes.clear();
}
