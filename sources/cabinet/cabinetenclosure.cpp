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
#include "cabinetenclosure.h"

#include "../partsdb/partsdatabase.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

namespace CabinetEnclosure
{
namespace
{
	QString tr(const char *text)
	{
		return QCoreApplication::translate("CabinetEnclosure", text);
	}

	QString sourceToString(Enclosure::Source source)
	{
		switch (source)
		{
			case Enclosure::Source::Parts: return QStringLiteral("parts");
			case Enclosure::Source::File: return QStringLiteral("file");
			case Enclosure::Source::Manual: return QStringLiteral("manual");
			case Enclosure::Source::None: break;
		}
		return QStringLiteral("none");
	}

	Enclosure::Source sourceFromString(const QString &text)
	{
		if (text == QLatin1String("parts")) return Enclosure::Source::Parts;
		if (text == QLatin1String("file")) return Enclosure::Source::File;
		if (text == QLatin1String("manual")) return Enclosure::Source::Manual;
		return Enclosure::Source::None;
	}

	/**
		The turn putting the front of node @p plate of @p model towards
		+z and its top towards +y; the middle of that front into
		@p front. The front looks into the enclosure: towards the middle
		of the model. The top is y, or z for a plate facing along y.
	*/
	QMatrix4x4 baseTurn(const CabinetStep::Model &model, int plate, QVector3D &front)
	{
		const CabinetStep::Node &node = model.nodes.at(plate);
		const QVector3D size = node.max - node.min;
		int thin = 0;
		for (int axis = 1 ; axis < 3 ; ++axis)
		{
			if (size[axis] < size[thin]) thin = axis;
		}
		const QVector3D middle = (model.min + model.max) / 2;
		const QVector3D plate_middle = (node.min + node.max) / 2;
		const float sign = middle[thin] >= plate_middle[thin] ? 1.0f : -1.0f;
		front = plate_middle;
		front[thin] = sign > 0 ? node.max[thin] : node.min[thin];
		QVector3D front_axis;
		front_axis[thin] = sign;
		QVector3D up_axis;
		up_axis[thin == 1 ? 2 : 1] = 1;
		for (const QMatrix4x4 &turn : CabinetStep::boxTurns())
		{
			if ((turn.mapVector(front_axis) - QVector3D(0, 0, 1)).length() < 1e-3f
				&& (turn.mapVector(up_axis) - QVector3D(0, 1, 0)).length() < 1e-3f) {
				return turn;
			}
		}
		return QMatrix4x4();
	}

	QMatrix4x4 turnOf(const CabinetStep::Model &model, int plate, const Enclosure &enclosure,
					  QVector3D &front)
	{
		QMatrix4x4 quarter;
		quarter.rotate(90.0f * float(((enclosure.quarter_turns % 4) + 4) % 4), 0, 0, 1);
		return quarter * baseTurn(model, plate, front);
	}
}

bool Mounted::operator==(const Mounted &other) const
{
	return symbol == other.symbol && label == other.label && manufacturer == other.manufacturer
			&& order_number == other.order_number && node == other.node
			&& (at - other.at).length() < 1e-4f && (normal - other.normal).length() < 1e-4f
			&& (up - other.up).length() < 1e-4f && quarter_turns == other.quarter_turns
			&& side == other.side && qAbs(sink - other.sink) < 1e-6;
}

int Enclosure::mountedIndex(const QUuid &symbol) const
{
	for (int i = 0 ; i < mounted.size() ; ++i)
	{
		if (mounted.at(i).symbol == symbol) return i;
	}
	return -1;
}

bool Enclosure::placed() const
{
	return source == Source::Manual || (hasModel() && !plate.isEmpty());
}

QString Enclosure::title() const
{
	switch (source)
	{
		case Source::Parts:
			return QStringLiteral("%1 %2").arg(manufacturer, order_number).trimmed();
		case Source::File:
			return QFileInfo(path).fileName();
		case Source::Manual: {
			const QLocale locale;
			return QStringLiteral("%1 × %2 × %3 mm")
					.arg(locale.toString(width), locale.toString(height), locale.toString(depth));
		}
		case Source::None:
			break;
	}
	return QString();
}

QString Enclosure::toString() const
{
	if (source == Source::None && hidden.isEmpty() && mounted.isEmpty()) return QString();
	QJsonObject object;
	object.insert(QStringLiteral("source"), sourceToString(source));
	switch (source)
	{
		case Source::Parts:
			object.insert(QStringLiteral("manufacturer"), manufacturer);
			object.insert(QStringLiteral("order_number"), order_number);
			break;
		case Source::File:
			object.insert(QStringLiteral("path"), path);
			break;
		case Source::Manual:
			object.insert(QStringLiteral("width"), width);
			object.insert(QStringLiteral("height"), height);
			object.insert(QStringLiteral("depth"), depth);
			object.insert(QStringLiteral("plate_width"), plate_width);
			object.insert(QStringLiteral("plate_height"), plate_height);
			break;
		case Source::None:
			break;
	}
	if (source != Source::Manual && !plate.isEmpty()) object.insert(QStringLiteral("plate"), plate);
	if (quarter_turns % 4) object.insert(QStringLiteral("quarter_turns"), quarter_turns % 4);
	if (!hidden.isEmpty()) object.insert(QStringLiteral("hidden"), QJsonArray::fromStringList(hidden));
	if (!mounted.isEmpty()) {
		auto vector = [](const QVector3D &v) {
			return QJsonArray({double(v.x()), double(v.y()), double(v.z())});
		};
		QJsonArray list;
		for (const Mounted &each : mounted)
		{
			QJsonObject device;
			device.insert(QStringLiteral("symbol"), each.symbol.toString(QUuid::WithoutBraces));
			device.insert(QStringLiteral("label"), each.label);
			device.insert(QStringLiteral("manufacturer"), each.manufacturer);
			device.insert(QStringLiteral("order_number"), each.order_number);
			if (!each.node.isEmpty()) device.insert(QStringLiteral("node"), each.node);
			device.insert(QStringLiteral("at"), vector(each.at));
			device.insert(QStringLiteral("normal"), vector(each.normal));
			device.insert(QStringLiteral("up"), vector(each.up));
			if (each.quarter_turns % 4) device.insert(QStringLiteral("quarter_turns"), each.quarter_turns % 4);
			if (each.side) device.insert(QStringLiteral("side"), each.side);
			if (!qFuzzyIsNull(each.sink)) device.insert(QStringLiteral("sink"), each.sink);
			list.append(device);
		}
		object.insert(QStringLiteral("mounted"), list);
	}
	return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

Enclosure Enclosure::fromString(const QString &text)
{
	Enclosure enclosure;
	const QJsonObject object = QJsonDocument::fromJson(text.toUtf8()).object();
	if (object.isEmpty()) return enclosure;
	enclosure.source = sourceFromString(object.value(QStringLiteral("source")).toString());
	enclosure.manufacturer = object.value(QStringLiteral("manufacturer")).toString();
	enclosure.order_number = object.value(QStringLiteral("order_number")).toString();
	enclosure.path = object.value(QStringLiteral("path")).toString();
	auto measure = [&object](const char *key, double fallback) {
		const double value = object.value(QLatin1String(key)).toDouble(fallback);
		return value > 0 ? value : fallback;
	};
	enclosure.width = measure("width", enclosure.width);
	enclosure.height = measure("height", enclosure.height);
	enclosure.depth = measure("depth", enclosure.depth);
	enclosure.plate_width = measure("plate_width", enclosure.plate_width);
	enclosure.plate_height = measure("plate_height", enclosure.plate_height);
	enclosure.plate = enclosure.source == Source::Manual
			? QStringLiteral("plate") : object.value(QStringLiteral("plate")).toString();
	enclosure.quarter_turns = object.value(QStringLiteral("quarter_turns")).toInt();
	for (const QJsonValue &value : object.value(QStringLiteral("hidden")).toArray())
		enclosure.hidden << value.toString();
	auto vector = [](const QJsonValue &value) {
		const QJsonArray array = value.toArray();
		return array.size() == 3 ? QVector3D(float(array.at(0).toDouble()), float(array.at(1).toDouble()),
											 float(array.at(2).toDouble()))
								 : QVector3D();
	};
	for (const QJsonValue &value : object.value(QStringLiteral("mounted")).toArray())
	{
		const QJsonObject device = value.toObject();
		Mounted each;
		each.symbol = QUuid(device.value(QStringLiteral("symbol")).toString());
		if (each.symbol.isNull()) continue;
		each.label = device.value(QStringLiteral("label")).toString();
		each.manufacturer = device.value(QStringLiteral("manufacturer")).toString();
		each.order_number = device.value(QStringLiteral("order_number")).toString();
		each.node = device.value(QStringLiteral("node")).toString();
		each.at = vector(device.value(QStringLiteral("at")));
		each.normal = vector(device.value(QStringLiteral("normal")));
		each.up = vector(device.value(QStringLiteral("up")));
		if (each.normal.isNull()) each.normal = QVector3D(0, 0, 1);
		if (each.up.isNull()) each.up = faceUp(each.normal);
		each.quarter_turns = device.value(QStringLiteral("quarter_turns")).toInt();
		each.side = qBound(0, device.value(QStringLiteral("side")).toInt(), 5);
		each.sink = device.value(QStringLiteral("sink")).toDouble();
		enclosure.mounted << each;
	}
	return enclosure;
}

bool Enclosure::operator==(const Enclosure &other) const
{
	return toString() == other.toString();
}

CabinetStep::Model manualModel(const Enclosure &enclosure)
{
	CabinetStep::Model model;
	const float w = float(enclosure.width), h = float(enclosure.height), d = float(enclosure.depth);
	const float t = float(sheet);
	const float pw = float(qMin(enclosure.plate_width, enclosure.width - 2 * sheet));
	const float ph = float(qMin(enclosure.plate_height, enclosure.height - 2 * sheet));
	if (w <= 2 * t || h <= 2 * t || d <= 2 * t || pw <= 0 || ph <= 0) {
		model.error = tr("The measures of the enclosure do not make a box.");
		return model;
	}
		//RAL 7035, and a galvanised plate
	const QColor body(214, 214, 208);
	const QColor galvanised(186, 190, 192);

	CabinetStep::Node root;
	root.name = tr("Enclosure");
	root.id = QStringLiteral("enclosure");
	root.assembly = true;
	model.nodes << root;
	struct Side {const char *name; const char *id; QVector3D min; QVector3D max; QColor color;};
	const Side sides[] = {
		{QT_TRANSLATE_NOOP("CabinetEnclosure", "Back"), "back", {-w / 2, 0, -d}, {w / 2, h, -d + t}, body},
		{QT_TRANSLATE_NOOP("CabinetEnclosure", "Left side"), "left", {-w / 2, 0, -d + t}, {-w / 2 + t, h, 0}, body},
		{QT_TRANSLATE_NOOP("CabinetEnclosure", "Right side"), "right", {w / 2 - t, 0, -d + t}, {w / 2, h, 0}, body},
		{QT_TRANSLATE_NOOP("CabinetEnclosure", "Top"), "top", {-w / 2 + t, h - t, -d + t}, {w / 2 - t, h, 0}, body},
		{QT_TRANSLATE_NOOP("CabinetEnclosure", "Bottom"), "bottom", {-w / 2 + t, 0, -d + t}, {w / 2 - t, t, 0}, body},
		{QT_TRANSLATE_NOOP("CabinetEnclosure", "Mounting plate"), "plate",
		 {-pw / 2, (h - ph) / 2, -d + t}, {pw / 2, (h + ph) / 2, -d + t + float(plate_thickness)}, galvanised}};
	for (const Side &side : sides)
	{
		CabinetStep::Node node;
		node.name = tr(side.name);
		node.id = QLatin1String(side.id);
		node.parent = 0;
		node.depth = 1;
		node.first = int(model.vertices.size() / CabinetStep::vertex_size);
		CabinetStep::appendBox(model.vertices, side.min, side.max, side.color);
		node.count = int(model.vertices.size() / CabinetStep::vertex_size) - node.first;
		node.min = side.min;
		node.max = side.max;
		model.nodes << node;
	}
	model.nodes[0].count = int(model.vertices.size() / CabinetStep::vertex_size);
	model.min = model.nodes[0].min = QVector3D(-w / 2, 0, -d);
	model.max = model.nodes[0].max = QVector3D(w / 2, h, 0);
	model.ok = true;
	return model;
}

QByteArray stepOf(const Enclosure &enclosure, QString *error)
{
	if (error) error->clear();
	if (enclosure.source == Enclosure::Source::File) {
		QFile file(enclosure.path);
		if (!file.open(QIODevice::ReadOnly)) {
			if (error) *error = tr("%1 cannot be read: %2").arg(enclosure.path, file.errorString());
			return QByteArray();
		}
		return file.readAll();
	}
	if (enclosure.source != Enclosure::Source::Parts) return QByteArray();
	if (!PartsDatabase::isEnabled()) {
		if (error) *error = tr("The parts database is not in use.");
		return QByteArray();
	}
	PartsDatabase database;
	if (!database.open(PartsDatabase::path(), error)) return QByteArray();
	const qint64 part = database.findPart(enclosure.manufacturer, enclosure.order_number);
	if (part <= 0) {
		if (error) *error = tr("The parts database has no part %1.").arg(enclosure.title());
		return QByteArray();
	}
	for (const PartFile &file : database.files(part))
	{
		if (file.kind != PartFileKind::Step) continue;
		const QByteArray content = database.fileContent(file.id, error);
		if (!content.isEmpty()) return content;
	}
	if (error && error->isEmpty()) *error = tr("The part %1 has no STEP model.").arg(enclosure.title());
	return QByteArray();
}

QMatrix4x4 placement(const CabinetStep::Model &model, const Enclosure &enclosure,
					 const Cabinet3d::Solid &plate, bool *ok)
{
	if (ok) *ok = false;
	const int index = model.node(enclosure.plate);
	if (index < 0 || model.nodes.at(index).count <= 0) return QMatrix4x4();
	QVector3D front;
	const QMatrix4x4 turn = turnOf(model, index, enclosure, front);
	const QVector3D target(plate.center().x(), plate.center().y(), plate.max.z());
	QMatrix4x4 move;
	move.translate(target - turn.map(front));
	if (ok) *ok = true;
	return move * turn;
}

QSizeF plateSize(const CabinetStep::Model &model, const Enclosure &enclosure,
				 const Cabinet3d::Solid &plate)
{
	Q_UNUSED(plate)
	const int index = model.node(enclosure.plate);
	if (index < 0) return QSizeF();
	QVector3D front;
	const QMatrix4x4 turn = turnOf(model, index, enclosure, front);
	const QVector3D extent = turn.mapVector(model.nodes.at(index).max - model.nodes.at(index).min);
	return QSizeF(qAbs(extent.x()), qAbs(extent.y()));
}

QVector3D faceUp(const QVector3D &normal)
{
	const QVector3D n = normal.normalized();
	QVector3D up = QVector3D(0, 1, 0) - QVector3D::dotProduct(QVector3D(0, 1, 0), n) * n;
		//A roof or a floor: its top towards the back
	if (up.length() < 0.2f) up = QVector3D(0, 0, -1) - QVector3D::dotProduct(QVector3D(0, 0, -1), n) * n;
	return up.normalized();
}

QMatrix4x4 mountedPlacement(const Mounted &mounted, const CabinetStep::Model &device)
{
		//The side of the model looking out, and its top
	static const QVector3D outs[6] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
	static const QVector3D tops[6] = {{0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
	const QVector3D out = outs[qBound(0, mounted.side, 5)];
	const QVector3D top = tops[qBound(0, mounted.side, 5)];
	const QVector3D n = mounted.normal.normalized();
	QVector3D s = mounted.up - QVector3D::dotProduct(mounted.up, n) * n;
	s = s.length() < 1e-4f ? faceUp(n) : s.normalized();

		//(out, top, out x top) onto (n, s, n x s)
	const QVector3D a = QVector3D::crossProduct(out, top);
	const QVector3D b = QVector3D::crossProduct(n, s);
	QMatrix4x4 from(out.x(), top.x(), a.x(), 0,
					out.y(), top.y(), a.y(), 0,
					out.z(), top.z(), a.z(), 0,
					0, 0, 0, 1);
	QMatrix4x4 to(n.x(), s.x(), b.x(), 0,
				  n.y(), s.y(), b.y(), 0,
				  n.z(), s.z(), b.z(), 0,
				  0, 0, 0, 1);
	QMatrix4x4 turn;
	turn.rotate(90.0f * float(((mounted.quarter_turns % 4) + 4) % 4), n);
	const QMatrix4x4 rotation = turn * to * from.transposed();

		//Its back, in the middle of it, on the point of the face
	QVector3D back = (device.min + device.max) / 2;
	for (int axis = 0 ; axis < 3 ; ++axis)
	{
		if (out[axis] > 0.5f) back[axis] = device.min[axis];
		if (out[axis] < -0.5f) back[axis] = device.max[axis];
	}
	QMatrix4x4 move;
	move.translate(mounted.at - n * float(mounted.sink) - rotation.map(back));
	return move * rotation;
}

bool isHidden(const CabinetStep::Model &model, int index, const QStringList &hidden)
{
	while (index >= 0 && index < model.nodes.size())
	{
		if (hidden.contains(QStringLiteral("node:") + model.nodes.at(index).id)) return true;
		index = model.nodes.at(index).parent;
	}
	return false;
}
}
