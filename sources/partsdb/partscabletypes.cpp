/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "partscabletypes.h"

#include <QRegularExpression>

QString PartsCableTypes::sourceColumn()
{
	return QStringLiteral("source");
}

/**
	@brief PartsCableTypes::columns
	@return the columns of the cable type file a part fills in, the
	three a cable type needs first, then the source column
*/
QStringList PartsCableTypes::columns()
{
	return {
		QStringLiteral("designation"),
		QStringLiteral("cores"),
		QStringLiteral("core_colors"),
		QStringLiteral("section"),
		QStringLiteral("color"),
		QStringLiteral("manufacturer"),
		QStringLiteral("manufacturer_reference"),
		QStringLiteral("supplier"),
		QStringLiteral("description"),
		sourceColumn()
	};
}

/**
	@brief PartsCableTypes::typeName
	@return the name a cable of this part gets as its type : the article
	number (e.g. H07RN-F 5G2,5), else the model, else the order number
*/
QString PartsCableTypes::typeName(const Part &part)
{
	for (const QString &name : {part.designation, part.model,
								part.order_number}) {
		if (!name.trimmed().isEmpty()) {
			return name.trimmed();
		}
	}
	return QString();
}

/**
	@brief PartsCableTypes::records
	@return one line for every part with cores and a name, in the order
	of @p parts. The colours are written with |, the way the cable type
	file writes them.
*/
QList<CableTypeRecord> PartsCableTypes::records(const QList<Part> &parts)
{
	QList<CableTypeRecord> result;
	for (const Part &part : parts) {
		const QString name = typeName(part);
		if (part.core_count <= 0 || name.isEmpty()) {
			continue;
		}
		QStringList colors;
		for (QString color : part.core_colors.split(
				 QRegularExpression(QStringLiteral("[|;,]")),
				 Qt::SkipEmptyParts)) {
			color = color.trimmed();
			if (!color.isEmpty()) {
				colors << color;
			}
		}

		CableTypeRecord record;
		record.setValue(QStringLiteral("designation"), name);
		record.setValue(QStringLiteral("cores"),
						QString::number(part.core_count));
		record.setValue(QStringLiteral("core_colors"),
						colors.join(QLatin1Char('|')));
		record.setValue(QStringLiteral("section"), part.section);
		record.setValue(QStringLiteral("color"), part.sheath_color);
		record.setValue(QStringLiteral("manufacturer"), part.manufacturer);
		record.setValue(QStringLiteral("manufacturer_reference"),
						part.order_number);
		record.setValue(QStringLiteral("supplier"), part.supplier);
		record.setValue(QStringLiteral("description"), part.description);
		record.setValue(sourceColumn(), tr("Parts database"));
		result.append(record);
	}
	return result;
}

bool PartsCableTypes::isFromPartsDatabase(const CableTypeRecord &record)
{
	return !record.value(sourceColumn()).isEmpty();
}
