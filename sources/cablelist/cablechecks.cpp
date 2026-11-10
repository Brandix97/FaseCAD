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
#include "cablechecks.h"

/**
	@brief CableChecks::problems
	@param facts : what is known of the cable
	@param minimum_spare : how many free cores a cable should keep in
	reserve; 0 checks nothing about spare cores
	@return one sentence per problem of the cable, empty when it has
	none. A cable without a type has no cores to count, so it is only
	told that.
*/
QStringList CableChecks::problems(const Facts &facts, int minimum_spare)
{
	QStringList result;
	if (facts.designation.trimmed().isEmpty()) {
		result << tr("No name");
	} else if (facts.duplicate_designation) {
		result << tr("Name used by another cable");
	}

	if (facts.core_count <= 0) {
		result << tr("No cable type");
		return result;
	}

	if (facts.used_cores == 0) {
		result << tr("No core wired");
	}

	const int spare = facts.core_count - facts.used_cores;
	if (minimum_spare > 0 && spare < minimum_spare) {
		result << (spare <= 0 ? tr("No spare core")
							  : tr("Only %n spare core(s)", nullptr, spare));
	}
	return result;
}
