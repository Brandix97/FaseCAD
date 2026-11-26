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
#ifndef CABINETSPACE_H
#define CABINETSPACE_H

#include "cabinet3d.h"

#include <QCoreApplication>
#include <QList>
#include <QMatrix4x4>
#include <QString>
#include <QVector>
#include <QVector3D>

/**
	@brief Whether the cabinet has room for what is in it (Brandon,
	2026-11-21), with boxes: the box each part takes and the free space
	the part of a device asks around it (its mounting clearances in the
	parts database).

	Three kinds of problem: two parts in each other; a part in the free
	space a device asks; a device out of the enclosure, through its walls
	or its closed door. The devices placed in 3D on the door or a side
	count as the others: a push button behind the door hitting a device of
	the plate is two parts in each other.

	Boxes of the cabinet: mm, x to the right, y up, z out of the plate.
*/
namespace CabinetSpace
{
	/// The sides of a box: the free space a device asks, a wall
	enum Side { Top, Bottom, Left, Right, Front, Rear, SideCount };

	/// A part of the cabinet, as it stands
	struct Body
	{
			/// Its solid in the 3D view
		int solid = -1;
		Cabinet3d::Solid::Kind kind = Cabinet3d::Solid::Kind::Device;
			/// A device placed in 3D, on the door or a side
		bool mounted = false;
		QString name;
		QVector3D min;
		QVector3D max;
		Cabinet3d::Clearance clearance;
	};

	struct Problem
	{
		enum class Kind {
				/// Two parts in each other, first and second
			Collision,
				/// Part second, or the enclosure when -1, in the free
				/// space side of device first asks
			Clearance,
				/// Device first through the wall side of the enclosure:
				/// the door when it is the front
			Enclosure
		};
		Kind kind = Kind::Collision;
			/// Indexes of the bodies checked
		int first = -1;
		int second = -1;
		Side side = Top;
			/// The free space asked, mm
		float asked = 0;
			/// How much is missing, mm
		float missing = 0;
			/// Where, to be shown
		QVector3D min;
		QVector3D max;
	};

		/// The free space side @p side of @p body asks, as a box; false
		/// when it asks none
	bool zone(const Body &body, Side side, QVector3D &min, QVector3D &max);
		/// The problems of @p bodies, the worst first; @p inside the room
		/// inside the enclosure when @p has_inside
	QList<Problem> check(const QList<Body> &bodies, bool has_inside = false,
						 const QVector3D &inside_min = QVector3D(),
						 const QVector3D &inside_max = QVector3D());
		/// What problem @p problem of @p bodies is, as the user reads it
	QString describe(const Problem &problem, const QList<Body> &bodies);
		/// The name of side @p side, as the user reads it
	QString sideName(Side side);

		/// How far the ray from @p origin along @p direction goes before
		/// it meets one of the triangles @p first to @p first + @p count
		/// of @p vertices (CabinetStep::Model::vertices) moved by
		/// @p place; -1 when it meets none
	float hit(const QVector<float> &vertices, int first, int count, const QMatrix4x4 &place,
			  const QVector3D &origin, const QVector3D &direction);
}

#endif // CABINETSPACE_H
