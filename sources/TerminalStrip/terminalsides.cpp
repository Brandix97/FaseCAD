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
#include "terminalsides.h"

#include "../cable/cable.h"
#include "../diagram.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/terminal.h"
#include "../qet.h"
#include "../qetproject.h"

#include <QCoreApplication>

namespace {

/**
	@return true when the first wire of @p terminal is a core of a cable
	of @p project
*/
bool wiredToACable(QETProject *project, const Terminal *terminal)
{
	if (!project) return false;
	const QList<Conductor *> conductors = terminal->conductors();
	if (conductors.isEmpty()) return false;
	const QUuid wire = conductors.first()->uuid();
	for (Cable *cable : project->cables())
	{
		for (const CableCore &core : cable->usedCores())
		{
			if (core.conductor == wire) return true;
		}
	}
	return false;
}

qreal heightOf(const Terminal *terminal)
{
	return terminal->sceneBoundingRect().center().y();
}

} // namespace

QString TerminalSides::swappedKey()
{
	return QStringLiteral("outside_swapped");
}

bool TerminalSides::isSwapped(const Element *element)
{
	return element && QET::infoFlagIsTrue(
				element->elementInformations().value(swappedKey()).toString());
}

int TerminalSides::outsideIndex(const Element *element, Reason *reason)
{
	if (reason) *reason = Reason::None;
	if (!element) return -1;
	const QList<Terminal *> terminals = element->terminals();
	if (terminals.isEmpty()) return -1;
	if (terminals.size() == 1) return 0;

	QETProject *project = element->diagram()
			? element->diagram()->project() : nullptr;

		//The side wired to a cable goes out to the field; without one,
		//the lowest connection point
	int outside = -1;
	for (int i = 0; i < terminals.size(); ++i)
	{
		if (wiredToACable(project, terminals.at(i))) {
			outside = i;
			if (reason) *reason = Reason::Cable;
			break;
		}
	}
	if (outside < 0)
	{
		outside = 0;
		for (int i = 1; i < terminals.size(); ++i)
		{
			if (heightOf(terminals.at(i)) > heightOf(terminals.at(outside))) {
				outside = i;
			}
		}
		if (reason) *reason = Reason::Position;
	}

	if (!isSwapped(element)) return outside;

		//Swapped: the highest of the other ones goes out instead
	int swapped = -1;
	for (int i = 0; i < terminals.size(); ++i)
	{
		if (i == outside) continue;
		if (swapped < 0 || heightOf(terminals.at(i)) < heightOf(terminals.at(swapped))) {
			swapped = i;
		}
	}
	return swapped;
}

Terminal *TerminalSides::outside(const Element *element)
{
	const int index = outsideIndex(element);
	return index >= 0 ? element->terminals().at(index) : nullptr;
}

Terminal *TerminalSides::inside(const Element *element)
{
	const int out = outsideIndex(element);
	if (out < 0) return nullptr;
	const QList<Terminal *> terminals = element->terminals();
	Terminal *highest = nullptr;
	for (int i = 0; i < terminals.size(); ++i)
	{
		if (i == out) continue;
		if (!highest || heightOf(terminals.at(i)) < heightOf(highest)) {
			highest = terminals.at(i);
		}
	}
	return highest;
}

QString TerminalSides::placeOf(const Element *element, const Terminal *terminal)
{
	if (!element || !terminal) return QString();
	const QPointF from = element->sceneBoundingRect().center();
	const QPointF to = terminal->dockConductor();
	const qreal dx = to.x() - from.x();
	const qreal dy = to.y() - from.y();
	if (qAbs(dy) >= qAbs(dx)) {
		return dy > 0
				? QCoreApplication::translate("TerminalSides", "bottom",
											  "where a connection point of a terminal stands")
				: QCoreApplication::translate("TerminalSides", "top",
											  "where a connection point of a terminal stands");
	}
	return dx > 0
			? QCoreApplication::translate("TerminalSides", "right",
										  "where a connection point of a terminal stands")
			: QCoreApplication::translate("TerminalSides", "left",
										  "where a connection point of a terminal stands");
}
