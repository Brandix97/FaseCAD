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
#include "partscollectionitem.h"

#include "elementslocation.h"
#include "../partsdb/partsdatabase.h"
#include "../qeticons.h"

#include <QCoreApplication>
#include <QMap>
#include <QPixmap>

namespace {

QString tr(const char *text)
{
	return QCoreApplication::translate("PartsCollectionItem", text);
}

	/// The picture of a part, from its first image, or a null icon
QIcon pictureOf(const PartsDatabase &database, qint64 part_id)
{
	const QList<PartFile> files = database.files(part_id);
	for (const PartFile &file : files) {
		if (file.kind != PartFileKind::Image) {
			continue;
		}
		QPixmap pixmap;
		if (pixmap.loadFromData(database.fileContent(file.id))) {
			return QIcon(pixmap.scaled(48, 48, Qt::KeepAspectRatio,
									   Qt::SmoothTransformation));
		}
	}
	return QIcon();
}

}

PartsCollectionItem::PartsCollectionItem() :
	PartsCollectionItem(Root)
{
	setText(tr("Parts collection"));
	setIcon(QET::Icons::DocumentSpreadsheet);
	setToolTip(tr("The parts of the parts database. Drag one onto the"
				  " folio to place its symbol with the part, or onto a"
				  " symbol to add the part to it."));
	reload();
}

PartsCollectionItem::PartsCollectionItem(Kind kind) :
	m_kind(kind)
{
	setFlags(kind == PartItem
			 ? Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled
			 : Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}

bool PartsCollectionItem::isElement() const
{
	return m_kind == PartItem && !m_symbols.isEmpty();
}

/**
	@brief PartsCollectionItem::collectionPath
	@return "parts://", "parts://<category>" or "parts://<category>/<id>":
	no symbol of any collection has such a path, so nothing takes a part
	for the symbol it is placed with
*/
QString PartsCollectionItem::collectionPath() const
{
	switch (m_kind) {
		case Root:
			return QStringLiteral("parts://");
		case Category:
			return QStringLiteral("parts://") + m_category;
		case PartItem:
			return QStringLiteral("parts://%1/%2").arg(m_category).arg(m_id);
	}
	return QString();
}

/**
	@brief PartsCollectionItem::reload
	The root reads the parts database again: one child per category,
	and in each the parts of that category.
*/
void PartsCollectionItem::reload()
{
	if (m_kind != Root) {
		return;
	}
	removeRows(0, rowCount());

	PartsDatabase database;
	if (!database.open(PartsDatabase::path())) {
		return;
	}

		//The order of the parts manager: parts() gives them by
		//manufacturer and order number, the categories by name
	QMap<QString, QList<Part>> by_category;
	QList<Part> without_category;
	const QList<Part> listed = database.parts();
	for (const Part &row : listed) {
			//parts() gives the columns only: the symbols are a table of
			//their own
		const Part part = database.part(row.id).value_or(row);
		if (part.category.isEmpty()) {
			without_category << part;
		} else {
			by_category[part.category] << part;
		}
	}

	auto addCategory = [&](const QString &category, const QString &title,
						   const QList<Part> &list) {
		auto *folder = new PartsCollectionItem(Category);
		folder->m_category = category;
		folder->setText(title);
		folder->setIcon(QET::Icons::Folder);
		folder->setData(title);
		for (const Part &part : list) {
			auto *item = new PartsCollectionItem(PartItem);
			item->m_category = category;
			item->m_id = part.id;
			item->m_symbols = part.symbols;
			item->m_manufacturer = part.manufacturer;
			item->m_order_number = part.order_number;
			const QString order = part.order_number.isEmpty()
					? part.description : part.order_number;
			item->setText(part.manufacturer.isEmpty()
						  ? order
						  : part.manufacturer + QLatin1Char(' ') + order);

			QStringList tip{item->text()};
			if (!part.description.isEmpty()) {
				tip << part.description;
			}
			if (!part.designation.isEmpty()) {
				tip << part.designation;
			}
			if (part.symbols.isEmpty()) {
				tip << tr("No symbol: drop it onto a placed symbol");
			} else {
				QStringList names;
				for (const QString &symbol : part.symbols) {
					names << ElementsLocation(symbol).name();
				}
				tip << tr("Symbols: %1").arg(names.join(QStringLiteral(", ")));
				if (part.symbols.size() > 1) {
					tip << tr("Tab changes the symbol while the part is placed");
				}
			}
			item->setToolTip(tip.join(QLatin1Char('\n')));

				//What the search of the panel looks through
			item->setData(QStringList{part.manufacturer, part.order_number,
									  part.description, part.designation,
									  part.category, part.internal_reference,
									  part.supplier_reference}
						  .join(QLatin1Char(' ')));

			QIcon icon = pictureOf(database, part.id);
			if (icon.isNull() && !part.symbols.isEmpty()) {
				icon = ElementsLocation(part.symbols.first()).icon();
			}
			if (!icon.isNull()) {
				item->setIcon(icon);
			}
			folder->appendRow(item);
		}
		appendRow(folder);
	};

	const QStringList categories = database.categories();
	for (const QString &category : categories) {
		if (by_category.contains(category)) {
			addCategory(category, category, by_category.value(category));
		}
	}
	if (!without_category.isEmpty()) {
		addCategory(QString(), tr("(No category)"), without_category);
	}
}
