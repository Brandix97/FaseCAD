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
#ifndef PARTSSYMBOLFILES_H
#define PARTSSYMBOLFILES_H

#include "partsarchive.h"

/**
	@brief The PartsSymbolFiles namespace reads the symbols of a part from
	the collection for its .faspart, and puts the symbols of a .faspart in
	the user's collection, under "Imported parts", one folder per
	manufacturer (PartsArchive::SymbolReader and SymbolWriter).

	A symbol already there with the same definition is the one used; one
	with the same name and another definition is kept, the new one taking
	another name.
*/
namespace PartsSymbolFiles
{
	QString importFolderName();
	PartsArchive::SymbolFile read(const QString &location);
	QString write(const PartsArchive::SymbolFile &symbol, const Part &part);
}

#endif // PARTSSYMBOLFILES_H
