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
#ifndef CABLECHECKS_H
#define CABLECHECKS_H

#include <QCoreApplication>
#include <QString>
#include <QStringList>

/**
	@brief What the cable manager checks on a cable, and how it says it.

	Plain values in, plain sentences out : no project, no widget, so the
	rules can be tested on their own and stay the same in every window
	that shows them.
*/
class CableChecks
{
	Q_DECLARE_TR_FUNCTIONS(CableChecks)

	public:
		/// What is known of one cable for the checks.
		struct Facts {
			QString designation;
				/// Cores the type of the cable has; 0 when it has no type
			int core_count = 0;
				/// Cores wired to a conductor
			int used_cores = 0;
				/// Another cable of the project has the same designation
			bool duplicate_designation = false;
		};

		static QStringList problems(const Facts &facts, int minimum_spare);
};

#endif // CABLECHECKS_H
