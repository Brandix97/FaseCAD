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
#ifndef PARTSCOLLECTIONITEM_H
#define PARTSCOLLECTIONITEM_H

#include "elementcollectionitem.h"

/**
	@brief The PartsCollectionItem class is the "Parts collection" of the
	collection panel: the parts of the parts database, by category, in
	the order of the parts manager (categories by name, a part without
	one last; the parts of a category by manufacturer and order number).

	A part with symbols (Part::symbols) is dragged or placed as the first
	of them, with the part (PartsPlacement), Tab going to the next one; a
	part without any is only dropped onto a symbol already placed. The
	tree is read again when the parts manager writes the database
	(reload()).
*/
class PartsCollectionItem : public ElementCollectionItem
{
	public:
		enum Kind {Root, Category, PartItem};
		enum {Type = UserType + 20};
		int type() const override {return Type;}

		PartsCollectionItem();
		void reload();

		Kind kind() const {return m_kind;}
		bool isDir() const override {return m_kind != PartItem;}
		bool isElement() const override;
		QString localName() override {return text();}
		QString name() const override {return text();}
		QString collectionPath() const override;
		bool isCollectionRoot() const override {return m_kind == Root;}
		void addChildAtPath(const QString &) override {}
		void setUpData() override {}
		void setUpIcon() override {}

			/// The symbols of the part, as ElementsLocation strings
		QStringList symbols() const {return m_symbols;}
		QString manufacturer() const {return m_manufacturer;}
		QString orderNumber() const {return m_order_number;}

	private:
		explicit PartsCollectionItem(Kind kind);

		Kind m_kind = Root;
		QString m_category;
		qint64 m_id = 0;
		QStringList m_symbols;
		QString m_manufacturer;
		QString m_order_number;
};

#endif // PARTSCOLLECTIONITEM_H
