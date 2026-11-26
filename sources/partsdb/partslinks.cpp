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
#include "partslinks.h"

#include <QLocale>
#include <QStringList>

namespace {

	///A tab or a line break typed in a field would break the line
QString clean(QString text)
{
	text.replace(QLatin1Char('\t'), QLatin1Char(' '));
	text.replace(QLatin1Char('\n'), QLatin1Char(' '));
	text.replace(QLatin1Char('\r'), QLatin1Char(' '));
	return text.trimmed();
}

} // namespace

QString PartsLinks::encode(const QList<Link> &links)
{
	QStringList lines;
	for (const Link &link : links) {
		lines << clean(link.manufacturer) + QLatin1Char('\t')
				 + clean(link.order_number) + QLatin1Char('\t')
				 + QLocale::c().toString(link.units, 'g', 12);
	}
	return lines.join(QLatin1Char('\n'));
}

/**
	@brief PartsLinks::decode
	@return the parts of @p value; a line without an order number says
	nothing and is left out, and units which are no number count one.
*/
QList<PartsLinks::Link> PartsLinks::decode(const QString &value)
{
	QList<Link> links;
	for (const QString &line : value.split(QLatin1Char('\n'),
										   Qt::SkipEmptyParts)) {
		const QStringList cells = line.split(QLatin1Char('\t'));
		Link link;
		link.manufacturer = cells.value(0).trimmed();
		link.order_number = cells.value(1).trimmed();
		if (link.order_number.isEmpty()) {
			continue;
		}
		bool ok = false;
		const double units = QLocale::c().toDouble(cells.value(2).trimmed(),
												   &ok);
		link.units = ok && units > 0 ? units : 1;
		links << link;
	}
	return links;
}
