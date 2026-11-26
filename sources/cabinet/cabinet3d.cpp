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
#include "cabinet3d.h"

#include "../diagram.h"
#include "../partsdb/partsdatabase.h"
#include "../partsdb/partslinks.h"
#include "../qetgraphicsitem/element.h"
#include "../qetproject.h"
#include "../ui/devicespanel.h"
#include "cabinetdevices.h"
#include "cabinetenclosure.h"
#include "cabinetitem.h"
#include "cabinetstep.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRectF>
#include <QSet>
#include <QTemporaryDir>

#include <algorithm>
#include <memory>

namespace Cabinet3d
{
namespace
{
	/// The box of @p rect (mm, y down as drawn) between @p z0 and @p z1
	void place(Solid &solid, const QRectF &rect, float z0, float z1)
	{
		solid.min = QVector3D(float(rect.left()), float(-rect.bottom()), z0);
		solid.max = QVector3D(float(rect.right()), float(-rect.top()), z1);
	}

	/**
		What the parts database says of the devices: the part of each
		symbol, found once for all of them
	*/
	class Parts
	{
		public:
			Parts(Diagram *cabinet, const QList<CabinetItem *> &devices)
			{
				QETProject *project = cabinet->project();
				if (!project || !PartsDatabase::isEnabled()) return;
				m_database = std::make_unique<PartsDatabase>();
				if (!m_database->open(PartsDatabase::path())) {
					m_database.reset();
					return;
				}
				QSet<QUuid> symbols;
				for (CabinetItem *device : devices)
				{
					if (!device->properties().device.isNull())
						symbols.insert(device->properties().device);
				}
				project->buildFoliosHoldingSymbols(symbols);
				for (Diagram *folio : project->diagrams())
				{
					if (!folio->isLoaded()) continue;
					for (Element *element : folio->elements())
					{
						const QString parts = element->elementInformations()
								.value(PartsLinks::infoKey()).toString();
						if (parts.trimmed().isEmpty()) continue;
						m_by_symbol.insert(element->uuid(), parts);
						const QString label = element->actualLabel().trimmed();
						if (!label.isEmpty() && !m_by_label.contains(label))
							m_by_label.insert(label, parts);
					}
				}
			}

				/// The part of @p device and its STEP model into @p solid,
				/// its depth, 0 when not known
			double fill(const CabinetItem *device, Solid &solid) const
			{
				if (!m_database) return 0;
				const CabinetItem::Properties properties = device->properties();
				QString parts = m_by_symbol.value(properties.device);
				if (parts.isEmpty())
					parts = m_by_label.value(properties.device_label.trimmed());
				for (const PartsLinks::Link &link : PartsLinks::decode(parts))
				{
					const qint64 id = m_database->findPart(link.manufacturer, link.order_number);
					if (id <= 0) continue;
					const std::optional<Part> part = m_database->part(id);
					if (!part) continue;
					solid.part = QStringLiteral("%1 %2").arg(link.manufacturer, link.order_number).trimmed();
					solid.clearance.top = float(part->clearance_top.value_or(0));
					solid.clearance.bottom = float(part->clearance_bottom.value_or(0));
					solid.clearance.left = float(part->clearance_left.value_or(0));
					solid.clearance.right = float(part->clearance_right.value_or(0));
					solid.clearance.front = float(part->clearance_front.value_or(0));
					solid.clearance.rear = float(part->clearance_rear.value_or(0));
					for (const PartFile &file : m_database->files(id))
					{
						if (file.kind != PartFileKind::Step) continue;
						solid.step = m_database->fileContent(file.id);
						if (solid.step.isEmpty()) continue;
						solid.step_name = file.file_name;
						break;
					}
					return part->depth.value_or(0);
				}
				return 0;
			}

		private:
			std::unique_ptr<PartsDatabase> m_database;
			QHash<QUuid, QString> m_by_symbol;
			QHash<QString, QString> m_by_label;
	};
}

QList<Solid> solidsOf(Diagram *cabinet, bool with_parts)
{
	QList<Solid> solids;
	if (!cabinet) return solids;
	cabinet->ensureLoaded();
	const qreal pixels = cabinet->pixelsPerMm();
	if (pixels <= 0) return solids;

	QList<CabinetItem *> items;
	for (QGraphicsItem *item : cabinet->items())
	{
		if (auto *part = qgraphicsitem_cast<CabinetItem *>(item)) items << part;
	}
		//Top to bottom, left to right: the same order each time
	std::sort(items.begin(), items.end(), [](CabinetItem *a, CabinetItem *b) {
		const QPointF pa = a->sceneRect().topLeft(), pb = b->sceneRect().topLeft();
		return pa.y() != pb.y() ? pa.y() < pb.y() : pa.x() < pb.x();
	});
	auto mm = [pixels](const CabinetItem *item) {
		const QRectF rect = item->sceneRect();
		return QRectF(rect.x() / pixels, rect.y() / pixels,
					  rect.width() / pixels, rect.height() / pixels);
	};

	QList<CabinetItem *> drawings;
	QList<QPair<QRectF, float>> rails;
	for (CabinetItem *item : items)
	{
		const CabinetItem::Properties properties = item->properties();
		Solid solid;
		solid.item = item->uuid();
			//"600 × 800 mm", "40×60 · 500 mm": the navigator says what it is
		solid.name = item->description();
		if (solid.name.isEmpty()) solid.name = item->name();
		switch (properties.kind)
		{
			case CabinetItem::Kind::MountingPlate: {
				solid.kind = Solid::Kind::Plate;
				solid.color = QColor(205, 207, 205);
				place(solid, mm(item), -plate_thickness, 0);
				solid.enclosure = properties.enclosure;
				const CabinetEnclosure::Enclosure enclosure =
						CabinetEnclosure::Enclosure::fromString(properties.enclosure);
				if (with_parts || enclosure.source == CabinetEnclosure::Enclosure::Source::File)
					solid.enclosure_step = CabinetEnclosure::stepOf(enclosure, &solid.enclosure_error);
				break;
			}
			case CabinetItem::Kind::DinRail: {
				solid.kind = Solid::Kind::Rail;
					//Zinc plated steel
				solid.color = QColor(196, 203, 210);
				float depth = float(properties.depth);
				if (depth <= 0) depth = float(CabinetItem::railProfile(properties.profile).depth);
				if (depth <= 0) depth = 7.5f;
				place(solid, mm(item), 0, depth);
				rails << qMakePair(mm(item), depth);
				break;
			}
			case CabinetItem::Kind::WireDuct:
				solid.kind = Solid::Kind::Duct;
					//RAL 7030
				solid.color = QColor(147, 147, 136);
				place(solid, mm(item), 0,
					  properties.depth > 0 ? float(properties.depth) : default_duct_height);
				break;
			case CabinetItem::Kind::Drawing:
				drawings << item;
				continue;
		}
		solids << solid;
	}

	std::unique_ptr<Parts> parts;
	if (with_parts && !drawings.isEmpty()) parts = std::make_unique<Parts>(cabinet, drawings);
	for (CabinetItem *item : drawings)
	{
		const CabinetItem::Properties properties = item->properties();
		Solid solid;
		solid.kind = Solid::Kind::Device;
		solid.item = item->uuid();
		solid.color = QColor(226, 226, 220);
		solid.name = properties.device_label.trimmed();
		if (solid.name.isEmpty()) solid.name = QFileInfo(properties.source).completeBaseName();
		if (solid.name.isEmpty()) solid.name = item->name();
		const QRectF rect = mm(item);
			//On the rails under it
		float base = 0;
		for (const auto &rail : std::as_const(rails))
		{
			if (rail.first.intersects(rect)) base = qMax(base, rail.second);
		}
		double depth = parts ? parts->fill(item, solid) : 0;
		if (depth <= 0) depth = default_device_depth;
		place(solid, rect, base, base + float(depth));
		solids << solid;
	}

		//The devices placed in 3D only, on the enclosure of a plate
	for (int p = 0 ; p < solids.size() ; ++p)
	{
		if (solids.at(p).kind != Solid::Kind::Plate || solids.at(p).enclosure.isEmpty()) continue;
		const CabinetEnclosure::Enclosure enclosure =
				CabinetEnclosure::Enclosure::fromString(solids.at(p).enclosure);
		for (int i = 0 ; i < enclosure.mounted.size() ; ++i)
		{
			const CabinetEnclosure::Mounted &each = enclosure.mounted.at(i);
			Solid solid;
			solid.kind = Solid::Kind::Device;
			solid.item = each.symbol;
			solid.name = each.label;
			solid.color = QColor(226, 226, 220);
			solid.part = QStringLiteral("%1 %2").arg(each.manufacturer, each.order_number).trimmed();
			solid.mounted = i;
			if (with_parts) solid.step = stepOfPart(each.manufacturer, each.order_number, &solid.step_name);
			solids << solid;
		}
	}
	return solids;
}

QByteArray stepOfPart(const QString &manufacturer, const QString &order_number, QString *file_name)
{
	if (!PartsDatabase::isEnabled()) return QByteArray();
	PartsDatabase database;
	if (!database.open(PartsDatabase::path())) return QByteArray();
	const qint64 id = database.findPart(manufacturer, order_number);
	if (id <= 0) return QByteArray();
	for (const PartFile &file : database.files(id))
	{
		if (file.kind != PartFileKind::Step) continue;
		const QByteArray content = database.fileContent(file.id);
		if (content.isEmpty()) continue;
		if (file_name) *file_name = file.file_name;
		return content;
	}
	return QByteArray();
}

QList<Unplaced> unplacedOf(Diagram *cabinet, const QList<QUuid> &mounted)
{
	QList<Unplaced> result;
	QETProject *project = cabinet ? cabinet->project() : nullptr;
	if (!project || !PartsDatabase::isEnabled()) return result;
	PartsDatabase database;
	if (!database.open(PartsDatabase::path())) return result;

	QSet<QUuid> drawn_symbols;
	QSet<QString> drawn_labels;
	for (const CabinetDevices::Placed &placed : CabinetDevices::placed(project))
	{
		drawn_symbols.insert(placed.device);
		if (!placed.label.trimmed().isEmpty()) drawn_labels.insert(placed.label.trimmed());
	}
	for (const DevicesPanel::Line &line : DevicesPanel::lines(DevicesPanel::devices(project)))
	{
		if (line.label.isEmpty() || drawn_labels.contains(line.label)) continue;
		bool skip = false;
		for (const DevicesPanel::Device &symbol : line.symbols)
		{
			const QUuid uuid(symbol.uuid);
			skip = skip || drawn_symbols.contains(uuid) || mounted.contains(uuid);
		}
		if (skip) continue;
			//The first part of its symbols with a STEP model
		bool found = false;
		for (int s = 0 ; s < line.symbols.size() && !found ; ++s)
		{
			const DevicesPanel::Device &symbol = line.symbols.at((line.main + s) % line.symbols.size());
			for (const PartsLinks::Link &link : PartsLinks::decode(symbol.parts))
			{
				const qint64 id = database.findPart(link.manufacturer, link.order_number);
				if (id <= 0) continue;
				for (const PartFile &file : database.files(id))
				{
					if (file.kind != PartFileKind::Step) continue;
					Unplaced device;
					device.symbol = QUuid(line.symbols.at(line.main).uuid);
					device.label = line.label;
					device.description = line.description;
					device.manufacturer = link.manufacturer;
					device.order_number = link.order_number;
					device.step = database.fileContent(file.id);
					device.step_name = file.file_name;
					if (device.step.isEmpty()) continue;
					result << device;
					found = true;
					break;
				}
				if (found) break;
			}
		}
			//Without a STEP model, a box of the mounting measures of the
			//first part that says them (Brandon, 2026-11-26)
		for (int s = 0 ; s < line.symbols.size() && !found ; ++s)
		{
			const DevicesPanel::Device &symbol = line.symbols.at((line.main + s) % line.symbols.size());
			for (const PartsLinks::Link &link : PartsLinks::decode(symbol.parts))
			{
				const qint64 id = database.findPart(link.manufacturer, link.order_number);
				const std::optional<Part> part = id > 0 ? database.part(id) : std::nullopt;
				if (!part || part->width.value_or(0) <= 0 || part->height.value_or(0) <= 0
					|| part->depth.value_or(0) <= 0)
					continue;
				const QVector3D size(float(*part->width), float(*part->height), float(*part->depth));
				Unplaced device;
				device.symbol = QUuid(line.symbols.at(line.main).uuid);
				device.label = line.label;
				device.description = line.description;
				device.manufacturer = link.manufacturer;
				device.order_number = link.order_number;
				device.step = boxStep(size);
				if (device.step.isEmpty()) break;
				device.step_name = QCoreApplication::translate("Cabinet3d", "Box %1 × %2 × %3 mm",
															   "a device without STEP, by its mounting measures")
						.arg(double(size.x())).arg(double(size.y())).arg(double(size.z()));
				result << device;
				found = true;
				break;
			}
		}
	}
	return result;
}

QByteArray boxStep(const QVector3D &size)
{
	static QHash<QString, QByteArray> made;
	const QString key = QStringLiteral("%1x%2x%3").arg(double(size.x())).arg(double(size.y())).arg(double(size.z()));
	const auto known = made.constFind(key);
	if (known != made.constEnd()) return known.value();
	QTemporaryDir dir;
	if (!dir.isValid()) return QByteArray();
	Solid box;
	box.kind = Solid::Kind::Device;
	box.name = QStringLiteral("box");
	box.min = QVector3D(0, 0, 0);
	box.max = size;
	box.color = QColor(150, 150, 150);
	const QString path = dir.filePath(QStringLiteral("box.step"));
	if (!CabinetStep::write({box}, path)) return QByteArray();
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) return QByteArray();
	const QByteArray content = file.readAll();
	made.insert(key, content);
	return content;
}

QVector<Piece> piecesOf(const Solid &solid, bool covers)
{
	QVector<Piece> pieces;
	const QVector3D size = solid.size();
	if (solid.kind != Solid::Kind::Rail && solid.kind != Solid::Kind::Duct) {
		pieces << Piece{solid.min, solid.max, solid.color};
		return pieces;
	}
		//Along u, across v from the middle, out of the plate w
	const bool along_x = size.x() >= size.y();
	const float length = along_x ? size.x() : size.y();
	const float width = along_x ? size.y() : size.x();
	const float depth = size.z();
	const QVector3D middle = solid.center();
	auto add = [&](float u0, float u1, float v0, float v1, float w0, float w1, const QColor &color) {
		if (u1 - u0 <= 0.001f || v1 - v0 <= 0.001f || w1 - w0 <= 0.001f) return;
		const QVector3D a = along_x
				? QVector3D(solid.min.x() + u0, middle.y() + v0, solid.min.z() + w0)
				: QVector3D(middle.x() + v0, solid.min.y() + u0, solid.min.z() + w0);
		const QVector3D b = along_x
				? QVector3D(solid.min.x() + u1, middle.y() + v1, solid.min.z() + w1)
				: QVector3D(middle.x() + v1, solid.min.y() + u1, solid.min.z() + w1);
		pieces << Piece{QVector3D(qMin(a.x(), b.x()), qMin(a.y(), b.y()), a.z()),
						QVector3D(qMax(a.x(), b.x()), qMax(a.y(), b.y()), b.z()), color};
	};

	if (solid.kind == Solid::Kind::Rail) {
			//TH35: a bottom 27 mm wide against the plate, its sides up,
			//the two lips the devices clip on; long holes 5.2 x 15 mm
			//every 25 mm in the bottom
		const float sheet = qMin(1.0f, depth / 4);
		const float scale = width / 35.0f;
		const float bottom = 13.5f * scale;
		const float hole = 2.6f * scale;
		const QColor zinc = solid.color;
		add(0, length, -bottom, -bottom + sheet, 0, depth, zinc);
		add(0, length, bottom - sheet, bottom, 0, depth, zinc);
		add(0, length, -width / 2, -bottom, depth - sheet, depth, zinc);
		add(0, length, bottom, width / 2, depth - sheet, depth, zinc);
		add(0, length, -bottom + sheet, -hole, 0, sheet, zinc);
		add(0, length, hole, bottom - sheet, 0, sheet, zinc);
		const float pitch = 25.0f, slot = 15.0f;
		const int holes = int((length - (pitch - slot)) / pitch);
		if (holes < 1) {
			add(0, length, -hole, hole, 0, sheet, zinc);
		} else {
			const float start = (length - (holes * pitch - (pitch - slot))) / 2;
			float from = 0;
			for (int k = 0 ; k < holes ; ++k)
			{
				const float at = start + k * pitch;
				add(from, at, -hole, hole, 0, sheet, zinc);
				from = at + slot;
			}
			add(from, length, -hole, hole, 0, sheet, zinc);
		}
		return pieces;
	}

		//A slotted duct, RAL 7030: its bottom, a band at the foot of each
		//wall, fingers of 6 mm with slots of 6 mm, and its cover
	const float wall = qMin(2.0f, width / 10);
	const float cover = qMin(2.0f, depth / 10);
	const float band = qMin(12.0f, depth / 4) + wall;
	const float top = depth - cover;
	const QColor body = solid.color;
	add(0, length, -width / 2, width / 2, 0, wall, body);
	for (const float side : {-1.0f, 1.0f})
	{
		const float v0 = side < 0 ? -width / 2 : width / 2 - wall;
		const float v1 = v0 + wall;
		add(0, length, v0, v1, wall, band, body);
		for (float u = 0 ; u < length ; u += 12.0f)
			add(u, qMin(u + 6.0f, length), v0, v1, band, top, body);
	}
	if (covers) add(0, length, -width / 2, width / 2, top, depth, body.lighter(112));
	return pieces;
}

void bounds(const QList<Solid> &solids, QVector3D &min, QVector3D &max)
{
	if (solids.isEmpty()) {
		min = max = QVector3D();
		return;
	}
	min = solids.first().min;
	max = solids.first().max;
	for (const Solid &solid : solids)
	{
		min = QVector3D(qMin(min.x(), solid.min.x()), qMin(min.y(), solid.min.y()),
						qMin(min.z(), solid.min.z()));
		max = QVector3D(qMax(max.x(), solid.max.x()), qMax(max.y(), solid.max.y()),
						qMax(max.z(), solid.max.z()));
	}
}
}
