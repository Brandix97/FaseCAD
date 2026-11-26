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
#ifndef WIREBRIDGES_H
#define WIREBRIDGES_H

#include <QSet>
#include <QSharedPointer>
#include <QVector>

class Conductor;
class RealTerminal;
class TerminalStrip;
class QUndoCommand;

/**
	A wire between two terminals of one terminal strip, side by side, is
	a bridge of that strip (Brandon, 2026-11-25): drawing it bridges them,
	deleting it takes the bridge back, each in the same undo step.
*/
namespace WireBridges
{
		/// The strip @p wire joins two terminals of, and those two
		/// terminals; false when its ends are not two terminals of one
		/// strip
	bool ends(const Conductor *wire, TerminalStrip **strip,
			  QVector<QSharedPointer<RealTerminal>> *terminals);
		/// Bridge the two terminals @p wire joins, when they are side by
		/// side in their strip and not bridged together yet: a child of
		/// @p parent
		/// @return the step made, nullptr when none
	QUndoCommand *bridge(const Conductor *wire, QUndoCommand *parent);
		/// The terminals the wires @p going joined leave their bridge,
		/// when no other wire joins them to it any more: children of
		/// @p parent
		/// @return how many steps were made
	int letGo(const QSet<Conductor *> &going, QUndoCommand *parent);
}

#endif // WIREBRIDGES_H
