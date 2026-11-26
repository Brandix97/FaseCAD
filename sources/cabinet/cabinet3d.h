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
#ifndef CABINET3D_H
#define CABINET3D_H

#include <QByteArray>
#include <QColor>
#include <QList>
#include <QString>
#include <QUuid>
#include <QVector>
#include <QVector3D>

class Diagram;

/**
	@brief The cabinet of a cabinet folio in three dimensions (Brandon,
	2026-11-20): every part on the folio becomes a solid standing on the
	mounting plate, in millimetres, x to the right, y up, z out of the
	plate towards the one looking at it.

	The plate is 2 mm thick behind z = 0; a rail stands its profile's
	depth off the plate, a duct its height (60 mm when not given); a
	device stands on the rails under it, as deep as its part says in the
	parts database (60 mm when it does not), and takes the part's STEP
	model when the part has one.
*/
namespace Cabinet3d
{
	/**
		The free space the part of a device asks around it, mm: its
		mounting clearances in the parts database (Brandon, 2026-11-21),
		as it stands on the plate
	*/
	struct Clearance
	{
		float top = 0;
		float bottom = 0;
		float left = 0;
		float right = 0;
		float front = 0;
		float rear = 0;

		bool any() const
		{return top > 0 || bottom > 0 || left > 0 || right > 0 || front > 0 || rear > 0;}
	};

	struct Solid
	{
		enum class Kind { Plate, Rail, Duct, Device };
		Kind kind = Kind::Device;
			/// The label of a device, the description of another part
		QString name;
			/// The part on the folio (CabinetItem::uuid())
		QUuid item;
			/// The box it takes, mm
		QVector3D min;
		QVector3D max;
		QColor color;
			/// A device: its part, as "manufacturer order number", and
			/// the STEP model of the part, empty when it has none
		QString part;
		QByteArray step;
		QString step_name;
			/// A plate: the enclosure it stands in, as JSON
			/// (CabinetEnclosure), and the STEP of that enclosure, or why
			/// there is none
		QString enclosure;
		QByteArray enclosure_step;
		QString enclosure_error;
			/// A device placed in 3D only: its place in the list of its
			/// plate's enclosure (CabinetEnclosure::Enclosure::mounted);
			/// its box is where its model stands, worked out when drawn
		int mounted = -1;
			/// A device on the plate: the free space its part asks
		Clearance clearance;

		QVector3D size() const {return max - min;}
		QVector3D center() const {return (min + max) / 2;}
	};

	/// Thickness of a mounting plate, mm
	constexpr float plate_thickness = 2.0f;
	/// Height of a duct and depth of a device when not given, mm
	constexpr float default_duct_height = 60.0f;
	constexpr float default_device_depth = 60.0f;

		/// The solids of the cabinet drawn on @p cabinet; the parts
		/// database is read when @p with_parts (depth and STEP model of
		/// each device)
	QList<Solid> solidsOf(Diagram *cabinet, bool with_parts = true);
		/// The box holding every solid of @p solids
	void bounds(const QList<Solid> &solids, QVector3D &min, QVector3D &max);

	/**
		A device of the schematic with a STEP model in the parts database
		and no drawing on a cabinet folio: a beacon, a push button, a
		switch disconnector, to place in 3D on the door or a side
	*/
	struct Unplaced
	{
		QUuid symbol;
		QString label;
		QString description;
		QString manufacturer;
		QString order_number;
		QByteArray step;
		QString step_name;
	};
		/// The devices of the project of @p cabinet to place in 3D, but
		/// those of @p mounted, placed already: with their STEP model, or
		/// a box of their mounting measures when they have none
	QList<Unplaced> unplacedOf(Diagram *cabinet, const QList<QUuid> &mounted = {});
		/// The STEP model of a part of the parts database, empty when it
		/// has none; its file name into @p file_name
	QByteArray stepOfPart(const QString &manufacturer, const QString &order_number,
						  QString *file_name = nullptr);
		/// A STEP model of a box of @p size mm (width, height, depth), for
		/// a device without one: empty without OpenCASCADE
	QByteArray boxStep(const QVector3D &size);

	/// A box of what a solid is made of
	struct Piece
	{
		QVector3D min;
		QVector3D max;
		QColor color;
	};
		/// What @p solid is made of, as it is (Brandon, 2026-11-20): a
		/// rail its top hat profile of 1 mm sheet with the long holes of
		/// its bottom, a duct its bottom, its slotted walls and, with
		/// @p covers, its cover; one box for a plate or a device
	QVector<Piece> piecesOf(const Solid &solid, bool covers = true);
}

#endif // CABINET3D_H
