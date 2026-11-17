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
#ifndef PARTTEXTS_H
#define PARTTEXTS_H

#include <QCoreApplication>
#include <QList>
#include <QString>

class Element;
class PartsDatabase;
struct Part;

/**
	@brief The texts of a symbol which show data of one of its parts
	(DynamicElementTextItem::PartInfo, Brandon, 2026-11-16): the
	description, the rated voltage, a line of technical data... of a part
	of the parts database the element is made of.

	A text names the part by its manufacturer and order number, as the
	"parts" information of the element does (PartsLinks), and the data by
	an id: a field of the part ("description", "voltage"...) or a line of
	its technical data ("data:" and the name of the line). It keeps the
	value it last read, so it shows the same on a computer without the
	parts database; it reads it again when its folio is built and when the
	parts manager writes the database.
*/
class PartTexts
{
	Q_DECLARE_TR_FUNCTIONS(PartTexts)

	public:
			/// A piece of data of a part, which a text may show
		struct Field
		{
			QString manufacturer;
			QString order_number;
			QString id;
			QString name;
			QString value;

				/// What a text saves to find it again
			QString reference() const;
		};

		static QString reference(const QString &manufacturer,
								 const QString &order_number,
								 const QString &id);
		static bool decode(const QString &reference, QString *manufacturer,
						   QString *order_number, QString *id);
			/// The name of the data @p id, as the user reads it
		static QString fieldName(const QString &id);
			/// "Rated voltage, K-24": what a text reads
		static QString describe(const QString &reference);

		static QString valueOf(const Part &part, const QString &id);
			/// Every piece of data @p part has a value for
		static QList<Field> fieldsOf(const Part &part);
			/// Every piece of data of every part of @p element; the parts
			/// not in @p database are left out
		static QList<Field> fieldsOf(const Element *element,
									 const PartsDatabase &database);
			/// True when @p element is made of at least one part
		static bool hasParts(const Element *element);

			/// Read again the data the texts of @p elements show, from
			/// the parts database when it is used. Nothing is opened when
			/// none of them shows data of a part.
			/// @return how many texts changed
		static int refresh(const QList<Element *> &elements);
};

#endif // PARTTEXTS_H
