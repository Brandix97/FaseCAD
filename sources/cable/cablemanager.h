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
#ifndef CABLEMANAGER_H
#define CABLEMANAGER_H

#include <QLineF>
#include <QList>
#include <QPointF>
#include <QUuid>

class Cable;
class Conductor;
class Diagram;
class QETProject;

/**
	@brief One place where a drawn cable line crosses a conductor.
*/
struct CableCrossing
{
	Conductor *conductor = nullptr;
		/// Scene coordinates of the crossing, where the slash is drawn
	QPointF position;
};

/**
	@brief The CableManager works out everything which follows from the
	cables of a project, so nothing has to be maintained by hand.

	It detects which conductors a line about to be drawn crosses, and it
	turns the cables back into the cable field of every conductor they
	own -- both directions of one single rule: the cable is the only
	source of truth.
*/
class CableManager
{
	public:
			///How far a colour label may stand from where a line crosses
			///a wire for that wire to count as the one the label stands
			///on: one scene unit, and that is already generous. The grid
			///is there to put a label exactly on the crossing, and it
			///does: a label drawn on a wire, slid along its own line or
			///carried there by a paste lands on that crossing to the
			///last decimal. The single unit only soaks up the rounding
			///which comes of projecting a label onto its line, and a
			///terminal which is a hair off the grid. Anything beyond it
			///is beside the wire rather than on it, so a label one
			///whole grid step away -- however the grid is set -- gives
			///up its entry. One number for every place a label is given
			///to a wire: drawing one, dragging one and wiring a copy
			///must not disagree about it.
		static constexpr qreal bind_tolerance = 1.0;

		//Drawing
		static QList<CableCrossing> crossedConductors(Diagram *diagram, const QLineF &line);
		static bool crossingOf(Diagram *diagram,
							   const QLineF &line,
							   Conductor *conductor,
							   QPointF *result);
		static qreal distanceTo(Conductor *conductor, const QPointF &scene_pos);

		//Keeping the drawing in step with the cables
		static void refreshLabels(QETProject *project);
		static void claimConductor(QETProject *project, Conductor *conductor, Cable *new_owner);

		//Lookup
		static Cable *cableByUuid(QETProject *project, const QUuid &uuid);
		static Conductor *conductorByUuid(QETProject *project, const QUuid &uuid);
		static QString baseName();
		static QString nextDesignation(QETProject *project, Diagram *diagram);
};

#endif // CABLEMANAGER_H
