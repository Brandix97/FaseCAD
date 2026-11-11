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
#include "cabinetsnap.h"

#include "../diagram.h"
#include "cabinetitem.h"

#include <QApplication>
#include <QGraphicsObject>
#include <QGraphicsView>
#include <QHash>
#include <QPainter>
#include <QPointer>

#include <cmath>

namespace {

/**
	The marker of the corner or the middle something caught. It keeps its
	size on the screen whatever the zoom.
*/
class SnapMarker : public QGraphicsObject
{
	public:
		SnapMarker()
		{
			setFlag(ItemIgnoresTransformations, true);
			setAcceptedMouseButtons(Qt::NoButton);
			setAcceptHoverEvents(false);
			setZValue(100001);
		}

		void setAnchor(CabinetSnap::Anchor anchor)
		{
			if (anchor == m_anchor) return;
			m_anchor = anchor;
			update();
		}

		QRectF boundingRect() const override
		{
			return QRectF(-8, -8, 16, 16);
		}

		void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override
		{
			painter->setRenderHint(QPainter::Antialiasing, true);
			QPen pen(QColor(255, 140, 0), 2.0);
			pen.setCosmetic(true);
			painter->setPen(pen);
			painter->setBrush(Qt::NoBrush);
			if (m_anchor == CabinetSnap::Anchor::Corner) {
				painter->drawRect(QRectF(-5, -5, 10, 10));
			} else {
				painter->drawPolygon(QPolygonF({QPointF(0, -6), QPointF(5.5, 4),
												QPointF(-5.5, 4)}));
			}
		}

	private:
		CabinetSnap::Anchor m_anchor = CabinetSnap::Anchor::Corner;
};

QHash<const Diagram *, QPointer<SnapMarker>> &markers()
{
	static QHash<const Diagram *, QPointer<SnapMarker>> all;
	return all;
}

	/// @p value onto the nearest of @p lines within @p reach, if one is
	/// @return the distance it moved, or a NaN if none is near
qreal nearestLine(qreal value, const QVector<qreal> &lines, qreal reach)
{
	qreal best = std::nan("");
	qreal best_distance = reach;
	for (qreal line : lines) {
		const qreal distance = std::abs(line - value);
		if (distance <= best_distance) {
			best_distance = distance;
			best = line - value;
		}
	}
	return best;
}

	/// The smallest move putting one of @p values onto one of @p lines
qreal nearestMove(const QVector<qreal> &values, const QVector<qreal> &lines, qreal reach)
{
	qreal best = std::nan("");
	for (qreal value : values) {
		const qreal move = nearestLine(value, lines, reach);
		if (!std::isnan(move) && (std::isnan(best) || std::abs(move) < std::abs(best)))
			best = move;
	}
	return best;
}

} // namespace

/**
	@brief CabinetSnap::active
	@return true when things catch: the magnet on and Ctrl not held
*/
bool CabinetSnap::active()
{
	return Diagram::gridSnap()
			&& !QApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
}

/**
	@brief CabinetSnap::anchorsOf
	@return the four corners of @p rect and the middles of its four sides
*/
QVector<CabinetSnap::Target> CabinetSnap::anchorsOf(const QRectF &rect)
{
	const QPointF center = rect.center();
	return {
		{rect.topLeft(), Anchor::Corner}, {rect.topRight(), Anchor::Corner},
		{rect.bottomRight(), Anchor::Corner}, {rect.bottomLeft(), Anchor::Corner},
		{QPointF(center.x(), rect.top()), Anchor::Middle},
		{QPointF(rect.right(), center.y()), Anchor::Middle},
		{QPointF(center.x(), rect.bottom()), Anchor::Middle},
		{QPointF(rect.left(), center.y()), Anchor::Middle}};
}

/**
	@brief CabinetSnap::targetsOf
	@return the corners and the middles of the sides of the parts of
	@p diagram, those of @p ignored left out
*/
QVector<CabinetSnap::Target> CabinetSnap::targetsOf(const Diagram *diagram,
													const QSet<const QGraphicsItem *> &ignored)
{
	QVector<Target> targets;
	if (!diagram) return targets;
	const QList<QGraphicsItem *> items = diagram->items();
	for (QGraphicsItem *item : items) {
		if (ignored.contains(item) || !item->isVisible()) continue;
		const auto *part = qgraphicsitem_cast<const CabinetItem *>(item);
		if (!part) continue;
		targets << anchorsOf(part->sceneRect());
	}
	return targets;
}

/**
	@brief CabinetSnap::tolerance
	@return how near a corner or a middle catches, in the pixels of the
	folio: ten pixels of the screen, whatever the zoom
*/
qreal CabinetSnap::tolerance(const Diagram *diagram)
{
	qreal zoom = 1.0;
	if (diagram && !diagram->views().isEmpty())
		zoom = diagram->views().constFirst()->transform().m11();
	return 10.0 / qMax(zoom, 0.05);
}

/**
	@brief CabinetSnap::roundMm
	@return @p value, in the pixels of @p diagram, on a whole millimetre
	of the cabinet
*/
qreal CabinetSnap::roundMm(qreal value, const Diagram *diagram)
{
	const qreal pixels = diagram ? diagram->pixelsPerMm() : 1.0;
	if (pixels <= 0) return value;
	return std::round(value / pixels) * pixels;
}

/**
	@brief CabinetSnap::point
	@return where the point @p point goes: onto the nearest corner or
	middle of a part, else onto the line of one along each direction,
	else on the millimetre
*/
CabinetSnap::Snap CabinetSnap::point(const QPointF &point, const Diagram *diagram,
									 const QSet<const QGraphicsItem *> &ignored)
{
	Snap snap;
	snap.value = point;
	if (!active() || !diagram) return snap;

	const QVector<Target> targets = targetsOf(diagram, ignored);
	const qreal reach = tolerance(diagram);
	qreal best = reach;
	for (const Target &target : targets) {
		const qreal distance = std::hypot(target.point.x() - point.x(),
										  target.point.y() - point.y());
		if (distance <= best) {
			best = distance;
			snap.found = true;
			snap.at = target.point;
			snap.anchor = target.anchor;
		}
	}
	if (snap.found) {
		snap.value = snap.at;
		snap.caught_x = snap.caught_y = true;
		return snap;
	}

	QVector<qreal> xs, ys;
	for (const Target &target : targets) {
		xs << target.point.x();
		ys << target.point.y();
	}
	const qreal move_x = nearestLine(point.x(), xs, reach);
	const qreal move_y = nearestLine(point.y(), ys, reach);
	snap.caught_x = !std::isnan(move_x);
	snap.caught_y = !std::isnan(move_y);
	snap.value.setX(snap.caught_x ? point.x() + move_x : roundMm(point.x(), diagram));
	snap.value.setY(snap.caught_y ? point.y() + move_y : roundMm(point.y(), diagram));
	return snap;
}

/**
	@brief CabinetSnap::topLeft
	@return where the top left corner of a part goes, the part dropped
	where @p rect is: one of its corners or middles onto the nearest corner
	or middle of another part, else lined up with one along each
	direction, else on the millimetre
*/
CabinetSnap::Snap CabinetSnap::topLeft(const QRectF &rect, const Diagram *diagram,
									   const QSet<const QGraphicsItem *> &ignored)
{
	Snap snap;
	snap.value = rect.topLeft();
	if (!active() || !diagram) return snap;

	const QVector<Target> targets = targetsOf(diagram, ignored);
	const QVector<Target> anchors = anchorsOf(rect);
	const qreal reach = tolerance(diagram);
	qreal best = reach;
	QPointF move;
	for (const Target &target : targets) {
		for (const Target &anchor : anchors) {
			const QPointF delta = target.point - anchor.point;
			const qreal distance = std::hypot(delta.x(), delta.y());
			if (distance <= best) {
				best = distance;
				move = delta;
				snap.found = true;
				snap.at = target.point;
				snap.anchor = target.anchor;
			}
		}
	}
	if (snap.found) {
		snap.value = rect.topLeft() + move;
		snap.caught_x = snap.caught_y = true;
		return snap;
	}

	QVector<qreal> xs, ys;
	for (const Target &target : targets) {
		xs << target.point.x();
		ys << target.point.y();
	}
	const qreal move_x = nearestMove({rect.left(), rect.center().x(), rect.right()}, xs, reach);
	const qreal move_y = nearestMove({rect.top(), rect.center().y(), rect.bottom()}, ys, reach);
	snap.caught_x = !std::isnan(move_x);
	snap.caught_y = !std::isnan(move_y);
	snap.value.setX(snap.caught_x ? rect.left() + move_x : roundMm(rect.left(), diagram));
	snap.value.setY(snap.caught_y ? rect.top() + move_y : roundMm(rect.top(), diagram));
	return snap;
}

/**
	@brief CabinetSnap::showMarker
	Show on @p diagram the corner or the middle @p snap caught, or nothing
	if it caught none.
*/
void CabinetSnap::showMarker(Diagram *diagram, const Snap &snap)
{
	if (!diagram) return;
	if (!snap.found) {
		hideMarker(diagram);
		return;
	}
	QPointer<SnapMarker> &marker = markers()[diagram];
	if (!marker) {
		marker = new SnapMarker();
		diagram->QGraphicsScene::addItem(marker);
	}
	marker->setAnchor(snap.anchor);
	marker->setPos(snap.at);
	marker->show();
}

void CabinetSnap::hideMarker(Diagram *diagram)
{
	const auto found = markers().constFind(diagram);
	if (found == markers().constEnd()) return;
	if (found.value()) found.value()->hide();
}

bool CabinetSnap::markerShown(const Diagram *diagram)
{
	const QPointer<SnapMarker> marker = markers().value(diagram);
	return marker && marker->isVisible();
}
