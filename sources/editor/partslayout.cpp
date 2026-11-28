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
#include "partslayout.h"

#include <QCoreApplication>
#include <QGraphicsItem>

#include <algorithm>
#include <numeric>

int PartsLayout::minimumCount(Mode mode)
{
	return mode == Mode::DistributeHorizontally
			|| mode == Mode::DistributeVertically ? 3 : 2;
}

QList<QPointF> PartsLayout::offsets(const QList<QRectF> &rects, Mode mode)
{
	QList<QPointF> result(rects.size(), QPointF());
	if (rects.size() < minimumCount(mode)) return result;

	QRectF all = rects.first();
	for (const QRectF &r : rects) all = all.united(r);

	if (mode == Mode::DistributeHorizontally || mode == Mode::DistributeVertically) {
		const bool horizontal = mode == Mode::DistributeHorizontally;
		auto start = [horizontal](const QRectF &r) {return horizontal ? r.left() : r.top();};
		auto size = [horizontal](const QRectF &r) {return horizontal ? r.width() : r.height();};
		auto centre = [horizontal](const QRectF &r) {
			return horizontal ? r.center().x() : r.center().y();};

			//In the order they stand, by their centres
		QList<int> order(rects.size());
		std::iota(order.begin(), order.end(), 0);
		std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
			return centre(rects.at(a)) < centre(rects.at(b));});

		const QRectF &first = rects.at(order.first());
		const QRectF &last = rects.at(order.last());
		const qreal span = (start(last) + size(last)) - start(first);
		qreal sizes = 0;
		for (const QRectF &r : rects) sizes += size(r);
		const qreal gap = (span - sizes) / (rects.size() - 1);

		qreal at = start(first) + size(first) + gap;
		for (int i = 1; i < order.size() - 1; ++i) {
			const QRectF &r = rects.at(order.at(i));
			const qreal move = at - start(r);
			result[order.at(i)] = horizontal ? QPointF(move, 0) : QPointF(0, move);
			at += size(r) + gap;
		}
		return result;
	}

	for (int i = 0; i < rects.size(); ++i) {
		const QRectF &r = rects.at(i);
		switch (mode) {
			case Mode::AlignLeft:    result[i] = QPointF(all.left() - r.left(), 0); break;
			case Mode::AlignHCenter: result[i] = QPointF(all.center().x() - r.center().x(), 0); break;
			case Mode::AlignRight:   result[i] = QPointF(all.right() - r.right(), 0); break;
			case Mode::AlignTop:     result[i] = QPointF(0, all.top() - r.top()); break;
			case Mode::AlignVCenter: result[i] = QPointF(0, all.center().y() - r.center().y()); break;
			case Mode::AlignBottom:  result[i] = QPointF(0, all.bottom() - r.bottom()); break;
			default: break;
		}
	}
	return result;
}

PartsLayoutCommand::PartsLayoutCommand(const QList<QGraphicsItem *> &parts,
									   PartsLayout::Mode mode,
									   QUndoCommand *parent) :
	QUndoCommand(parent)
{
	const bool distribute = mode == PartsLayout::Mode::DistributeHorizontally
			|| mode == PartsLayout::Mode::DistributeVertically;
	setText(distribute
			? QCoreApplication::translate("PartsLayout", "Distribute")
			: QCoreApplication::translate("PartsLayout", "Align"));
	QList<QRectF> rects;
	for (QGraphicsItem *part : parts) rects << part->sceneBoundingRect();
	const QList<QPointF> moves = PartsLayout::offsets(rects, mode);
	for (int i = 0; i < parts.size(); ++i) {
		if (qFuzzyIsNull(moves.at(i).x()) && qFuzzyIsNull(moves.at(i).y())) continue;
		m_parts << parts.at(i);
		m_offsets << moves.at(i);
	}
}

void PartsLayoutCommand::redo()
{
	for (int i = 0; i < m_parts.size(); ++i)
		m_parts.at(i)->moveBy(m_offsets.at(i).x(), m_offsets.at(i).y());
}

void PartsLayoutCommand::undo()
{
	for (int i = 0; i < m_parts.size(); ++i)
		m_parts.at(i)->moveBy(-m_offsets.at(i).x(), -m_offsets.at(i).y());
}
