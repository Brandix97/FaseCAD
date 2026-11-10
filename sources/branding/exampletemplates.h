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
#ifndef EXAMPLETEMPLATES_H
#define EXAMPLETEMPLATES_H

#include <QString>
#include <QStringList>

/**
	@brief The example templates FaseCAD ships with (direct-on-line and
	star-delta starts, digital and analog I/O cards), copied into the
	user's templates so they show in Collections > Templates.

	Each example is copied once : the user may change it or delete it,
	and a deleted example does not come back. A later version shipping a
	new example copies that one.
*/
namespace ExampleTemplates
{
		/// Where the examples are, in the program's resources
	QString resourceDir();
		/// The folder of the user's templates they are copied into
	QString folderName();
		/**
			Copy into @p templates_dir/folderName() every .qetmak of
			@p source_dir not copied before.
			@return the names of the files copied
		*/
	QStringList install(const QString &source_dir,
						const QString &templates_dir);
}

#endif // EXAMPLETEMPLATES_H
