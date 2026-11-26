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
#ifndef PARTSDBSYMBOLS_H
#define PARTSDBSYMBOLS_H

#include "partsarchive.h"

#include <QString>

class PartsDatabase;

/**
	@brief Symbols kept inside the parts database (Brandon, 2026-11-20): a
	symbol file (.elmt) imported for a part, which need not be in the
	collection. It is a file of the part (PartFileKind::Symbol), and the
	part names it among its symbols as "partsdb:" + the id of the part and
	the file name, so it follows the part renamed. Where a symbol of the
	collection is needed (an icon, a drag, a placement) it is put in the
	collection, under "Imported parts" as a .faspart's symbols are, and
	that one is used: so on every computer sharing the database.
*/
namespace PartsDbSymbols
{
	bool isStored(const QString &location);
	QString location(qint64 part_id, const QString &file_name);
	bool decode(const QString &location, qint64 *part_id, QString *file_name);
		/// The file name of a stored symbol, empty for another one
	QString fileName(const QString &location);

		/// Whether @p definition is the definition of a symbol
	bool isSymbol(const QByteArray &definition);
		/// The stored symbol @p location, from @p database; empty when
		/// it is not there
	PartsArchive::SymbolFile read(const PartsDatabase &database, const QString &location);
		/// The symbol of the collection @p location stands for: a symbol
		/// of the collection as it is, a stored one put in the collection
		/// first; empty when it cannot be
	QString resolve(const PartsDatabase &database, const QString &location);
}

#endif // PARTSDBSYMBOLS_H
