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
#include "partsmaterial.h"

#include <QLocale>

namespace {

QString numberText(const std::optional<double> &value)
{
	return value ? QLocale::c().toString(*value, 'g', 12) : QString();
}

} // namespace

MaterialRecord PartsMaterial::record(const Part &part,
									 std::optional<double> quantity)
{
	MaterialRecord record;
	record.setValue(QStringLiteral("description"), part.description);
	record.setValue(QStringLiteral("designation"), part.designation);
	record.setValue(QStringLiteral("manufacturer"), part.manufacturer);
	record.setValue(QStringLiteral("manufacturer_reference"),
					part.order_number);
	record.setValue(QStringLiteral("machine_manufacturer_reference"),
					part.internal_reference);
	record.setValue(QStringLiteral("supplier"), part.supplier);
	record.setValue(QStringLiteral("model"), part.model);
	record.setValue(QStringLiteral("category"), part.category);
	record.setValue(QStringLiteral("voltage_rating"), part.voltage);
	record.setValue(QStringLiteral("current_rating"), part.current);
	record.setValue(QStringLiteral("width"), numberText(part.width));
	record.setValue(QStringLiteral("height"), numberText(part.height));
	record.setValue(QStringLiteral("depth"), numberText(part.depth));
		//Not the article's own: written only when the part says something
	if (!part.unit.isEmpty()) {
		record.setValue(QStringLiteral("unity"), part.unit);
	}
	if (!part.notes.isEmpty()) {
		record.setValue(QStringLiteral("notes"), part.notes);
	}
	if (quantity) {
		record.setValue(QStringLiteral("quantity"), numberText(quantity));
	}
	return record;
}
