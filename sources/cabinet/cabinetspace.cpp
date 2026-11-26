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
#include "cabinetspace.h"

#include "cabinetstep.h"

#include <QSet>
#include <QtMath>

#include <algorithm>

namespace CabinetSpace
{
namespace
{
		/// Less than this is touching, not in each other, mm
	const float touching = 0.5f;

	QString tr(const char *text)
	{
		return QCoreApplication::translate("CabinetSpace", text);
	}

	bool isDevice(const Body &body)
	{
		return body.kind == Cabinet3d::Solid::Kind::Device;
	}

		/// The box both @p a and @p b take into @p min and @p max; false
		/// when they only touch, or less
	bool overlap(const QVector3D &a_min, const QVector3D &a_max,
				 const QVector3D &b_min, const QVector3D &b_max,
				 QVector3D &min, QVector3D &max)
	{
		min = QVector3D(qMax(a_min.x(), b_min.x()), qMax(a_min.y(), b_min.y()),
						qMax(a_min.z(), b_min.z()));
		max = QVector3D(qMin(a_max.x(), b_max.x()), qMin(a_max.y(), b_max.y()),
						qMin(a_max.z(), b_max.z()));
		return max.x() - min.x() > touching && max.y() - min.y() > touching
				&& max.z() - min.z() > touching;
	}

	float asked(const Cabinet3d::Clearance &clearance, Side side)
	{
		switch (side) {
			case Top: return clearance.top;
			case Bottom: return clearance.bottom;
			case Left: return clearance.left;
			case Right: return clearance.right;
			case Front: return clearance.front;
			case Rear: return clearance.rear;
			default: return 0;
		}
	}

		/// How far @p min to @p max goes beyond @p inside on side
		/// @p side, and that part of it
	float beyond(const QVector3D &min, const QVector3D &max, const QVector3D &inside_min,
				 const QVector3D &inside_max, Side side, QVector3D &out_min, QVector3D &out_max)
	{
		out_min = min;
		out_max = max;
		switch (side) {
			case Top:
				out_min.setY(qMax(min.y(), inside_max.y()));
				return max.y() - inside_max.y();
			case Bottom:
				out_max.setY(qMin(max.y(), inside_min.y()));
				return inside_min.y() - min.y();
			case Left:
				out_max.setX(qMin(max.x(), inside_min.x()));
				return inside_min.x() - min.x();
			case Right:
				out_min.setX(qMax(min.x(), inside_max.x()));
				return max.x() - inside_max.x();
			case Front:
				out_min.setZ(qMax(min.z(), inside_max.z()));
				return max.z() - inside_max.z();
			default:
				return 0;
		}
	}

	QString nameOf(const Body &body)
	{
		switch (body.kind) {
			case Cabinet3d::Solid::Kind::Duct: return tr("the duct %1").arg(body.name);
			case Cabinet3d::Solid::Kind::Rail: return tr("the rail %1").arg(body.name);
			default: return body.name;
		}
	}

	QString millimetres(float value)
	{
		return QString::number(qMax(1, qRound(value)));
	}
}

bool zone(const Body &body, Side side, QVector3D &min, QVector3D &max)
{
	const float space = asked(body.clearance, side);
		//Behind a device on the plate is the plate
	if (space <= touching || body.mounted || side == Rear || !isDevice(body)) return false;
	min = body.min;
	max = body.max;
	switch (side) {
		case Top:
			min.setY(body.max.y());
			max.setY(body.max.y() + space);
			return true;
		case Bottom:
			max.setY(body.min.y());
			min.setY(body.min.y() - space);
			return true;
		case Left:
			max.setX(body.min.x());
			min.setX(body.min.x() - space);
			return true;
		case Right:
			min.setX(body.max.x());
			max.setX(body.max.x() + space);
			return true;
		case Front:
			min.setZ(body.max.z());
			max.setZ(body.max.z() + space);
			return true;
		default:
			return false;
	}
}

QList<Problem> check(const QList<Body> &bodies, bool has_inside,
					 const QVector3D &inside_min, const QVector3D &inside_max)
{
	QList<Problem> problems;
	QSet<QPair<int, int>> colliding;
	auto plate = [](const Body &body) {return body.kind == Cabinet3d::Solid::Kind::Plate;};

		//Two parts in each other, one of them a device: rails and ducts
		//meeting at their ends are left alone
	for (int i = 0 ; i < bodies.size() ; ++i)
	{
		for (int j = i + 1 ; j < bodies.size() ; ++j)
		{
			const Body &a = bodies.at(i);
			const Body &b = bodies.at(j);
			if (plate(a) || plate(b) || (!isDevice(a) && !isDevice(b))) continue;
			Problem problem;
			if (!overlap(a.min, a.max, b.min, b.max, problem.min, problem.max)) continue;
			problem.kind = Problem::Kind::Collision;
			problem.first = isDevice(a) ? i : j;
			problem.second = isDevice(a) ? j : i;
			const QVector3D size = problem.max - problem.min;
			problem.missing = qMin(size.x(), qMin(size.y(), size.z()));
			problems << problem;
			colliding.insert(qMakePair(i, j));
			colliding.insert(qMakePair(j, i));
		}
	}

	for (int i = 0 ; i < bodies.size() ; ++i)
	{
		const Body &body = bodies.at(i);
		if (!isDevice(body) || body.mounted) continue;
		for (int s = Top ; s <= Front ; ++s)
		{
			const Side side = Side(s);
			QVector3D zone_min, zone_max;
			if (!zone(body, side, zone_min, zone_max)) continue;
			const float space = asked(body.clearance, side);
				//Another part in the free space the device asks
			for (int j = 0 ; j < bodies.size() ; ++j)
			{
				const Body &other = bodies.at(j);
				if (j == i || plate(other) || colliding.contains(qMakePair(i, j))) continue;
				Problem problem;
				if (!overlap(zone_min, zone_max, other.min, other.max, problem.min, problem.max)) continue;
				float gap = 0;
				switch (side) {
					case Top: gap = other.min.y() - body.max.y(); break;
					case Bottom: gap = body.min.y() - other.max.y(); break;
					case Left: gap = body.min.x() - other.max.x(); break;
					case Right: gap = other.min.x() - body.max.x(); break;
					case Front: gap = other.min.z() - body.max.z(); break;
					default: break;
				}
				problem.kind = Problem::Kind::Clearance;
				problem.first = i;
				problem.second = j;
				problem.side = side;
				problem.asked = space;
				problem.missing = space - qMax(0.0f, gap);
				problems << problem;
			}
				//The enclosure too near
			if (has_inside) {
				Problem problem;
				const float out = beyond(zone_min, zone_max, inside_min, inside_max, side,
										 problem.min, problem.max);
				if (out > touching) {
					problem.kind = Problem::Kind::Clearance;
					problem.first = i;
					problem.side = side;
					problem.asked = space;
					problem.missing = qMin(out, space);
					problems << problem;
				}
			}
		}
			//Through a wall, or keeping the door from closing
		if (has_inside) {
			for (int s = Top ; s <= Front ; ++s)
			{
				Problem problem;
				const float out = beyond(body.min, body.max, inside_min, inside_max, Side(s),
										 problem.min, problem.max);
				if (out <= touching) continue;
				problem.kind = Problem::Kind::Enclosure;
				problem.first = i;
				problem.side = Side(s);
				problem.missing = out;
				problems << problem;
			}
		}
	}

		//Parts in each other first, then out of the enclosure, then too
		//near; the most first
	std::stable_sort(problems.begin(), problems.end(), [](const Problem &a, const Problem &b) {
		auto rank = [](Problem::Kind kind) {
			return kind == Problem::Kind::Collision ? 0 : kind == Problem::Kind::Enclosure ? 1 : 2;
		};
		if (rank(a.kind) != rank(b.kind)) return rank(a.kind) < rank(b.kind);
		return a.missing > b.missing;
	});
	return problems;
}

QString sideName(Side side)
{
	switch (side) {
		case Top: return tr("above");
		case Bottom: return tr("below");
		case Left: return tr("on its left");
		case Right: return tr("on its right");
		case Front: return tr("in front");
		case Rear: return tr("behind");
		default: return QString();
	}
}

QString describe(const Problem &problem, const QList<Body> &bodies)
{
	const QString first = problem.first >= 0 && problem.first < bodies.size()
			? nameOf(bodies.at(problem.first)) : QString();
	const QString second = problem.second >= 0 && problem.second < bodies.size()
			? nameOf(bodies.at(problem.second)) : QString();
	switch (problem.kind) {
		case Problem::Kind::Collision:
			return tr("%1 and %2 are %3 mm into each other")
					.arg(first, second, millimetres(problem.missing));
		case Problem::Kind::Clearance:
			if (problem.second < 0) {
				return tr("%1 asks for %2 mm free %3: the enclosure leaves %4 mm too few")
						.arg(first, millimetres(problem.asked), sideName(problem.side),
							 millimetres(problem.missing));
			}
			return tr("%1 asks for %2 mm free %3: %4 is %5 mm too near")
					.arg(first, millimetres(problem.asked), sideName(problem.side), second,
						 millimetres(problem.missing));
		case Problem::Kind::Enclosure:
			if (problem.side == Front) {
				return tr("%1 keeps the door from closing: it is %2 mm too deep")
						.arg(first, millimetres(problem.missing));
			}
			return tr("%1 goes %2 mm through the enclosure %3")
					.arg(first, millimetres(problem.missing), sideName(problem.side));
	}
	return QString();
}

float hit(const QVector<float> &vertices, int first, int count, const QMatrix4x4 &place,
		  const QVector3D &origin, const QVector3D &direction)
{
	float nearest = -1;
	const int size = CabinetStep::vertex_size;
	const int end = qMin(first + count, int(vertices.size() / size));
	for (int v = first ; v + 2 < end ; v += 3)
	{
		auto at = [&](int index) {
			const int i = index * size;
			return place.map(QVector3D(vertices[i], vertices[i + 1], vertices[i + 2]));
		};
		const QVector3D p0 = at(v);
		const QVector3D e1 = at(v + 1) - p0;
		const QVector3D e2 = at(v + 2) - p0;
			//Möller and Trumbore
		const QVector3D h = QVector3D::crossProduct(direction, e2);
		const float a = QVector3D::dotProduct(e1, h);
		if (qAbs(a) < 1e-9f) continue;
		const float f = 1.0f / a;
		const QVector3D s = origin - p0;
		const float u = f * QVector3D::dotProduct(s, h);
		if (u < 0 || u > 1) continue;
		const QVector3D q = QVector3D::crossProduct(s, e1);
		const float w = f * QVector3D::dotProduct(direction, q);
		if (w < 0 || u + w > 1) continue;
		const float t = f * QVector3D::dotProduct(e2, q);
		if (t > 1e-3f && (nearest < 0 || t < nearest)) nearest = t;
	}
	return nearest;
}
}
