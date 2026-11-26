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
#include "partsplacement.h"

#include "partsdatabase.h"
#include "partslinks.h"
#include "partsmaterial.h"
#include "../materiallist/materiallist.h"
#include "../qetgraphicsitem/element.h"
#include "../qetinformation.h"
#include "../undocommand/changeelementinformationcommand.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>

QString PartsPlacement::mimeType()
{
	return QStringLiteral("application/x-fasecad-part");
}

QByteArray PartsPlacement::encode(const Key &key)
{
	QJsonObject object;
	object.insert(QStringLiteral("manufacturer"), key.manufacturer);
	object.insert(QStringLiteral("order_number"), key.order_number);
	object.insert(QStringLiteral("symbols"), QJsonArray::fromStringList(key.symbols));
	return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

std::optional<PartsPlacement::Key> PartsPlacement::decode(const QMimeData *data)
{
	if (!data || !data->hasFormat(mimeType())) {
		return std::nullopt;
	}
	const QJsonObject object =
			QJsonDocument::fromJson(data->data(mimeType())).object();
	Key key{object.value(QStringLiteral("manufacturer")).toString(),
			object.value(QStringLiteral("order_number")).toString(), {}};
	const QJsonArray symbols = object.value(QStringLiteral("symbols")).toArray();
	for (const QJsonValue &symbol : symbols) {
		key.symbols << symbol.toString();
	}
	if (key.order_number.isEmpty()) {
		return std::nullopt;
	}
	return key;
}

/**
	@brief PartsPlacement::addPart
	Add the part @p key to the Parts tab of @p element (one unit more when
	it is there already), as a child of @p parent so it is undone with
	it. With @p with_article, a symbol with no part yet also takes the
	article of the part in its information, as the Parts tab writes it.
*/
void PartsPlacement::addPart(Element *element, const Key &key,
							 bool with_article, QUndoCommand *parent)
{
	if (!element) {
		return;
	}
	const DiagramContext before = element->elementInformations();
	DiagramContext after = before;

	QList<PartsLinks::Link> links = PartsLinks::decode(
				before.value(PartsLinks::infoKey()).toString());
	const bool first = links.isEmpty();
	bool found = false;
	for (PartsLinks::Link &link : links) {
		if (link.manufacturer.compare(key.manufacturer, Qt::CaseInsensitive) == 0
			&& link.order_number.compare(key.order_number, Qt::CaseInsensitive) == 0) {
			++link.units;
			found = true;
			break;
		}
	}
	if (!found) {
		links.append({key.manufacturer, key.order_number, 1});
	}
	after.addValue(PartsLinks::infoKey(), PartsLinks::encode(links));

	if (with_article && first) {
		PartsDatabase database;
		if (database.open(PartsDatabase::path())) {
			const qint64 id = database.findPart(key.manufacturer, key.order_number);
			if (const std::optional<Part> part = database.part(id)) {
				const MaterialRecord record = PartsMaterial::record(*part);
				for (const QString &column : MaterialList::columnsForBlock(0)) {
					if (!record.values.contains(column)) {
						continue;
					}
					const QString value = record.value(column);
					if (value.isEmpty() && !MaterialList::isArticleBound(column)) {
						continue;
					}
					const QString info = MaterialList::elementInfoKey(column, 0);
					if (!info.isEmpty()
						&& QETInformation::elementInfoKeys().contains(info)) {
						after.addValue(info, value);
					}
				}
			}
		}
	}

	if (after != before) {
		new ChangeElementInformationCommand(element, before, after, parent);
	}
}

/**
	@brief PartsPlacement::preparer
	@return what DiagramEventAddElement::setPrepare() takes for a symbol
	placed with the part @p key: each copy placed gets the part and its
	article
*/
std::function<void(Element *, QUndoCommand *)> PartsPlacement::preparer(const Key &key)
{
	return [key](Element *element, QUndoCommand *undo) {
		addPart(element, key, true, undo);
	};
}
