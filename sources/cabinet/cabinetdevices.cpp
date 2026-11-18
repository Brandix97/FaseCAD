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
#include "cabinetdevices.h"

#include "../diagram.h"
#include "../diagramevent/diagrameventaddcabinetitem.h"
#include "../partsdb/partsdatabase.h"
#include "../partsdb/partslinks.h"
#include "../qetproject.h"
#include "cabinetdialogs.h"
#include "dxfreader.h"

#include <QDomElement>
#include <QDomNodeList>
#include <QFileInfo>
#include <QGraphicsItem>

/**
	@brief CabinetDevices::cabinetFolios
	@return the cabinet folios of @p project, in the order of its folios,
	built or not
*/
QList<Diagram *> CabinetDevices::cabinetFolios(QETProject *project)
{
	QList<Diagram *> folios;
	if (!project) return folios;
	for (Diagram *folio : project->folios()) {
		if (folio->isCabinetFolio()) folios << folio;
	}
	return folios;
}

/**
	@brief CabinetDevices::placed
	@return the devices drawn on the cabinet folios of @p project. A folio
	not built yet is read from its XML and stays as it is. The drawing a
	tool is still moving with the mouse is not placed yet.
*/
QList<CabinetDevices::Placed> CabinetDevices::placed(QETProject *project)
{
	QList<Placed> result;
	for (Diagram *folio : cabinetFolios(project)) {
		const int number = project->folioIndex(folio) + 1;
		if (folio->isLoaded()) {
			const CabinetItem *preview = nullptr;
			if (auto *tool = qobject_cast<DiagramEventAddCabinetItem *>(folio->eventInterface()))
				preview = tool->preview();
			for (QGraphicsItem *item : folio->items()) {
				auto *part = qgraphicsitem_cast<CabinetItem *>(item);
				if (!part || part == preview || !part->isDevice()) continue;
				result.append({part->properties().device,
							   part->properties().device_label, number});
			}
			continue;
		}
		const QDomNodeList items = folio->deferredXml()
				.elementsByTagName(QStringLiteral("cabinet_item"));
		for (int i = 0; i < items.size(); ++i) {
			const QDomElement item = items.at(i).toElement();
			if (item.attribute(QStringLiteral("kind")) != QLatin1String("dxf_drawing"))
				continue;
			const QUuid device(item.attribute(QStringLiteral("device")));
			const QString label = item.attribute(QStringLiteral("device_label"));
			if (device.isNull() && label.isEmpty()) continue;
			result.append({device, label, number});
		}
	}
	return result;
}

/**
	@brief CabinetDevices::drawingFromParts
	Read into @p properties the drawing of a device made of @p parts (the
	element information "parts", PartsLinks): the first DXF file of its
	first part that has one, in the parts database.
	@return false if there is none: the parts database is off, the device
	has no part, no part has a DXF. @p error then says why a DXF that is
	there could not be read, empty otherwise.
*/
bool CabinetDevices::drawingFromParts(const QString &parts,
									  CabinetItem::Properties &properties,
									  QString *error)
{
	if (error) error->clear();
	const QList<PartsLinks::Link> links = PartsLinks::decode(parts);
	if (links.isEmpty() || !PartsDatabase::isEnabled()) return false;
	PartsDatabase database;
	if (!database.open(PartsDatabase::path(), error)) return false;

	for (const PartsLinks::Link &link : links) {
		const qint64 part = database.findPart(link.manufacturer, link.order_number);
		if (part <= 0) continue;
		for (const PartFile &file : database.files(part)) {
			if (file.kind != PartFileKind::Dxf) continue;
			QString why;
			const QByteArray content = database.fileContent(file.id, &why);
			if (content.isEmpty()) {
				if (error && error->isEmpty()) *error = why;
				continue;
			}
			const DxfReader::Result read = DxfReader::read(content);
			if (!read.ok) {
				if (error && error->isEmpty())
					*error = tr("%1 could not be read.\n\n%2").arg(file.file_name, read.error);
				continue;
			}
			CabinetDialogs::setDrawing(properties, read,
									   QFileInfo(file.file_name).completeBaseName());
			if (error) error->clear();
			return true;
		}
	}
	return false;
}
