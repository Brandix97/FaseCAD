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
#ifndef ELEMENTFORMATPATTERN_H
#define ELEMENTFORMATPATTERN_H

#include <QList>
#include <QVariantHash>

class QObject;
class QUndoCommand;

/**
	@brief The ElementFormatPattern namespace
	Copy format and Apply format of the element editor: the look of one
	part (line style, weight, filling, colour, antialiasing, font, frame)
	taken from it and given to other parts. A property goes only to a part
	that has the same property with the same type, so the colour of a line
	never becomes the colour of a text.
*/
namespace ElementFormatPattern
{
		/// The format properties @p part has, by name
	QVariantHash take(const QObject *part);
		/// One undo command giving @p parts the properties of @p pattern
		/// they have and that differ; nullptr when nothing changes
	QUndoCommand *applyCommand(const QList<QObject *> &parts,
							   const QVariantHash &pattern);

		/// The format copied last, shared by every element editor
	QVariantHash copied();
	void setCopied(const QVariantHash &pattern);
	bool hasCopied();
}

#endif // ELEMENTFORMATPATTERN_H
