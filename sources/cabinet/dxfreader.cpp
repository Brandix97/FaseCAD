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
#include "dxfreader.h"

#include <QFile>
#include <QHash>
#include <QPair>
#include <QPointF>
#include <QTransform>
#include <QVector>

#include <cmath>

namespace {

using Code = QPair<int, QString>;
using Codes = QVector<Code>;

constexpr double pi = 3.14159265358979323846;

	/// An entity as read: its type and its group codes; the vertices of
	/// a POLYLINE come with it
struct Entity {
	QString type;
	Codes codes;
	QVector<Codes> vertices;
};

struct Block {
	QPointF base;
	QVector<Entity> entities;
};

	/// The first value of @p code, or @p fallback
QString text(const Codes &codes, int code, const QString &fallback = QString())
{
	for (const Code &pair : codes) {
		if (pair.first == code) return pair.second;
	}
	return fallback;
}

double number(const Codes &codes, int code, double fallback = 0.0)
{
	for (const Code &pair : codes) {
		if (pair.first == code) {
			bool ok = false;
			const double value = pair.second.toDouble(&ok);
			return ok ? value : fallback;
		}
	}
	return fallback;
}

QVector<double> numbers(const Codes &codes, int code)
{
	QVector<double> values;
	for (const Code &pair : codes) {
		if (pair.first == code) values << pair.second.toDouble();
	}
	return values;
}

	/// The points given by the pairs of codes @p x_code and @p x_code + 10
QVector<QPointF> points(const Codes &codes, int x_code)
{
	QVector<QPointF> list;
	for (const Code &pair : codes) {
		if (pair.first == x_code) list << QPointF(pair.second.toDouble(), 0);
		else if (pair.first == x_code + 10 && !list.isEmpty())
			list.last().setY(pair.second.toDouble());
	}
	return list;
}

	/// Whether the entity is drawn upside down (extrusion along -z): its
	/// own coordinates then have their x mirrored
bool mirrored(const Codes &codes)
{
	return number(codes, 230, 1.0) < 0;
}

	/// An arc of the circle of @p center and @p radius, from @p start
	/// degrees counter-clockwise over @p sweep degrees, the y axis up
void addArc(QPainterPath &path, const QPointF &center, double radius,
			double start, double sweep, bool move)
{
	const QRectF rect(center.x() - radius, center.y() - radius,
					  2 * radius, 2 * radius);
		//Qt turns the other way with its y axis down: with the y axis up,
		//its negative angles are counter-clockwise
	if (move) path.arcMoveTo(rect, -start);
	path.arcTo(rect, -start, -sweep);
}

	/// The segment from @p from to @p to, an arc if @p bulge is not 0
	/// (the tangent of a quarter of its angle, counter-clockwise if positive)
void addBulge(QPainterPath &path, const QPointF &from, const QPointF &to, double bulge)
{
	const QPointF chord = to - from;
	const double length = std::hypot(chord.x(), chord.y());
	if (std::abs(bulge) < 1e-9 || length < 1e-12) {
		path.lineTo(to);
		return;
	}
	const double angle = 4.0 * std::atan(bulge);
	const double radius = length / (2.0 * std::abs(std::sin(angle / 2.0)));
	const QPointF middle = (from + to) / 2.0;
	const QPointF left(-chord.y() / length, chord.x() / length);
	const QPointF center = middle + left * ((length / 2.0) / std::tan(angle / 2.0));
	const double start = std::atan2(from.y() - center.y(), from.x() - center.x()) * 180.0 / pi;
	addArc(path, center, radius, start, angle * 180.0 / pi, false);
}

struct Vertex {
	QPointF point;
	double bulge = 0;
};

void addVertices(QPainterPath &path, const QVector<Vertex> &vertices, bool closed)
{
	if (vertices.size() < 2) return;
	path.moveTo(vertices.first().point);
	for (int i = 1 ; i < vertices.size() ; ++i)
		addBulge(path, vertices.at(i - 1).point, vertices.at(i).point, vertices.at(i - 1).bulge);
	if (closed)
		addBulge(path, vertices.last().point, vertices.first().point, vertices.last().bulge);
}

	/// The points of a B-spline (rational if @p weights is not empty)
	/// of @p degree, along its knots
QVector<QPointF> splinePoints(const QVector<QPointF> &control, const QVector<double> &knots,
							  QVector<double> weights, int degree)
{
	QVector<QPointF> result;
	const int count = control.size();
	if (weights.size() != count) weights = QVector<double>(count, 1.0);
	if (degree < 1 || count < degree + 1 || knots.size() != count + degree + 1)
		return result;

	auto evaluate = [&](double u, int span) {
		QVector<double> x(degree + 1), y(degree + 1), w(degree + 1);
		for (int j = 0 ; j <= degree ; ++j) {
			const int index = j + span - degree;
			w[j] = weights.at(index);
			x[j] = control.at(index).x() * w[j];
			y[j] = control.at(index).y() * w[j];
		}
		for (int r = 1 ; r <= degree ; ++r) {
			for (int j = degree ; j >= r ; --j) {
				const int index = j + span - degree;
				const double denominator = knots.at(index + degree - r + 1) - knots.at(index);
				const double alpha = std::abs(denominator) < 1e-15
						? 0.0 : (u - knots.at(index)) / denominator;
				x[j] = (1.0 - alpha) * x[j - 1] + alpha * x[j];
				y[j] = (1.0 - alpha) * y[j - 1] + alpha * y[j];
				w[j] = (1.0 - alpha) * w[j - 1] + alpha * w[j];
			}
		}
		const double weight = std::abs(w[degree]) < 1e-15 ? 1.0 : w[degree];
		return QPointF(x[degree] / weight, y[degree] / weight);
	};

	constexpr int steps = 16;
	for (int span = degree ; span < count ; ++span) {
		const double from = knots.at(span);
		const double to = knots.at(span + 1);
		if (to - from < 1e-15) continue;
		for (int step = result.isEmpty() ? 0 : 1 ; step <= steps ; ++step)
			result << evaluate(from + (to - from) * step / steps, span);
	}
	return result;
}

class Builder
{
	public:
		Builder(const QHash<QString, Block> &blocks, DxfReader::Result &result) :
			m_blocks(blocks), m_result(result) {}

		void draw(const QVector<Entity> &entities, const QTransform &transform,
				  QPainterPath &out, int depth)
		{
			for (const Entity &entity : entities) {
					//Paper space, and what is not shown
				if (number(entity.codes, 67) == 1.0 || number(entity.codes, 60) == 1.0)
					continue;
				if (entity.type == QLatin1String("INSERT")) {
					insert(entity, transform, out, depth);
					continue;
				}
				QPainterPath path;
				if (!drawOne(entity, path)) {
					m_result.skipped[entity.type] += 1;
					continue;
				}
				out.addPath(transform.map(path));
				++m_result.drawn;
			}
		}

	private:
		void insert(const Entity &entity, const QTransform &transform,
					QPainterPath &out, int depth)
		{
			const QString name = text(entity.codes, 2);
			if (depth > 8 || !m_blocks.contains(name)) {
				m_result.skipped[entity.type] += 1;
				return;
			}
			const Block &block = m_blocks[name];
			QPointF at(number(entity.codes, 10), number(entity.codes, 20));
			if (mirrored(entity.codes)) at.setX(-at.x());
			QTransform local;
			local.translate(at.x(), at.y());
			local.rotate(number(entity.codes, 50));
			local.scale(number(entity.codes, 41, 1.0), number(entity.codes, 42, 1.0));
			local.translate(-block.base.x(), -block.base.y());
			if (mirrored(entity.codes)) local = local * QTransform::fromScale(-1, 1);
			draw(block.entities, local * transform, out, depth + 1);
		}

		bool drawOne(const Entity &entity, QPainterPath &path)
		{
			const Codes &codes = entity.codes;
			const double flip = mirrored(codes) ? -1.0 : 1.0;
			const QString &type = entity.type;

			if (type == QLatin1String("LINE")) {
				path.moveTo(number(codes, 10), number(codes, 20));
				path.lineTo(number(codes, 11), number(codes, 21));
				return true;
			}
			if (type == QLatin1String("CIRCLE")) {
				const double radius = number(codes, 40);
				if (radius <= 0) return false;
				path.addEllipse(QPointF(flip * number(codes, 10), number(codes, 20)),
								radius, radius);
				return true;
			}
			if (type == QLatin1String("ARC")) {
				const double radius = number(codes, 40);
				if (radius <= 0) return false;
				double start = number(codes, 50);
				double end = number(codes, 51);
				if (flip < 0) {
						//Seen from below: the arc runs the other way
					const double old_start = start;
					start = 180.0 - end;
					end = 180.0 - old_start;
				}
				double sweep = end - start;
				while (sweep <= 0) sweep += 360.0;
				while (sweep > 360.0) sweep -= 360.0;
				addArc(path, QPointF(flip * number(codes, 10), number(codes, 20)),
					   radius, start, sweep, true);
				return true;
			}
			if (type == QLatin1String("LWPOLYLINE")) {
				QVector<Vertex> vertices;
				for (const Code &pair : codes) {
					if (pair.first == 10) {
						Vertex vertex;
						vertex.point.setX(flip * pair.second.toDouble());
						vertices << vertex;
					} else if (pair.first == 20 && !vertices.isEmpty()) {
						vertices.last().point.setY(pair.second.toDouble());
					} else if (pair.first == 42 && !vertices.isEmpty()) {
						vertices.last().bulge = flip * pair.second.toDouble();
					}
				}
				addVertices(path, vertices, int(number(codes, 70)) & 1);
				return vertices.size() >= 2;
			}
			if (type == QLatin1String("POLYLINE")) {
				const int flags = int(number(codes, 70));
					//3D meshes and polyface meshes are no lines of a view
				if (flags & (16 | 64)) return false;
				QVector<Vertex> vertices;
				for (const Codes &vertex_codes : entity.vertices) {
						//The frame of a spline-fit polyline is not drawn
					if (int(number(vertex_codes, 70)) & 16) continue;
					Vertex vertex;
					vertex.point = QPointF(flip * number(vertex_codes, 10),
										   number(vertex_codes, 20));
					vertex.bulge = flip * number(vertex_codes, 42);
					vertices << vertex;
				}
				addVertices(path, vertices, flags & 1);
				return vertices.size() >= 2;
			}
			if (type == QLatin1String("ELLIPSE")) {
				const QPointF center(number(codes, 10), number(codes, 20));
				const QPointF major(number(codes, 11), number(codes, 21));
				const double ratio = number(codes, 40, 1.0);
				if (std::hypot(major.x(), major.y()) < 1e-12) return false;
				const QPointF minor(-major.y() * ratio, major.x() * ratio);
				const double start = number(codes, 41, 0.0);
				double sweep = number(codes, 42, 2 * pi) - start;
				while (sweep <= 1e-12) sweep += 2 * pi;
				while (sweep > 2 * pi + 1e-12) sweep -= 2 * pi;
				QPainterPath unit;
				addArc(unit, QPointF(0, 0), 1.0, start * 180.0 / pi, sweep * 180.0 / pi, true);
				const QTransform onto(major.x(), major.y(), minor.x(), minor.y(),
									  center.x(), center.y());
				path = onto.map(unit);
				return true;
			}
			if (type == QLatin1String("SPLINE")) {
				const QVector<QPointF> control = points(codes, 10);
				const QVector<QPointF> fit = points(codes, 11);
				QVector<QPointF> line = splinePoints(control, numbers(codes, 40),
													 numbers(codes, 41),
													 int(number(codes, 71, 3)));
				if (line.size() < 2) line = fit.size() >= 2 ? fit : control;
				if (line.size() < 2) return false;
				path.moveTo(line.first());
				for (int i = 1 ; i < line.size() ; ++i) path.lineTo(line.at(i));
				if ((int(number(codes, 70)) & 1) && line.first() != line.last())
					path.closeSubpath();
				return true;
			}
			if (type == QLatin1String("SOLID") || type == QLatin1String("TRACE")) {
				const QPointF a(number(codes, 10), number(codes, 20));
				const QPointF b(number(codes, 11), number(codes, 21));
				const QPointF c(number(codes, 12), number(codes, 22));
				const QPointF d(number(codes, 13, c.x()), number(codes, 23, c.y()));
					//The corners come as 1, 2, 4, 3
				path.moveTo(a);
				path.lineTo(b);
				path.lineTo(d);
				path.lineTo(c);
				path.closeSubpath();
				return true;
			}
			return false;
		}

		const QHash<QString, Block> &m_blocks;
		DxfReader::Result &m_result;
};

	/// The group codes of @p data: a code and a value, each on its line
bool pairsOf(const QByteArray &data, Codes &pairs)
{
	QList<QByteArray> lines = data.split('\n');
	for (QByteArray &line : lines) {
		if (line.endsWith('\r')) line.chop(1);
	}
	pairs.reserve(lines.size() / 2);
	for (int i = 0 ; i + 1 < lines.size() ; i += 2) {
		bool ok = false;
		const int code = lines.at(i).trimmed().toInt(&ok);
		if (!ok) return false;
		pairs << Code(code, QString::fromLatin1(lines.at(i + 1).trimmed()));
	}
	return true;
}

} // namespace

/**
	@brief DxfReader::millimetresPerUnit
	@return the millimetres of one unit of a file whose $INSUNITS is
	@p insunits; a file without a unit is taken as millimetres
*/
double DxfReader::millimetresPerUnit(int insunits)
{
	switch (insunits) {
		case 1:  return 25.4;       //inches
		case 2:  return 304.8;      //feet
		case 4:  return 1.0;        //millimetres
		case 5:  return 10.0;       //centimetres
		case 6:  return 1000.0;     //metres
		case 8:  return 0.0000254;  //microinches
		case 9:  return 0.0254;     //mils
		case 10: return 914.4;      //yards
		case 13: return 0.001;      //microns
		case 14: return 100.0;      //decimetres
		default: return 1.0;
	}
}

DxfReader::Result DxfReader::readFile(const QString &file_path)
{
	QFile file(file_path);
	if (!file.open(QIODevice::ReadOnly)) {
		Result result;
		result.error = tr("The file could not be opened: %1").arg(file.errorString());
		return result;
	}
	return read(file.readAll());
}

/**
	@brief DxfReader::read
	@return the drawing of the DXF file whose bytes are @p data
*/
DxfReader::Result DxfReader::read(const QByteArray &data)
{
	Result result;
	if (data.startsWith("AutoCAD Binary DXF")) {
		result.error = tr("This DXF file is binary: save it again as a text (ASCII) DXF.");
		return result;
	}
	Codes pairs;
	if (!pairsOf(data, pairs) || pairs.isEmpty()) {
		result.error = tr("This file is not a DXF drawing.");
		return result;
	}

	QHash<QString, Block> blocks;
	QVector<Entity> entities;
	int insunits = 0;

	QString section;
	QString variable;
	Block *block = nullptr;
	QString block_name;
	Entity current;
	bool in_entity = false;
	bool in_block_header = false;
	Entity *polyline = nullptr;   //the POLYLINE taking the VERTEX that follow

	auto finish = [&]() {
		if (!in_entity) return;
		in_entity = false;
		QVector<Entity> &list = (section == QLatin1String("BLOCKS") && block)
				? block->entities : entities;
		if (current.type == QLatin1String("VERTEX") && polyline) {
			polyline->vertices << current.codes;
			return;
		}
		if (current.type == QLatin1String("SEQEND")) {
			polyline = nullptr;
			return;
		}
		list << current;
		polyline = current.type == QLatin1String("POLYLINE") ? &list.last() : nullptr;
	};

	for (int i = 0 ; i < pairs.size() ; ++i) {
		const int code = pairs.at(i).first;
		const QString &value = pairs.at(i).second;

		if (code == 0) {
			finish();
			if (in_block_header) {
				in_block_header = false;
			}
			if (value == QLatin1String("SECTION")) {
				section.clear();
				if (i + 1 < pairs.size() && pairs.at(i + 1).first == 2) {
					section = pairs.at(i + 1).second;
					++i;
				}
				continue;
			}
			if (value == QLatin1String("ENDSEC")) {
				section.clear();
				block = nullptr;
				polyline = nullptr;
				continue;
			}
			if (value == QLatin1String("EOF")) break;
			if (section == QLatin1String("BLOCKS")) {
				if (value == QLatin1String("BLOCK")) {
					in_block_header = true;
					block_name.clear();
					block = nullptr;
					polyline = nullptr;
					continue;
				}
				if (value == QLatin1String("ENDBLK")) {
					block = nullptr;
					polyline = nullptr;
					continue;
				}
			}
			if (section == QLatin1String("ENTITIES")
					|| (section == QLatin1String("BLOCKS") && block)) {
				current = Entity();
				current.type = value;
				in_entity = true;
			}
			continue;
		}

		if (section == QLatin1String("HEADER")) {
			if (code == 9) variable = value;
			else if (variable == QLatin1String("$INSUNITS") && code == 70) insunits = value.toInt();
			continue;
		}
		if (in_block_header) {
			if (code == 2) {
				block_name = value;
				block = &blocks[block_name];
				block->entities.clear();
			} else if (block && code == 10) {
				block->base.setX(value.toDouble());
			} else if (block && code == 20) {
				block->base.setY(value.toDouble());
			}
			continue;
		}
		if (in_entity) current.codes << pairs.at(i);
	}
	finish();

	result.unit = millimetresPerUnit(insunits);
	QPainterPath drawing;
	Builder(blocks, result).draw(entities, QTransform(), drawing, 0);

		//In millimetres, the y axis down, the top left corner at 0
	drawing = QTransform::fromScale(result.unit, -result.unit).map(drawing);
	const QRectF bounds = drawing.boundingRect();
	if (result.drawn == 0 || (bounds.width() <= 0 && bounds.height() <= 0)) {
		result.error = tr("This DXF file holds no line this program can draw.");
		return result;
	}
	result.path = QTransform::fromTranslate(-bounds.left(), -bounds.top()).map(drawing);
	result.ok = true;
	return result;
}

/**
	@brief DxfReader::pathToString
	@return @p path written as "M x y L x y C x y x y x y ...", the way it
	is kept in a project file
*/
QString DxfReader::pathToString(const QPainterPath &path)
{
	QStringList parts;
	parts.reserve(path.elementCount() * 3);
	auto add = [&](double value) {
		parts << QString::number(std::round(value * 10000.0) / 10000.0, 'g', 12);
	};
	for (int i = 0 ; i < path.elementCount() ; ++i) {
		const QPainterPath::Element element = path.elementAt(i);
		switch (element.type) {
			case QPainterPath::MoveToElement:
				parts << QStringLiteral("M");
				break;
			case QPainterPath::LineToElement:
				parts << QStringLiteral("L");
				break;
			case QPainterPath::CurveToElement:
				parts << QStringLiteral("C");
				break;
			case QPainterPath::CurveToDataElement:
				break;
		}
		add(element.x);
		add(element.y);
	}
	return parts.join(QLatin1Char(' '));
}

/**
	@brief DxfReader::pathFromString
	@return the path pathToString() wrote as @p text; @p ok is false if
	the text is not one
*/
QPainterPath DxfReader::pathFromString(const QString &text, bool *ok)
{
	QPainterPath path;
	const QStringList parts = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
	bool good = true;
	auto value = [&](int index) {
		if (index >= parts.size()) {
			good = false;
			return 0.0;
		}
		bool converted = false;
		const double result = parts.at(index).toDouble(&converted);
		if (!converted) good = false;
		return result;
	};
	int i = 0;
	while (good && i < parts.size()) {
		const QString command = parts.at(i);
		if (command == QLatin1String("M")) {
			path.moveTo(value(i + 1), value(i + 2));
			i += 3;
		} else if (command == QLatin1String("L")) {
			path.lineTo(value(i + 1), value(i + 2));
			i += 3;
		} else if (command == QLatin1String("C")) {
			path.cubicTo(value(i + 1), value(i + 2), value(i + 3),
						 value(i + 4), value(i + 5), value(i + 6));
			i += 7;
		} else {
			good = false;
		}
	}
	if (ok) *ok = good;
	return good ? path : QPainterPath();
}
