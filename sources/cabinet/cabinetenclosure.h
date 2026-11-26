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
#ifndef CABINETENCLOSURE_H
#define CABINETENCLOSURE_H

#include "cabinet3d.h"
#include "cabinetstep.h"

#include <QMatrix4x4>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVector3D>

/**
	@brief The enclosure a mounting plate stands in, in 3D (Brandon,
	2026-11-20): the STEP model of a part of the parts database, a STEP
	file, or a box made of the measures given -- a hollow box of 5 mm
	sheet, open at the front, its plate against the back. The node of the
	model that is the mounting plate takes the plate of the cabinet folio:
	the enclosure is turned and moved so that the front of that node is
	where the front of the folio's plate is, and the folio's own plate is
	not drawn.

	Kept as JSON in the plate (CabinetItem::Properties::enclosure), with
	what is hidden in the 3D view.
*/
namespace CabinetEnclosure
{
	/**
		A device placed in 3D only, not on the plate: a beacon, a push
		button, a switch disconnector on the door or a side (Brandon,
		2026-11-20). It stands on a face of node @p node of the
		enclosure, in the axes of the enclosure's model, or of the
		cabinet when @p node is empty, its back on the face, @p sink mm
		into it.
	*/
	struct Mounted
	{
			/// The symbol of the device, its label, its part
		QUuid symbol;
		QString label;
		QString manufacturer;
		QString order_number;
		QString node;
			/// The point of the face, the face looking out, its top
		QVector3D at;
		QVector3D normal;
		QVector3D up;
			/// Quarter turns about the face
		int quarter_turns = 0;
			/// The side of the model looking out: +z, -z, +x, -x, +y, -y
		int side = 0;
		double sink = 0;

		bool operator==(const Mounted &other) const;
	};

	struct Enclosure
	{
		enum class Source { None, Parts, File, Manual };
		Source source = Source::None;
			/// A part of the parts database
		QString manufacturer;
		QString order_number;
			/// A STEP file
		QString path;
			/// A box made of measures, mm: outside, and the plate
		double width = 600;
		double height = 800;
		double depth = 300;
		double plate_width = 500;
		double plate_height = 700;
			/// The node of the model that is the mounting plate (its id);
			/// "plate" for a box made of measures
		QString plate;
			/// Quarter turns of the enclosure about its plate
		int quarter_turns = 0;
			/// Hidden in 3D: "node:<id>" of the enclosure, "item:<uuid>"
			/// of the folio and of the devices placed in 3D (their symbol)
		QStringList hidden;
			/// The devices placed in 3D only
		QList<Mounted> mounted;
		int mountedIndex(const QUuid &symbol) const;

		bool hasModel() const {return source != Source::None;}
			/// Whether its plate takes the folio's plate
		bool placed() const;
		QString title() const;
		QString toString() const;
		static Enclosure fromString(const QString &text);
		bool operator==(const Enclosure &other) const;
		bool operator!=(const Enclosure &other) const {return !(*this == other);}
	};

	/// Thickness of the sheet and of the plate of a box made of measures
	constexpr double sheet = 5.0;
	constexpr double plate_thickness = 3.0;

		/// The box made of the measures of @p enclosure, as a model: y up,
		/// open towards +z
	CabinetStep::Model manualModel(const Enclosure &enclosure);
		/// The STEP file of @p enclosure, from the parts database or the
		/// disk; empty, saying why, when there is none
	QByteArray stepOf(const Enclosure &enclosure, QString *error = nullptr);

		/// Where @p model goes so that the front of its plate is the front
		/// of @p plate, the folio's plate; @p ok false when it has no plate
	QMatrix4x4 placement(const CabinetStep::Model &model, const Enclosure &enclosure,
						 const Cabinet3d::Solid &plate, bool *ok = nullptr);
		/// The width and height of the plate of @p model once placed, mm
	QSizeF plateSize(const CabinetStep::Model &model, const Enclosure &enclosure,
					 const Cabinet3d::Solid &plate);
		/// Whether node @p index of @p model, or one holding it, is hidden
	bool isHidden(const CabinetStep::Model &model, int index, const QStringList &hidden);

		/// Where the model @p device of @p mounted goes, in the axes it is
		/// placed in (those of the enclosure's model, or of the cabinet)
	QMatrix4x4 mountedPlacement(const Mounted &mounted, const CabinetStep::Model &device);
		/// The top of a face looking towards @p normal: up when it can be,
		/// else towards the back
	QVector3D faceUp(const QVector3D &normal);
}

#endif // CABINETENCLOSURE_H
