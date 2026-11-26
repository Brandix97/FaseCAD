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
#ifndef PARTSPLACEMENT_H
#define PARTSPLACEMENT_H

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

class Element;
class QMimeData;
class QUndoCommand;

/**
	@brief The PartsPlacement namespace puts a part of the parts database
	on a placed symbol, from the parts collection of the collection panel:

	 - a part dragged onto the folio carries its first symbol
	   (Part::symbols, the usual element uri of a drag) and the part
	   itself with all its symbols (mimeType()); the symbol is placed with
	   the part in its Parts tab and the part's article in its
	   information, Tab going to the part's next symbol meanwhile;
	 - dropped onto a symbol already placed, the part is added to that
	   symbol's Parts tab and nothing else changes.
*/
namespace PartsPlacement
{
	struct Key {
		QString manufacturer;
		QString order_number;
			/// The symbols of the part, the one placed first first: Tab
			/// goes through them while the part is placed
		QStringList symbols;
	};

	QString mimeType();
	QByteArray encode(const Key &key);
	std::optional<Key> decode(const QMimeData *data);

	void addPart(Element *element, const Key &key, bool with_article,
				 QUndoCommand *parent);
	std::function<void(Element *, QUndoCommand *)> preparer(const Key &key);
}

#endif // PARTSPLACEMENT_H
