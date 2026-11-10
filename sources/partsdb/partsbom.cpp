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
#include "partsbom.h"

#include "partsdatabase.h"
#include "partslinks.h"
#include "../bomexport.h"
#include "../dataBase/projectdatabase.h"
#include "../qet.h"
#include "../qetproject.h"
#include "../ui/devicespanel.h"

#include <QCoreApplication>
#include <QSqlQuery>
#include <QUuid>

#include <algorithm>
#include <optional>

namespace {

	/// The information keys of an article: the main one, or an auxiliary
	/// one (1 to 4)
QString articleKey(const QString &key, int block)
{
	return block == 0 ? key
					  : key + QStringLiteral("_auxiliary%1").arg(block);
}

	/// A quantity typed into the information: a comma is a decimal point
	/// too; nothing, or no positive number, is one
double quantityOf(const QString &text)
{
	QString value = text.trimmed();
	value.replace(QLatin1Char(','), QLatin1Char('.'));
	bool ok = false;
	const double quantity = value.toDouble(&ok);
	return ok && quantity > 0 ? quantity : 1;
}

QString numberText(double value)
{
	return QString::number(value, 'g', 12);
}

	/// One line of the list, while it is being added up
struct Line
{
	QMap<QString, QString> values;
	double quantity = 0;
	QStringList devices;
};

	/// What one device brings to a line
struct Use
{
	QString manufacturer;
	QString order_number;
	double quantity = 1;
		/// What the device itself says of the part
	QMap<QString, QString> values;
};

QString groupKey(const Use &use)
{
	if (!use.order_number.isEmpty()) {
		return use.manufacturer.toCaseFolded() + QLatin1Char('\t')
				+ use.order_number.toCaseFolded();
	}
		//An article with no order number is grouped by what it says
	return QStringLiteral("\t\t") + use.manufacturer.toCaseFolded()
			+ QLatin1Char('\t')
			+ use.values.value(QStringLiteral("designation")).toCaseFolded()
			+ QLatin1Char('\t')
			+ use.values.value(QStringLiteral("description")).toCaseFolded();
}

	/// The uses of one device
QList<Use> usesOf(const DiagramContext &information)
{
	QList<Use> uses;
	const QList<PartsLinks::Link> links = PartsLinks::decode(
				information.value(PartsLinks::infoKey()).toString());
	if (!links.isEmpty()) {
		for (const PartsLinks::Link &link : links) {
			Use use;
			use.manufacturer = link.manufacturer.trimmed();
			use.order_number = link.order_number.trimmed();
			use.quantity = link.units;
			uses << use;
		}
		return uses;
	}

	for (int block = 0; block < 5; ++block) {
		auto value = [&](const char *key) {
			return information.value(articleKey(QLatin1String(key), block))
					.toString().trimmed();
		};
		Use use;
		use.manufacturer = value("manufacturer");
		use.order_number = value("manufacturer_reference");
		use.values.insert(QStringLiteral("designation"), value("designation"));
		use.values.insert(QStringLiteral("description"), value("description"));
		use.values.insert(QStringLiteral("supplier"), value("supplier"));
		use.values.insert(QStringLiteral("internal_reference"),
						  value("machine_manufacturer_reference"));
		use.values.insert(QStringLiteral("unit"), value("unity"));
		if (use.order_number.isEmpty()
				&& use.values.value(QStringLiteral("designation")).isEmpty()) {
			continue;
		}
		use.quantity = quantityOf(value("quantity"));
		uses << use;
	}
	return uses;
}

} // namespace

const QList<CableList::Column> &PartsBom::columns()
{
	static const QList<CableList::Column> list = {
		{QStringLiteral("pos"),
		 QCoreApplication::translate("PartsBom", "Pos.")},
		{QStringLiteral("quantity"),
		 QCoreApplication::translate("PartsBom", "Quantity")},
		{QStringLiteral("unit"),
		 QCoreApplication::translate("PartsBom", "Unit")},
		{QStringLiteral("manufacturer"),
		 QCoreApplication::translate("PartsBom", "Manufacturer")},
		{QStringLiteral("order_number"),
		 QCoreApplication::translate("PartsBom", "Order number")},
		{QStringLiteral("description"),
		 QCoreApplication::translate("PartsBom", "Description")},
		{QStringLiteral("designation"),
		 QCoreApplication::translate("PartsBom", "Designation")},
		{QStringLiteral("category"),
		 QCoreApplication::translate("PartsBom", "Category")},
		{QStringLiteral("supplier"),
		 QCoreApplication::translate("PartsBom", "Supplier")},
		{QStringLiteral("supplier_reference"),
		 QCoreApplication::translate("PartsBom", "Supplier reference")},
		{QStringLiteral("internal_reference"),
		 QCoreApplication::translate("PartsBom", "Internal reference")},
		{QStringLiteral("devices"),
		 QCoreApplication::translate("PartsBom", "Devices")}};
	return list;
}

const QStringList &PartsBom::defaultKeys()
{
	static const QStringList keys = {
		QStringLiteral("pos"), QStringLiteral("quantity"),
		QStringLiteral("unit"), QStringLiteral("manufacturer"),
		QStringLiteral("order_number"), QStringLiteral("description"),
		QStringLiteral("devices")};
	return keys;
}

QString PartsBom::labelOf(const QString &key)
{
	for (const CableList::Column &column : columns()) {
		if (column.key == key) {
			return column.label;
		}
	}
	return QString();
}

/**
	@brief PartsBom::devices
	@return the devices of @p project the list counts, read from the
	project database, in the order of the folios and of their cells
*/
QList<PartsBom::Device> PartsBom::devices(QETProject *project)
{
	QList<Device> result;
	if (!project || !project->dataBase()) {
		return result;
	}
	projectDataBase *database = project->dataBase();
	QSqlQuery query = database->newQuery(QStringLiteral(
		"SELECT e.uuid, ei.label "
		"FROM element e "
		"JOIN diagram d ON d.uuid = e.diagram_uuid "
		"LEFT JOIN element_info ei ON ei.element_uuid = e.uuid "
			//The kinds the nomenclature lists, but the cabinet
			//thumbnails: a thumbnail draws a device of a folio again
		"WHERE e.type IN ('simple', 'terminal', 'master', 'slave') "
		"ORDER BY d.pos, e.pos"));
	if (!query.exec()) {
		return result;
	}
	while (query.next()) {
		Device device;
		device.information = database->elementInformation(
					QUuid(query.value(0).toString()));
		if (QET::infoFlagIsTrue(device.information
								.value(QStringLiteral("exclude_from_bom"))
								.toString())) {
			continue;
		}
		device.label = query.value(1).toString().trimmed();
		result << device;
	}
	return result;
}

/**
	@brief PartsBom::rows
	@return the lines for @p devices: one per part, sorted by manufacturer
	and order number, numbered from 1
*/
QVector<QMap<QString, QString>> PartsBom::rows(const QList<Device> &devices,
											   const PartsDatabase *database)
{
	QMap<QString, Line> lines;
	QStringList order;
	for (const Device &device : devices) {
		for (const Use &use : usesOf(device.information)) {
			const QString key = groupKey(use);
			auto it = lines.find(key);
			if (it == lines.end()) {
				it = lines.insert(key, Line());
				order << key;
				it->values.insert(QStringLiteral("manufacturer"),
								  use.manufacturer);
				it->values.insert(QStringLiteral("order_number"),
								  use.order_number);
				std::optional<Part> part;
				if (database && database->isOpen()
						&& !use.order_number.isEmpty()) {
					part = database->part(database->findPart(
											  use.manufacturer,
											  use.order_number));
				}
				if (part) {
					it->values.insert(QStringLiteral("description"),
									  part->description);
					it->values.insert(QStringLiteral("designation"),
									  part->designation);
					it->values.insert(QStringLiteral("category"),
									  part->category);
					it->values.insert(QStringLiteral("supplier"),
									  part->supplier);
					it->values.insert(QStringLiteral("supplier_reference"),
									  part->supplier_reference);
					it->values.insert(QStringLiteral("internal_reference"),
									  part->internal_reference);
					it->values.insert(QStringLiteral("unit"), part->unit);
				}
			}
				//The device's own article fills what is still empty
			for (auto value = use.values.constBegin();
				 value != use.values.constEnd(); ++value) {
				if (it->values.value(value.key()).isEmpty()) {
					it->values.insert(value.key(), value.value());
				}
			}
			it->quantity += use.quantity;
			if (!device.label.isEmpty()
					&& !it->devices.contains(device.label)) {
				it->devices << device.label;
			}
		}
	}

	auto text = [](const Line &line, const char *key) {
		return line.values.value(QLatin1String(key));
	};
	std::stable_sort(order.begin(), order.end(),
					 [&](const QString &a, const QString &b) {
		const Line &first = lines[a];
		const Line &second = lines[b];
		const QString first_manufacturer = text(first, "manufacturer");
		const QString second_manufacturer = text(second, "manufacturer");
		if (first_manufacturer.isEmpty() != second_manufacturer.isEmpty()) {
			return second_manufacturer.isEmpty();
		}
		if (first_manufacturer.compare(second_manufacturer,
									   Qt::CaseInsensitive) != 0) {
			return DevicesPanel::labelLessThan(first_manufacturer,
											   second_manufacturer);
		}
		return DevicesPanel::labelLessThan(text(first, "order_number"),
										   text(second, "order_number"));
	});

	QVector<QMap<QString, QString>> result;
	for (const QString &key : std::as_const(order)) {
		Line line = lines.value(key);
		std::sort(line.devices.begin(), line.devices.end(),
				  &DevicesPanel::labelLessThan);
		line.values.insert(QStringLiteral("pos"),
						   QString::number(result.size() + 1));
		line.values.insert(QStringLiteral("quantity"),
						   numberText(line.quantity));
		line.values.insert(QStringLiteral("devices"),
						   line.devices.join(QStringLiteral(", ")));
		result << line.values;
	}
	return result;
}

QVector<QMap<QString, QString>> PartsBom::rows(QETProject *project)
{
	PartsDatabase database;
	if (PartsDatabase::isEnabled()) {
		database.open(PartsDatabase::path());
	}
	return rows(devices(project), &database);
}

/**
	@brief PartsBom::toCsv
	@return the list of @p project in the CSV format of the other exports,
	with the columns @p keys (the default ones if none is known)
*/
QByteArray PartsBom::toCsv(QETProject *project, const QStringList &keys,
						   bool include_headers)
{
	QStringList kept;
	for (const QString &key : keys) {
		if (!labelOf(key).isEmpty() && !kept.contains(key)) {
			kept << key;
		}
	}
	if (kept.isEmpty()) {
		kept = defaultKeys();
	}

	QByteArray csv("\xEF\xBB\xBF");
	if (include_headers) {
		QStringList headers;
		for (const QString &key : std::as_const(kept)) {
			headers << labelOf(key);
		}
		csv += BomExport::csvRecord(headers);
	}
	for (const QMap<QString, QString> &row : rows(project)) {
		QStringList values;
		for (const QString &key : std::as_const(kept)) {
			values << row.value(key);
		}
		csv += BomExport::csvRecord(values);
	}
	return csv;
}
