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
#include "wiresections.h"

#include "QPropertyUndoCommand/qpropertyundocommand.h"
#include "conductorproperties.h"
#include "diagram.h"
#include "partsdb/partsdatabase.h"
#include "partsdb/partslinks.h"
#include "properties/elementdata.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminal.h"
#include "qetinformation.h"
#include "qetproject.h"

#include <QLocale>
#include <QRegularExpression>
#include <QSet>
#include <QUndoCommand>

#include <optional>

namespace
{
		/// IEC 60204-1 table 6: copper, PVC, 40 °C, one balanced
		/// three-phase circuit -- the same table as the website's
		/// calculator (web/fuente/datos_cables.py)
	const QList<qreal> table_sections{0.75, 1, 1.5, 2.5, 4, 6, 10, 16, 25, 35, 50, 70, 95, 120};
	const QList<qreal> iz_b1{8.6, 10.3, 13.5, 18.3, 24, 31, 44, 59, 77, 96, 117, 149, 180, 208};
	const QList<qreal> iz_b2{8.5, 10.1, 13.1, 17.4, 23, 30, 40, 54, 70, 86, 103, 130, 156, 179};
	const QList<qreal> iz_c{9.8, 11.7, 15.2, 21, 28, 36, 50, 66, 84, 104, 125, 160, 194, 225};
	const QList<qreal> iz_e{10.4, 12.4, 16.1, 22, 30, 37, 52, 70, 88, 110, 133, 171, 207, 240};
		/// Table D.1: ambient temperature
	const QList<int> temperatures{30, 35, 40, 45, 50, 55, 60};
	const QList<qreal> k_temperature{1.15, 1.08, 1.00, 0.91, 0.82, 0.71, 0.58};
		/// Table D.2: circuits grouped
	const QList<int> groups{1, 2, 4, 6, 9};
	const QList<qreal> k_group_b{1.0, 0.80, 0.65, 0.57, 0.50};
	const QList<qreal> k_group_c{1.0, 0.85, 0.75, 0.72, 0.70};
	const QList<qreal> k_group_e{1.0, 0.88, 0.77, 0.73, 0.72};

	const QList<qreal> &izOf(WireSections::Method method)
	{
		switch (method) {
			case WireSections::Method::B2: return iz_b2;
			case WireSections::Method::C: return iz_c;
			case WireSections::Method::E: return iz_e;
			case WireSections::Method::B1: break;
		}
		return iz_b1;
	}

		/// The factor of the first row of the table at or over @p value:
		/// the side the wire is safe on
	template <typename T>
	qreal factorAtOrOver(const QList<T> &rows, const QList<qreal> &factors, T value)
	{
		for (int i = 0 ; i < rows.size() ; ++i) {
			if (rows.at(i) >= value) {
				return factors.at(i);
			}
		}
		return factors.last();
	}

	int leadingNumber(const QString &name, bool *found)
	{
		static const QRegularExpression rx(QStringLiteral("^\\s*(\\d+)"));
		const QRegularExpressionMatch match = rx.match(name);
		*found = match.hasMatch();
		return *found ? match.captured(1).toInt() : 0;
	}
}

QList<qreal> WireSections::sections()
{
	return table_sections;
}

qreal WireSections::capacity(qreal section, const Conditions &conditions)
{
	const int i = table_sections.indexOf(section);
	if (i < 0) {
		return 0;
	}
	const QList<qreal> &k_group = conditions.method == Method::C ? k_group_c
			: conditions.method == Method::E ? k_group_e : k_group_b;
	return izOf(conditions.method).at(i)
			* factorAtOrOver(temperatures, k_temperature, conditions.ambient)
			* factorAtOrOver(groups, k_group, qMax(1, conditions.grouped));
}

qreal WireSections::minimumSection(qreal current, const Conditions &conditions)
{
	if (current <= 0) {
		return 0;
	}
	for (const qreal section : table_sections) {
		if (capacity(section, conditions) >= current) {
			return section;
		}
	}
	return 0;
}

qreal WireSections::parseSection(const QString &text)
{
	static const QRegularExpression rx(QStringLiteral("(\\d+(?:[.,]\\d+)?)"));
	const QRegularExpressionMatch match = rx.match(text);
	if (!match.hasMatch()) {
		return 0;
	}
	QString number = match.captured(1);
	number.replace(QLatin1Char(','), QLatin1Char('.'));
	return number.toDouble();
}

QString WireSections::sectionText(qreal section)
{
	return QLocale().toString(section, 'g', 4) + QStringLiteral(" mm²");
}

qreal WireSections::parseCurrent(const QString &text)
{
	return parseSection(text);
}

bool WireSections::isProtection(const Element *element)
{
	if (!element) {
		return false;
	}
	const ElementData data = element->elementData();
	return data.m_type == ElementData::Master && data.m_master_type == ElementData::Protection;
}

/**
	@brief WireSections::ratedCurrent
	@return the rated current of the protection @p element: its information
	"Rated current", else the rated current of its first part in the parts
	database; 0 when it says none
*/
qreal WireSections::ratedCurrent(const Element *element)
{
	if (!element) {
		return 0;
	}
	const DiagramContext information = element->elementInformations();
	const qreal current = parseCurrent(
				information.value(QETInformation::ELMT_CURRENT_RATING).toString());
	if (current > 0 || !PartsDatabase::isEnabled()) {
		return current;
	}
	const QList<PartsLinks::Link> links = PartsLinks::decode(
				information.value(PartsLinks::infoKey()).toString());
	if (links.isEmpty()) {
		return 0;
	}
	PartsDatabase database;
	if (!database.open(PartsDatabase::path())) {
		return 0;
	}
	const std::optional<Part> part = database.part(
				database.findPart(links.first().manufacturer, links.first().order_number));
	return part ? parseCurrent(part->current) : 0;
}

/**
	@brief WireSections::outgoing
	@return the terminals of @p element the current goes out of: those with
	an even number (2, 4, 6, 2/T1...), or, when its terminals have no
	number, those in the lower half of the symbol
*/
QList<Terminal *> WireSections::outgoing(const Element *element)
{
	QList<Terminal *> list;
	if (!element) {
		return list;
	}
	bool any_number = false;
	for (Terminal *terminal : element->terminals()) {
		bool found = false;
		const int number = leadingNumber(terminal->name(), &found);
		if (found) {
			any_number = true;
			if (number % 2 == 0) {
				list << terminal;
			}
		}
	}
	if (any_number) {
		return list;
	}
	const qreal middle = element->sceneBoundingRect().center().y();
	for (Terminal *terminal : element->terminals()) {
		if (terminal->dockConductor().y() > middle) {
			list << terminal;
		}
	}
	return list;
}

qreal WireSections::maxCurrent(const Element *element, const PartsDatabase *database)
{
	if (!element || !database || !database->isOpen()) {
		return 0;
	}
	qreal highest = 0;
	const QList<PartsLinks::Link> links = PartsLinks::decode(
				element->elementInformations().value(PartsLinks::infoKey()).toString());
	for (const PartsLinks::Link &link : links) {
		const std::optional<Part> part = database->part(
					database->findPart(link.manufacturer, link.order_number));
		if (part && part->max_current) {
			highest = qMax(highest, qreal(*part->max_current));
		}
	}
	return highest;
}

QList<WireSections::Row> WireSections::check(QETProject *project, const Conditions &conditions)
{
	QList<Row> rows;
	if (!project) {
		return rows;
	}
		//The maximum current of the parts, one opening for them all
	PartsDatabase database;
	if (PartsDatabase::isEnabled()) {
		database.open(PartsDatabase::path());
	}
	for (Diagram *sheet : project->diagrams())
	{
		if (sheet->isCabinetFolio() || sheet->isRoutingFolio()) {
			continue;
		}
		sheet->ensureLoaded();
		for (Element *element : sheet->elements())
		{
			const qreal maximum = maxCurrent(element, &database);
			if (!isProtection(element) && maximum <= 0) {
				continue;
			}
			Row row;
			row.protection = element;
			row.label = element->actualLabel();
			row.current = maximum > 0 ? maximum : ratedCurrent(element);
			row.required = minimumSection(row.current, conditions);
			QSet<Conductor *> seen;
			for (Terminal *terminal : outgoing(element)) {
				for (Conductor *conductor : terminal->conductors()) {
					QSet<Conductor *> potential = conductor->relatedPotentialConductors(true);
					potential.insert(conductor);
					for (Conductor *wire : potential) {
						if (!seen.contains(wire)) {
							seen.insert(wire);
							row.wires << wire;
						}
					}
				}
			}
			for (const QPointer<Conductor> &wire : std::as_const(row.wires)) {
				const qreal section = parseSection(wire->properties().m_wire_section);
				if (section <= 0) {
					++row.without;
				} else if (row.required > 0 && section < row.required) {
					++row.too_small;
				}
			}
			rows << row;
		}
	}
	return rows;
}

QUndoCommand *WireSections::apply(const QList<Row> &rows)
{
	auto *undo = new QUndoCommand();
	int wires = 0;
	for (const Row &row : rows)
	{
		if (row.required <= 0) {
			continue;
		}
		for (const QPointer<Conductor> &wire : row.wires)
		{
			if (!wire) {
				continue;
			}
			ConductorProperties properties = wire->properties();
			if (parseSection(properties.m_wire_section) >= row.required) {
				continue;
			}
			QVariant old_value;
			old_value.setValue(properties);
			properties.m_wire_section = sectionText(row.required);
			QVariant new_value;
			new_value.setValue(properties);
			new QPropertyUndoCommand(wire.data(), "properties", old_value, new_value, undo);
			++wires;
		}
	}
	if (!wires) {
		delete undo;
		return nullptr;
	}
	undo->setText(QCoreApplication::translate("WireSections",
											  "Give %n wire(s) the section of their protection",
											  "", wires));
	return undo;
}
