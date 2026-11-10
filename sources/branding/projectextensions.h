/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef PROJECTEXTENSIONS_H
#define PROJECTEXTENSIONS_H

#include <QString>
#include <QStringList>

/**
	@brief The extensions of the project files.

	FaseCAD saves its projects as .edrw (XML), .edrwz (zipped) and .edrwdb
	(one database file). The content is the one QElectroTech writes, and
	QElectroTech's own extensions (.qet, .qetz, .qetdb) are still opened:
	which format a file is in is told by either name.
*/
namespace ProjectExtensions
{
		/// The extensions a project is saved with
	inline QString xml() {return QStringLiteral(".edrw");}
	inline QString zipped() {return QStringLiteral(".edrwz");}
	inline QString database() {return QStringLiteral(".edrwdb");}

	inline bool endsWithAny(const QString &path, const QStringList &suffixes)
	{
		for (const QString &suffix : suffixes) {
			if (path.endsWith(suffix, Qt::CaseInsensitive)) {
				return true;
			}
		}
		return false;
	}

	inline bool isZipped(const QString &path)
	{
		return endsWithAny(path, {zipped(), QStringLiteral(".qetz")});
	}

	inline bool isDatabase(const QString &path)
	{
		return endsWithAny(path, {database(), QStringLiteral(".qetdb")});
	}

		/**
			Every project extension, the longest first, so that taking
			them off the end of a name never takes .edrw off a .edrwz
		*/
	inline QStringList all()
	{
		return {database(), zipped(), xml(), QStringLiteral(".qetdb"),
				QStringLiteral(".qetz"), QStringLiteral(".qet")};
	}

		/// The patterns of a file dialog opening a project
	inline QString openPatterns()
	{
		return QStringLiteral("*.edrw *.edrwz *.edrwdb *.qet *.qetz *.qetdb");
	}
}

#endif // PROJECTEXTENSIONS_H
