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
#ifndef PARTSARCHIVE_H
#define PARTSARCHIVE_H

#include "partsdatabase.h"

#include <QByteArray>
#include <QList>
#include <QString>

#include <functional>
#include <optional>

/**
	@brief The PartsArchive namespace writes a part of the parts database to
	a .faspart file and reads it back, on another computer as well.

	A .faspart is a zip (QetZip) holding faspart.json, the part itself
	(PartsDatabase::toJson()) with its accessories named by manufacturer
	and order number, the list of its files and of its symbols; then the
	files (image, DXF drawing, 3D model), linked ones copied in, and the
	symbols' definitions. One file holds one part.

	The parts database knows nothing of the collection: reading a symbol
	to write it, and putting one read back into the user's collection, are
	given by whoever calls (SymbolReader, SymbolWriter).
*/
namespace PartsArchive
{
	QString suffix();

		/// A symbol in a .faspart: its file name and its definition
	struct SymbolFile {
		QString name;
		QByteArray definition;
	};
		/// An accessory, named as another database knows it
	struct Accessory {
		QString manufacturer;
		QString order_number;
		double quantity = 1;
	};
		/// A file of the part, its content copied in
	struct File {
		PartFileKind kind = PartFileKind::Image;
		QString name;
		QByteArray content;
	};
		/// Everything a .faspart holds
	struct Content {
		Part part;
		QList<Accessory> accessories;
		QList<File> files;
		QList<SymbolFile> symbols;
	};

		/// The definition of the symbol at an ElementsLocation string,
		/// or a SymbolFile without a name when there is none
	using SymbolReader = std::function<SymbolFile(const QString &location)>;
		/// Put @p symbol in the collection for @p part, and say where
		/// (an ElementsLocation string), empty when it cannot
	using SymbolWriter = std::function<QString(const SymbolFile &symbol,
											   const Part &part)>;

	QString fileNameFor(const Part &part);
	std::optional<Content> contentOf(const PartsDatabase &database, qint64 id,
									 const SymbolReader &reader,
									 QString *error = nullptr);
	bool write(const QString &path, const Content &content,
			   QString *error = nullptr);
	std::optional<Content> read(const QString &path, QString *error = nullptr);
	qint64 store(PartsDatabase &database, const Content &content,
				 qint64 replace_id, const SymbolWriter &writer,
				 QString *error = nullptr);
}

#endif // PARTSARCHIVE_H
