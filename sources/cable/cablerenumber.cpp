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
#include "cablerenumber.h"

#include "cable.h"
#include "cablemanager.h"
#include "../devicenaming.h"
#include "../diagram.h"
#include "../qetproject.h"

#include <QCoreApplication>
#include <QSet>

#include <algorithm>
#include <limits>

namespace {

/**
	@brief One cable of the project, and where it stands so that the
	cables can be laid out in an order everybody can see again.
*/
struct OrderedCable
{
	Cable *cable = nullptr;
		///The lowest folio the cable is drawn on
	int folio = 0;
		///How far it reaches to the left and how far to the top on
		///that folio
	qreal left = 0.0;
	qreal top = 0.0;
		///The folio the cable is numbered on
	Diagram *diagram = nullptr;
};

} // namespace

/**
	@brief CableRenumber::byHandCount
	@param project
	@return how many cables of that project carry a name typed in by hand
*/
int CableRenumber::byHandCount(QETProject *project)
{
	if (!project) return 0;

	int count = 0;
	for (Cable *cable : project->cables())
	{
		if (cable && cable->designationByHand()) {
			++count;
		}
	}
	return count;
}

/**
	@brief CableRenumber::plan
	Work the name of every cable of the project out, without writing
	anywhere. The cables are put in the order they are numbered in --
	folio by folio, and on each folio along whichever axis leads -- and
	numbered from 1 behind CableManager::baseName(), so that the
	numbering runs on without a hole.
	@param project the project whose cables are numbered
	@param x_axis_first true when the leftmost cable of a folio comes first
	@param include_by_hand false to leave the names typed in by hand alone
	@return the names, or what stopped the numbering
*/
CableRenumberPlan CableRenumber::plan(QETProject *project,
									  bool x_axis_first,
									  bool include_by_hand)
{
	CableRenumberPlan result;
	if (!project)
	{
		result.error = QCoreApplication::translate(
			"CableRenumber", "No project is open.");
		return result;
	}
	if (project->isReadOnly())
	{
		result.error = QCoreApplication::translate(
			"CableRenumber",
			"This project is read-only: its cables cannot be "
			"renumbered.");
		return result;
	}

		//Where every cable stands, and which ones are left alone: their
		//names stay as they are, so the numbering must not hand the
		//same number out again
	QList<OrderedCable> ordered;
	QSet<QString> taken;
	for (Cable *cable : project->cables())
	{
		if (!cable) continue;
		if (!include_by_hand && cable->designationByHand())
		{
			if (!cable->designation().isEmpty()) {
				taken.insert(cable->designation());
			}
			continue;
		}

		OrderedCable entry;
		entry.cable = cable;

		for (const CablePartData &part : cable->parts())
		{
			Diagram *diagram = project->diagramByUuid(part.diagram);
			if (!diagram) continue;

			const int index = project->folioIndex(diagram);
			const qreal left = qMin(part.p1.x(), part.p2.x());
			const qreal top = qMin(part.p1.y(), part.p2.y());

			if (!entry.diagram || index < entry.folio)
			{
				entry.folio = index;
				entry.diagram = diagram;
				entry.left = left;
				entry.top = top;
			} else if (index == entry.folio) {
				entry.left = qMin(entry.left, left);
				entry.top = qMin(entry.top, top);
			}
		}
			//A cable which stands on no folio the project still knows
			//is numbered last rather than first by accident
		if (!entry.diagram) {
			entry.folio = std::numeric_limits<int>::max();
		}
		ordered.append(entry);
	}

		//Folio by folio, and on each folio along the axis which leads:
		//the other axis is only asked when the leading one says two
		//cables are level, so that the numbering never depends on the
		//order the cables happen to be stored in
	std::sort(ordered.begin(), ordered.end(),
			  [x_axis_first](const OrderedCable &a, const OrderedCable &b)
	{
		if (a.folio != b.folio) return a.folio < b.folio;

		const qreal a_first = x_axis_first ? a.left : a.top;
		const qreal b_first = x_axis_first ? b.left : b.top;
		if (a_first != b_first) return a_first < b_first;

		const qreal a_second = x_axis_first ? a.top : a.left;
		const qreal b_second = x_axis_first ? b.top : b.left;
		if (a_second != b_second) return a_second < b_second;

		return a.cable->uuid().toString() < b.cable->uuid().toString();
	});

	for (const OrderedCable &entry : std::as_const(ordered))
	{
		if (!entry.diagram)
		{
			result.error = QCoreApplication::translate(
				"CableRenumber",
				"A cable is not on any sheet of the project: "
				"nothing was changed.");
			return result;
		}

			//The first number neither handed out yet nor kept by a cable
			//left alone
		const QString name =
				DeviceNaming::nextLabel(CableManager::baseName(), taken);
		taken.insert(name);
		result.cables.append(entry.cable);
		result.designations.append(name);
	}
	return result;
}
