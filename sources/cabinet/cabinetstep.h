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
#ifndef CABINETSTEP_H
#define CABINETSTEP_H

#include "cabinet3d.h"

#include <QByteArray>
#include <QColor>
#include <QMatrix4x4>
#include <QString>
#include <QVector>
#include <QVector3D>

/**
	@brief STEP for the cabinet in 3D (Brandon, 2026-11-20), through
	OpenCASCADE: the model of a device read from the STEP file of its part,
	in triangles with its colours, and the whole cabinet written as a STEP
	assembly for the mechanical CAD. Built without OpenCASCADE, available()
	is false, nothing is read and nothing written.

	A manufacturer's model has its own axes: it is turned the way its size
	best matches the device's drawing (width, height, then depth), and set
	in the middle of the drawing with its back on what it stands on.
*/
namespace CabinetStep
{
	bool available();

	/**
		A part or an assembly of a model, in the order the file has them,
		an assembly before what it holds: its vertices are those from
		first, count of them, its own and those of what it holds
	*/
	struct Node
	{
		QString name;
			/// Where it is in the file, the same each time it is read
		QString id;
		int parent = -1;
		int depth = 0;
		bool assembly = false;
		int first = 0;
		int count = 0;
		QVector3D min;
		QVector3D max;
	};

	struct Model
	{
		bool ok = false;
		QString error;
			/// Triangles, three vertices each: x y z, nx ny nz, r g b a;
			/// mm, in the model's own axes
		QVector<float> vertices;
		QVector3D min;
		QVector3D max;
		QVector<Node> nodes;

		int node(const QString &id) const;
	};
		/// Floats of one vertex in Model::vertices
	constexpr int vertex_size = 10;

		/// The model of the STEP file @p step; read once, then kept, in
		/// memory and in a folder of the computer (setCacheFolder())
	Model read(const QByteArray &step);
		/// Whether @p step is read already, in memory or on disk: read()
		/// then gives it at once. Its model into @p model.
	bool cached(const QByteArray &step, Model *model = nullptr);
		/// Where the models read are kept on disk; by default the cache
		/// folder of the program, "step"
	void setCacheFolder(const QString &folder);
	QString cacheFolder();

		/// The six sides of the box @p min to @p max, two triangles each,
		/// as Model::vertices has them
	void appendBox(QVector<float> &vertices, const QVector3D &min, const QVector3D &max,
				   const QColor &color, float alpha = 1.0f);

		/// The 24 turns of a box onto itself, the one leaving it as it is
		/// first
	QList<QMatrix4x4> boxTurns();
		/// The turn of a model @p size big that best fits a place @p target
		/// big (x wide, y high, z deep); a target depth of 0 is not matched
	QMatrix4x4 orientation(const QVector3D &size, const QVector3D &target);
		/// Where the model of @p model goes to stand for @p solid
	QMatrix4x4 placement(const Model &model, const Cabinet3d::Solid &solid);

		/// A STEP model to write where it stands: an enclosure
	struct Placed
	{
		QByteArray step;
		QMatrix4x4 place;
		QString name;
	};

		/// Write @p solids as a STEP assembly to @p path: boxes, the
		/// devices' models where they stand, and @p models; false, saying
		/// why, when it cannot
	bool write(const QList<Cabinet3d::Solid> &solids, const QString &path,
			   QString *error = nullptr, const QList<Placed> &models = {});
}

#endif // CABINETSTEP_H
