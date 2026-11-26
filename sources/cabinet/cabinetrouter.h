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
#ifndef CABINETROUTER_H
#define CABINETROUTER_H

#include <QList>
#include <QPointF>
#include <QRectF>
#include <QVector>

/**
	@brief The CabinetRouter class finds the way a wire takes through the
	wire ducts of a mounting plate (Brandon, 2026-11-19): from the centre
	of one device to the duct, along the ducts, and out to the centre of
	the other device, the shortest such way.

	Everything is in millimetres, the plate's own measures. A duct is a
	line along its middle; two ducts meet where they touch or cross (a T, a
	corner, one after the other, side by side), at their nearest points.

	A device goes from its centre square into the ducts it faces, the way
	its wires leave it in a cabinet: up or down into a horizontal duct
	right above or below it; only when there is none, sideways into a
	vertical duct beside it; only when there is none either, to the nearest
	end of a duct. The way into a duct never crosses another device. Out
	of the ducts a millimetre counts double when the way is chosen, so that
	of two ways as long the one inside the ducts is taken; the length given
	is the real one. Nothing of the drawing is known here: the routing
	sheet gives the ducts and the centres.
*/
class CabinetRouter
{
	public:
		struct Duct
		{
			QRectF rect;
			Qt::Orientation orientation = Qt::Horizontal;
		};

		struct Route
		{
				/// From the centre of the first device to the centre of
				/// the second one, square corners only through the ducts
			QVector<QPointF> points;
			qreal length = 0;
				/// False: no duct joins the two devices, the route is a
				/// straight line between them
			bool through_ducts = false;
		};

			/// @p tolerance: how far apart two ducts may be and still
			/// meet, in millimetres
			/// @p devices: the drawings of the devices on the plate, which
			/// the way into a duct does not cross
		explicit CabinetRouter(const QList<Duct> &ducts,
							   const QList<QRectF> &devices = {},
							   qreal tolerance = 2.0);

			/// The way between the centres of @p from and @p to; their own
			/// rectangles (when known) do not stop their way into a duct
		Route route(const QPointF &from, const QPointF &to,
					const QRectF &from_rect = QRectF(),
					const QRectF &to_rect = QRectF()) const;

			/// The length of a route along @p points
		static qreal lengthOf(const QVector<QPointF> &points);
			/// @p points without repeated points and without the points in
			/// the middle of a straight run
		static QVector<QPointF> simplified(const QVector<QPointF> &points);

	private:
		struct Line
		{
			Qt::Orientation orientation = Qt::Horizontal;
				/// The fixed coordinate (y of a horizontal line)
			qreal level = 0;
				/// Where it starts and ends along itself
			qreal from = 0;
			qreal to = 0;
			QPointF at(qreal t) const;
			qreal along(const QPointF &p) const;
			qreal clampAlong(qreal t) const;
		};
		struct Join
		{
			int line_a;
			qreal t_a;
			int line_b;
			qreal t_b;
		};

		QVector<Line> m_lines;
		QVector<Join> m_joins;
		QVector<QRectF> m_devices;
};

#endif // CABINETROUTER_H
