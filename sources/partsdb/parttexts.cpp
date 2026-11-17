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
#include "parttexts.h"

#include "partsdatabase.h"
#include "partslinks.h"
#include "../qetgraphicsitem/dynamicelementtextitem.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/elementtextitemgroup.h"

#include <QHash>

#include <optional>

namespace
{
	const QString data_prefix = QStringLiteral("data:");

		/// The fields of a part a text may show, in the order they are
		/// offered
	struct FieldName
	{
		const char *id;
		const char *name;
	};
	const FieldName field_names[] = {
		{"description", QT_TRANSLATE_NOOP("PartTexts", "Textual description")},
		{"designation", QT_TRANSLATE_NOOP("PartTexts", "Article number")},
		{"manufacturer", QT_TRANSLATE_NOOP("PartTexts", "Manufacturer")},
		{"order_number", QT_TRANSLATE_NOOP("PartTexts", "Order number")},
		{"internal_reference", QT_TRANSLATE_NOOP("PartTexts", "Internal number")},
		{"supplier", QT_TRANSLATE_NOOP("PartTexts", "Supplier")},
		{"supplier_reference", QT_TRANSLATE_NOOP("PartTexts", "Supplier reference")},
		{"category", QT_TRANSLATE_NOOP("PartTexts", "Category")},
		{"model", QT_TRANSLATE_NOOP("PartTexts", "Model")},
		{"voltage", QT_TRANSLATE_NOOP("PartTexts", "Rated voltage")},
		{"current", QT_TRANSLATE_NOOP("PartTexts", "Rated current")},
		{"unit", QT_TRANSLATE_NOOP("PartTexts", "Unity")},
		{"code_letter", QT_TRANSLATE_NOOP("PartTexts", "Device letter")},
		{"gtin", QT_TRANSLATE_NOOP("PartTexts", "GTIN / EAN")},
		{"width", QT_TRANSLATE_NOOP("PartTexts", "Width (mm)")},
		{"height", QT_TRANSLATE_NOOP("PartTexts", "Height (mm)")},
		{"depth", QT_TRANSLATE_NOOP("PartTexts", "Depth (mm)")},
		{"weight", QT_TRANSLATE_NOOP("PartTexts", "Weight (kg)")},
		{"power_loss", QT_TRANSLATE_NOOP("PartTexts", "Power loss (W)")},
		{"price", QT_TRANSLATE_NOOP("PartTexts", "Price")},
		{"core_count", QT_TRANSLATE_NOOP("PartTexts", "Number of cores")},
		{"core_colors", QT_TRANSLATE_NOOP("PartTexts", "Core colors")},
		{"section", QT_TRANSLATE_NOOP("PartTexts", "Cross-section")},
		{"sheath_color", QT_TRANSLATE_NOOP("PartTexts", "Sheath color")},
		{"datasheet_url", QT_TRANSLATE_NOOP("PartTexts", "Datasheet")},
		{"notes", QT_TRANSLATE_NOOP("PartTexts", "Notes")}
	};

	QString number(const std::optional<double> &value)
	{
		return value ? QString::number(*value) : QString();
	}

		/// Every text of @p element, the ones of its groups too
	QList<DynamicElementTextItem *> textsOf(const Element *element)
	{
		QList<DynamicElementTextItem *> texts = element->dynamicTextItems();
		for (ElementTextItemGroup *group : element->textGroups()) {
			texts << group->texts();
		}
		return texts;
	}
}

QString PartTexts::Field::reference() const
{
	return PartTexts::reference(manufacturer, order_number, id);
}

QString PartTexts::reference(const QString &manufacturer,
							 const QString &order_number,
							 const QString &id)
{
	return manufacturer + QLatin1Char('\t') + order_number
			+ QLatin1Char('\t') + id;
}

bool PartTexts::decode(const QString &reference, QString *manufacturer,
					   QString *order_number, QString *id)
{
	const QStringList parts = reference.split(QLatin1Char('\t'));
	if (parts.size() != 3 || parts.at(2).isEmpty()) {
		return false;
	}
	if (manufacturer) *manufacturer = parts.at(0);
	if (order_number) *order_number = parts.at(1);
	if (id) *id = parts.at(2);
	return true;
}

QString PartTexts::fieldName(const QString &id)
{
	if (id.startsWith(data_prefix)) {
		return id.mid(data_prefix.size());
	}
	for (const FieldName &field : field_names) {
		if (id == QLatin1String(field.id)) {
			return QCoreApplication::translate("PartTexts", field.name);
		}
	}
	return id;
}

QString PartTexts::describe(const QString &reference)
{
	QString manufacturer, order_number, id;
	if (!decode(reference, &manufacturer, &order_number, &id)) {
		return QString();
	}
	return tr("%1, %2", "the data a text shows (rated voltage...), then the"
					   " order number of the part").arg(fieldName(id), order_number);
}

QString PartTexts::valueOf(const Part &part, const QString &id)
{
	if (id.startsWith(data_prefix))
	{
		const QString name = id.mid(data_prefix.size());
		for (const auto &line : part.technical_data) {
			if (line.first == name) {
				return line.second;
			}
		}
		return QString();
	}

	if (id == QLatin1String("description")) return part.description;
	if (id == QLatin1String("designation")) return part.designation;
	if (id == QLatin1String("manufacturer")) return part.manufacturer;
	if (id == QLatin1String("order_number")) return part.order_number;
	if (id == QLatin1String("internal_reference")) return part.internal_reference;
	if (id == QLatin1String("supplier")) return part.supplier;
	if (id == QLatin1String("supplier_reference")) return part.supplier_reference;
	if (id == QLatin1String("category")) return part.category;
	if (id == QLatin1String("model")) return part.model;
	if (id == QLatin1String("voltage")) return part.voltage;
	if (id == QLatin1String("current")) return part.current;
	if (id == QLatin1String("unit")) return part.unit;
	if (id == QLatin1String("code_letter")) return part.code_letter;
	if (id == QLatin1String("gtin")) return part.gtin;
	if (id == QLatin1String("width")) return number(part.width);
	if (id == QLatin1String("height")) return number(part.height);
	if (id == QLatin1String("depth")) return number(part.depth);
	if (id == QLatin1String("weight")) return number(part.weight);
	if (id == QLatin1String("power_loss")) return number(part.power_loss);
	if (id == QLatin1String("price")) return number(part.price);
	if (id == QLatin1String("core_count")) {
		return part.core_count > 0 ? QString::number(part.core_count) : QString();
	}
	if (id == QLatin1String("core_colors")) return part.core_colors;
	if (id == QLatin1String("section")) return part.section;
	if (id == QLatin1String("sheath_color")) return part.sheath_color;
	if (id == QLatin1String("datasheet_url")) return part.datasheet_url;
	if (id == QLatin1String("notes")) return part.notes;
	return QString();
}

QList<PartTexts::Field> PartTexts::fieldsOf(const Part &part)
{
	QList<Field> fields;
	auto add = [&fields, &part](const QString &id, const QString &name) {
		const QString value = valueOf(part, id);
		if (value.isEmpty()) {
			return;
		}
		Field field;
		field.manufacturer = part.manufacturer;
		field.order_number = part.order_number;
		field.id = id;
		field.name = name;
		field.value = value;
		fields << field;
	};

	for (const FieldName &field : field_names) {
		add(QLatin1String(field.id),
			QCoreApplication::translate("PartTexts", field.name));
	}
	QStringList seen;
	for (const auto &line : part.technical_data) {
		if (line.first.isEmpty() || seen.contains(line.first)) {
			continue;
		}
		seen << line.first;
		add(data_prefix + line.first, line.first);
	}
	return fields;
}

QList<PartTexts::Field> PartTexts::fieldsOf(const Element *element,
											const PartsDatabase &database)
{
	QList<Field> fields;
	if (!element) {
		return fields;
	}
	const QList<PartsLinks::Link> links = PartsLinks::decode(
				element->elementInformations().value(PartsLinks::infoKey()).toString());
	for (const PartsLinks::Link &link : links) {
		if (const std::optional<Part> part = database.part(
					database.findPart(link.manufacturer, link.order_number))) {
			fields << fieldsOf(*part);
		}
	}
	return fields;
}

bool PartTexts::hasParts(const Element *element)
{
	return element && !PartsLinks::decode(
				element->elementInformations().value(PartsLinks::infoKey()).toString()).isEmpty();
}

int PartTexts::refresh(const QList<Element *> &elements)
{
	QList<DynamicElementTextItem *> texts;
	for (const Element *element : elements) {
		if (!element) {
			continue;
		}
		for (DynamicElementTextItem *text : textsOf(element)) {
			if (text->textFrom() == DynamicElementTextItem::PartInfo) {
				texts << text;
			}
		}
	}
	if (texts.isEmpty() || !PartsDatabase::isEnabled()) {
		return 0;
	}

	PartsDatabase database;
	if (!database.open(PartsDatabase::path())) {
		return 0;
	}

		//One reading per part, however many texts show it
	QHash<QString, std::optional<Part>> read;
	int changed = 0;
	for (DynamicElementTextItem *text : std::as_const(texts))
	{
		QString manufacturer, order_number, id;
		if (!decode(text->partField(), &manufacturer, &order_number, &id)) {
			continue;
		}
		const QString key = manufacturer + QLatin1Char('\t') + order_number;
		if (!read.contains(key)) {
			read.insert(key, database.part(database.findPart(manufacturer, order_number)));
		}
		const std::optional<Part> part = read.value(key);
			//A part no longer in the database: the text keeps what it
			//showed
		if (!part) {
			continue;
		}
		const QString value = valueOf(*part, id);
		if (value != text->partValue()) {
			text->setPartValue(value);
			++changed;
		}
	}
	return changed;
}
