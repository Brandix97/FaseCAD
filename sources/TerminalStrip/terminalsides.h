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
#ifndef TERMINALSIDES_H
#define TERMINALSIDES_H

#include <QString>

class Element;
class Terminal;

/**
	@brief Which connection point of a terminal goes into the panel and
	which one out of it, to the devices in the field.

	By default the outside is the side wired to a core of a cable, and
	without a cable the lowest connection point: the top goes into the
	panel. "Swap inside and outside of the panel", a choice of each
	placed terminal (the hidden information swappedKey()), turns that
	round (Brandon, 2026-11-16). The terminal strip plans, the element
	properties and the marks drawn beside a selected terminal all read
	it from here.
*/
namespace TerminalSides
{
		/// How the outside was chosen
	enum class Reason {
		None,       ///< fewer than two connection points
		Cable,      ///< the side wired to a core of a cable
		Position    ///< the lowest connection point
	};

		/// The information of a placed terminal holding the choice
	QString swappedKey();
	bool isSwapped(const Element *element);

		/**
			@return the index, in @p element ->terminals(), of the
			connection point which goes out of the panel: 0 for an
			element of one, -1 for none
			@param reason receives how it was chosen, before the swap
		*/
	int outsideIndex(const Element *element, Reason *reason = nullptr);
	Terminal *outside(const Element *element);
		/// The connection point which goes into the panel, the
		/// highest of the others; nullptr with fewer than two
	Terminal *inside(const Element *element);
		/// Where @p terminal stands on the sheet seen from the middle
		/// of its element: "top", "bottom", "left" or "right",
		/// translated
	QString placeOf(const Element *element, const Terminal *terminal);
}

#endif // TERMINALSIDES_H
