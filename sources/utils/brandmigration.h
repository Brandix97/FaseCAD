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
#ifndef BRANDMIGRATION_H
#define BRANDMIGRATION_H

#include <QString>

/**
	@brief The BrandMigration namespace: the program keeps its settings,
	its configuration folder and its data folder (own elements, title
	blocks, macros) under the name FaseCAD. The first time it starts so,
	what it had under the former name is copied there -- copied, never
	moved, so a QElectroTech installed beside it keeps its own.
*/
namespace BrandMigration
{
		/// The organization and application name (settings, folders)
	inline QString name() {return QStringLiteral("FaseCAD");}
	inline QString domain() {return QStringLiteral("fasecad.com");}
		/// The name the settings and folders had before
	inline QString formerName() {return QStringLiteral("QElectroTech");}

		/// Copy the settings and folders of the former name, once. Called
		/// after the application and organization names are set and
		/// before anything reads a setting.
	void run();

		/// On the very first start on a computer -- no settings under
		/// either name -- the new design: command tabs, parts database
		/// and the theme of the system. Called by the interface only, so
		/// the headless runs (exports, scripts) keep the plain defaults.
	void applyFirstStartDefaults();
}

#endif // BRANDMIGRATION_H
