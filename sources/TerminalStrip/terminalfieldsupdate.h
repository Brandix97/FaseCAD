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
#ifndef TERMINALFIELDSUPDATE_H
#define TERMINALFIELDSUPDATE_H

class Element;
class QDomElement;

/**
	@brief The label and terminal number fields of a placed terminal,
	brought in line with its symbol once "Update elements from the
	collections" replaced that symbol.

	A text field is the placed element's own from the moment it is placed,
	so a new symbol alone changes nothing of them (Brandon, 2026-11-16:
	the terminals already placed should show the strip on the left and
	the number on the right too). For the label and the terminal number:

	 - a field the new symbol has and the old one had not is added;
	 - a field which still stands where the old symbol put it takes the
	   place and the alignment of the new symbol;
	 - a field moved, aligned otherwise or deleted by hand is left alone.
*/
namespace TerminalFieldsUpdate
{
		/**
			@param element a placed terminal
			@param old_definition the &lt;definition&gt; it was placed from
			@param new_definition the &lt;definition&gt; it has now
			@return how many fields were added or moved
		*/
	int adopt(Element *element, const QDomElement &old_definition,
			  const QDomElement &new_definition);
}

#endif // TERMINALFIELDSUPDATE_H
