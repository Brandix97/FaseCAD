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
#ifndef DXFREADER_H
#define DXFREADER_H

#include <QCoreApplication>
#include <QMap>
#include <QPainterPath>
#include <QString>

/**
	@brief The DxfReader class reads the lines of a 2D drawing in the DXF
	format, as manufacturers give the front view of their devices.

	What it reads: LINE, LWPOLYLINE and POLYLINE (with their arcs), ARC,
	CIRCLE, ELLIPSE, SPLINE, SOLID, and the blocks an INSERT places. The
	rest (texts, hatches, dimensions...) is counted and left out. The
	drawing comes out as one path in millimetres, whatever the unit of the
	file, the y axis pointing down as on a folio, its top left corner at 0.
*/
class DxfReader
{
	Q_DECLARE_TR_FUNCTIONS(DxfReader)

	public:
		struct Result {
			bool ok = false;
				/// Why the file could not be read, if it could not
			QString error;
				/// The lines of the drawing, in millimetres
			QPainterPath path;
				/// How many entities were drawn
			int drawn = 0;
				/// The kinds of entity left out, and how many of each
			QMap<QString, int> skipped;
				/// Millimetres for one unit of the file
			double unit = 1.0;
		};

		static Result readFile(const QString &file_path);
		static Result read(const QByteArray &data);
		static double millimetresPerUnit(int insunits);

		static QString pathToString(const QPainterPath &path);
		static QPainterPath pathFromString(const QString &text, bool *ok = nullptr);
};

#endif // DXFREADER_H
