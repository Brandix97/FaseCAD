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
#ifndef DEVICENAMING_H
#define DEVICENAMING_H

#include <QList>
#include <QSet>
#include <QString>

class Element;

/**
	@brief The DeviceNaming namespace names a device as it is placed, in
	place of QElectroTech's numbering formulas: the default name of its
	symbol and the first number no other device of the project carries,
	-K1, -K2... (Brandon, 2026-11-13).

	The default name is the label the symbol gives itself (symbol editor,
	Information of the element), its own digits left out; without one, the
	prefix of its folder of the collection (qet_labels.xml, Settings,
	"Configure prefixes") behind a dash. A symbol with neither keeps the
	label it has. Only devices are named: a contact takes the label of
	its master, a terminal its strip's number, a folio reference none.
*/
namespace DeviceNaming
{
	QString baseName(const Element *element);
	QString nextLabel(const QString &base, const QSet<QString> &held);
	void nameNewElement(Element *element);
	void nameNewElements(const QList<Element *> &elements);
}

#endif // DEVICENAMING_H
