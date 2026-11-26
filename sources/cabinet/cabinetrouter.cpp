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
#include "cabinetrouter.h"

#include <QLineF>

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace
{
	constexpr qreal epsilon = 1e-6;
		/// A wire out of the ducts counts double when the way is chosen:
		/// as short, the way inside the ducts is the one taken. The length
		/// given is the real one.
	constexpr qreal outside_weight = 2.0;

	qreal manhattan(const QPointF &a, const QPointF &b)
	{
		return std::abs(a.x() - b.x()) + std::abs(a.y() - b.y());
	}
}

QPointF CabinetRouter::Line::at(qreal t) const
{
	return orientation == Qt::Horizontal ? QPointF(t, level) : QPointF(level, t);
}

qreal CabinetRouter::Line::along(const QPointF &p) const
{
	return orientation == Qt::Horizontal ? p.x() : p.y();
}

qreal CabinetRouter::Line::clampAlong(qreal t) const
{
	return qBound(from, t, to);
}

CabinetRouter::CabinetRouter(const QList<Duct> &ducts, const QList<QRectF> &devices,
							 qreal tolerance)
{
	for (const QRectF &rect : devices) {
		m_devices << rect.normalized();
	}
	QVector<QRectF> rects;
	for (const Duct &duct : ducts)
	{
		const QRectF rect = duct.rect.normalized();
		if (rect.isEmpty()) {
			continue;
		}
		Line line;
		line.orientation = duct.orientation;
		if (duct.orientation == Qt::Horizontal) {
			line.level = rect.center().y();
			line.from = rect.left();
			line.to = rect.right();
		} else {
			line.level = rect.center().x();
			line.from = rect.top();
			line.to = rect.bottom();
		}
		m_lines << line;
		rects << rect;
	}

		//Where two ducts meet: the nearest points of their middle lines
	for (int i = 0 ; i < m_lines.size() ; ++i)
	{
		const QRectF reach = rects.at(i).adjusted(-tolerance, -tolerance, tolerance, tolerance);
		for (int j = i + 1 ; j < m_lines.size() ; ++j)
		{
			if (!reach.intersects(rects.at(j))) {
				continue;
			}
			const Line &a = m_lines.at(i);
			const Line &b = m_lines.at(j);
			Join join{i, 0, j, 0};
			if (a.orientation != b.orientation) {
					//A T, a corner or a crossing
				join.t_a = a.clampAlong(b.level);
				join.t_b = b.clampAlong(a.level);
			} else {
				const qreal low = qMax(a.from, b.from);
				const qreal high = qMin(a.to, b.to);
				if (low <= high) {
						//Side by side, or one over the other
					join.t_a = join.t_b = (low + high) / 2.0;
				} else if (a.to < b.from) {
						//One after the other
					join.t_a = a.to;
					join.t_b = b.from;
				} else {
					join.t_a = a.from;
					join.t_b = b.to;
				}
			}
			m_joins << join;
		}
	}
}

/**
	@brief CabinetRouter::route
	@return the shortest way from @p from to @p to through the ducts, or a
	straight line between them when no duct joins them
*/
CabinetRouter::Route CabinetRouter::route(const QPointF &from, const QPointF &to,
										  const QRectF &from_rect, const QRectF &to_rect) const
{
	Route straight;
	straight.points = {from, to};
	straight.length = QLineF(from, to).length();
	if (m_lines.isEmpty()) {
		return straight;
	}

		//The stops of each line: its ends, where it meets another one, and
		//where each of the two devices goes into it
	QVector<QVector<qreal>> stops(m_lines.size());
	for (int i = 0 ; i < m_lines.size() ; ++i) {
		const Line &line = m_lines.at(i);
		stops[i] << line.from << line.to
				 << line.clampAlong(line.along(from))
				 << line.clampAlong(line.along(to));
	}
	for (const Join &join : m_joins) {
		stops[join.line_a] << join.t_a;
		stops[join.line_b] << join.t_b;
	}

		//One node per stop; 0 and 1 are the two devices
	struct Node
	{
		int line;
		qreal t;
		QPointF point;
	};
	QVector<Node> nodes{{-1, 0, from}, {-1, 0, to}};
	QVector<QVector<int>> node_of(m_lines.size());
	for (int i = 0 ; i < m_lines.size() ; ++i)
	{
		QVector<qreal> &list = stops[i];
		std::sort(list.begin(), list.end());
		QVector<qreal> unique;
		for (const qreal t : list) {
			if (unique.isEmpty() || t - unique.last() > epsilon) {
				unique << t;
			}
		}
		list = unique;
		for (const qreal t : list) {
			node_of[i] << nodes.size();
			nodes << Node{i, t, m_lines.at(i).at(t)};
		}
	}
	auto nodeAt = [&](int line, qreal t) {
		const QVector<qreal> &list = stops.at(line);
		int best = 0;
		for (int k = 1 ; k < list.size() ; ++k) {
			if (std::abs(list.at(k) - t) < std::abs(list.at(best) - t)) {
				best = k;
			}
		}
		return node_of.at(line).at(best);
	};

	QVector<QVector<QPair<int, qreal>>> edges(nodes.size());
	auto link = [&edges](int a, int b, qreal cost) {
		edges[a] << qMakePair(b, cost);
		edges[b] << qMakePair(a, cost);
	};
	for (int i = 0 ; i < m_lines.size() ; ++i) {
		const QVector<int> &list = node_of.at(i);
		for (int k = 1 ; k < list.size() ; ++k) {
			link(list.at(k - 1), list.at(k),
				 nodes.at(list.at(k)).t - nodes.at(list.at(k - 1)).t);
		}
	}
	for (const Join &join : m_joins) {
		const int a = nodeAt(join.line_a, join.t_a);
		const int b = nodeAt(join.line_b, join.t_b);
		link(a, b, outside_weight * manhattan(nodes.at(a).point, nodes.at(b).point));
	}
		//A straight way from a device into a duct crossing another device
	auto blocked = [this](const QPointF &a, const QPointF &b, const QRectF &own) {
		for (const QRectF &rect : m_devices)
		{
			if (!own.isNull() && rect == own.normalized()) {
				continue;
			}
			const QRectF inner = rect.adjusted(1, 1, -1, -1);
			if (inner.width() <= 0 || inner.height() <= 0) {
				continue;
			}
			if (std::abs(a.x() - b.x()) < epsilon) {
				const qreal low = qMin(a.y(), b.y());
				const qreal high = qMax(a.y(), b.y());
				if (a.x() > inner.left() && a.x() < inner.right()
					&& high > inner.top() && low < inner.bottom()) {
					return true;
				}
			} else if (std::abs(a.y() - b.y()) < epsilon) {
				const qreal low = qMin(a.x(), b.x());
				const qreal high = qMax(a.x(), b.x());
				if (a.y() > inner.top() && a.y() < inner.bottom()
					&& high > inner.left() && low < inner.right()) {
					return true;
				}
			}
		}
		return false;
	};
		//Where a device goes into the ducts: square into the horizontal
		//ducts right above or below it, else into the vertical ones beside
		//it, else to the nearest end of each duct
	auto entriesOf = [&](const QPointF &centre, const QRectF &own) {
		QVector<QPair<int, qreal>> entries;
		for (const Qt::Orientation wanted : {Qt::Horizontal, Qt::Vertical}) {
			for (int i = 0 ; i < m_lines.size() ; ++i) {
				const Line &line = m_lines.at(i);
				if (line.orientation != wanted) {
					continue;
				}
				const qreal t = line.along(centre);
				if (t < line.from - epsilon || t > line.to + epsilon
					|| blocked(centre, line.at(t), own)) {
					continue;
				}
				entries << qMakePair(i, line.clampAlong(t));
			}
			if (!entries.isEmpty()) {
				return entries;
			}
		}
		for (int i = 0 ; i < m_lines.size() ; ++i) {
			entries << qMakePair(i, m_lines.at(i).clampAlong(m_lines.at(i).along(centre)));
		}
		return entries;
	};
	for (int device = 0 ; device < 2 ; ++device) {
		const QPointF centre = nodes.at(device).point;
		for (const auto &entry : entriesOf(centre, device == 0 ? from_rect : to_rect)) {
			const int node = nodeAt(entry.first, entry.second);
			link(device, node, outside_weight * manhattan(centre, nodes.at(node).point));
		}
	}

		//The shortest way from the first device to the second (Dijkstra)
	const qreal infinite = std::numeric_limits<qreal>::infinity();
	QVector<qreal> distance(nodes.size(), infinite);
	QVector<int> previous(nodes.size(), -1);
	using Entry = std::pair<qreal, int>;
	std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
	distance[0] = 0;
	queue.push({0, 0});
	while (!queue.empty())
	{
		const auto [cost, node] = queue.top();
		queue.pop();
		if (cost > distance.at(node) + epsilon) {
			continue;
		}
		if (node == 1) {
			break;
		}
		for (const auto &edge : edges.at(node))
		{
			const qreal next = cost + edge.second;
			if (next + epsilon < distance.at(edge.first)) {
				distance[edge.first] = next;
				previous[edge.first] = node;
				queue.push({next, edge.first});
			}
		}
	}
	if (previous.at(1) < 0) {
		return straight;
	}

	QVector<int> path;
	for (int node = 1 ; node >= 0 ; node = previous.at(node)) {
		path.prepend(node);
		if (node == 0) {
			break;
		}
	}

		//Square corners: along the line of a duct first when on one, and
		//square into the duct when coming from a device
	QVector<QPointF> points;
	for (int k = 0 ; k < path.size() ; ++k)
	{
		const Node &here = nodes.at(path.at(k));
		if (k > 0)
		{
			const Node &before = nodes.at(path.at(k - 1));
			const QPointF p = before.point;
			const QPointF q = here.point;
			if (std::abs(p.x() - q.x()) > epsilon && std::abs(p.y() - q.y()) > epsilon)
			{
				const int line = before.line >= 0 ? before.line : here.line;
				const bool horizontal = m_lines.at(line).orientation == Qt::Horizontal;
				const bool along_first = before.line >= 0;
					//On a horizontal line, along it first; from a device into
					//a horizontal line, square to it (vertical) first
				const bool horizontal_first = along_first ? horizontal : !horizontal;
				points << (horizontal_first ? QPointF(q.x(), p.y()) : QPointF(p.x(), q.y()));
			}
		}
		points << here.point;
	}

	Route route;
	route.points = simplified(points);
	route.length = lengthOf(route.points);
	route.through_ducts = true;
	return route;
}

qreal CabinetRouter::lengthOf(const QVector<QPointF> &points)
{
	qreal length = 0;
	for (int k = 1 ; k < points.size() ; ++k) {
		length += QLineF(points.at(k - 1), points.at(k)).length();
	}
	return length;
}

QVector<QPointF> CabinetRouter::simplified(const QVector<QPointF> &points)
{
	QVector<QPointF> result;
	for (const QPointF &point : points)
	{
		if (!result.isEmpty() && manhattan(result.last(), point) < epsilon) {
			continue;
		}
			//The middle of three points in a straight line goes -- not
			//where the way turns back on itself
		if (result.size() >= 2) {
			const QPointF a = result.at(result.size() - 2);
			const QPointF b = result.last();
			const bool same_x = std::abs(a.x() - b.x()) < epsilon && std::abs(b.x() - point.x()) < epsilon;
			const bool same_y = std::abs(a.y() - b.y()) < epsilon && std::abs(b.y() - point.y()) < epsilon;
			const bool onwards = same_x ? (b.y() - a.y()) * (point.y() - b.y()) >= 0
										: (b.x() - a.x()) * (point.x() - b.x()) >= 0;
			if ((same_x || same_y) && onwards) {
				result.last() = point;
				continue;
			}
		}
		result << point;
	}
	return result;
}
