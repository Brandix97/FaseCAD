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
#include "cabinetclearances.h"

#include "cabinetitem.h"
#include "cabinetparts.h"
#include "../diagram.h"
#include "../partsdb/partsdatabase.h"
#include "../partsdb/partslinks.h"
#include "../qetgraphicsitem/element.h"
#include "../qetproject.h"

#include <QHash>
#include <QSet>

#include <memory>

namespace {
		/// The clearance of the first part of @p device with one
	QMarginsF clearanceOf(const CabinetParts &parts, const CabinetItem *device)
	{
		const CabinetItem::Properties properties = device->properties();
		for (const PartsLinks::Link &link : parts.linksOf(properties.device, properties.device_label)) {
			const std::optional<Part> part = parts.part(link);
			if (!part) continue;
			const QMarginsF margins(part->clearance_left.value_or(0),
									part->clearance_top.value_or(0),
									part->clearance_right.value_or(0),
									part->clearance_bottom.value_or(0));
			if (!margins.isNull()) return margins;
		}
		return QMarginsF();
	}

	QList<CabinetItem *> devicesOf(Diagram *folio)
	{
		QList<CabinetItem *> devices;
		if (!folio) return devices;
		for (QGraphicsItem *item : folio->items()) {
			auto *part = qgraphicsitem_cast<CabinetItem *>(item);
			if (part && part->properties().kind == CabinetItem::Kind::Drawing)
				devices << part;
		}
		return devices;
	}
}

QMarginsF CabinetClearances::of(const CabinetItem *device)
{
	if (!device || !device->scene()) return QMarginsF();
	auto *folio = qobject_cast<Diagram *>(device->scene());
	QSet<QUuid> symbols;
	if (!device->properties().device.isNull()) symbols.insert(device->properties().device);
	return clearanceOf(CabinetParts(folio, symbols), device);
}

void CabinetClearances::startPlacing(Diagram *folio, CabinetItem *placing)
{
	if (!folio) return;
	QList<CabinetItem *> devices = devicesOf(folio);
	if (placing && !devices.contains(placing)) devices << placing;
	QSet<QUuid> symbols;
	for (CabinetItem *device : std::as_const(devices))
		if (!device->properties().device.isNull()) symbols.insert(device->properties().device);
	const CabinetParts parts(folio, symbols);
	CabinetItem::setPlacingShown(true);
	for (CabinetItem *device : std::as_const(devices)) {
		device->setClearance(clearanceOf(parts, device));
	}
	for (QGraphicsItem *item : folio->items()) {
		if (auto *part = qgraphicsitem_cast<CabinetItem *>(item))
			part->refreshClearance();
	}
}

void CabinetClearances::endPlacing(Diagram *folio)
{
	CabinetItem::setPlacingShown(false);
	if (!folio) return;
	for (QGraphicsItem *item : folio->items()) {
		if (auto *part = qgraphicsitem_cast<CabinetItem *>(item))
			part->refreshClearance();
	}
}
