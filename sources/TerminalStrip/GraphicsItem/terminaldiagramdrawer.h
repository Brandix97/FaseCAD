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
#ifndef TERMINALDIAGRAMDRAWER_H
#define TERMINALDIAGRAMDRAWER_H

#include "terminalstripdrawer.h"

class QPainter;

namespace TerminalStripDrawer
{
	/**
	 * @brief The Diagram namespace draws the terminal diagram style: a
	 * table with one column per terminal (function text, PLC designation,
	 * connection of the inner side, bridges, terminal number), and under
	 * it the outer wire of every terminal, marked with its core colour or
	 * number, going down to the device it lands on -- a box with the label
	 * of the device, its pins and its symbol, one box for the terminals
	 * going to the same device.
	 */
	namespace Diagram
	{
		QRectF boundingRect(const AbstractTerminalStripInterface *strip,
							const TerminalStripLayoutPattern &pattern);
		void paint(QPainter *painter,
				   const AbstractTerminalStripInterface &strip,
				   const TerminalStripLayoutPattern &pattern,
				   bool preview_draw = false,
				   const QPointF &hover_pos = QPointF(),
				   QVector<XrefRect> *xref_rects = nullptr);
	}
}

#endif // TERMINALDIAGRAMDRAWER_H
