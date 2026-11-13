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
#include "devicenaming.h"

#include "autoNum/elementautonumschemecommand.h"
#include "diagram.h"
#include "qetgraphicsitem/element.h"
#include "qetinformation.h"
#include "qetproject.h"

/**
	@brief DeviceNaming::baseName
	@return the default name of @p element: the label its symbol gives
	it without its own digits ("-K" for "-K1"), else "-" and the prefix of
	its folder of the collection, else an empty string
*/
QString DeviceNaming::baseName(const Element *element)
{
	if (!element) {
		return QString();
	}
	QString own = element->elementInformations()
			.value(QETInformation::ELMT_LABEL).toString().trimmed();
	while (!own.isEmpty() && own.back().isDigit()) {
		own.chop(1);
	}
		//"_" is how a symbol of the collection says it has no label
	if (!own.isEmpty() && own != QLatin1String("_")) {
		return own;
	}
	const QString prefix = element->getPrefix().trimmed();
	if (prefix.isEmpty()) {
		return QString();
	}
	if (prefix.startsWith(QLatin1Char('-'))) {
		return prefix;
	}
	return QLatin1Char('-') + prefix;
}

/**
	@brief DeviceNaming::nextLabel
	@return @p base followed by the first number from 1 that makes a label
	not in @p held
*/
QString DeviceNaming::nextLabel(const QString &base, const QSet<QString> &held)
{
	for (int number = 1 ; ; ++number) {
		const QString label = base + QString::number(number);
		if (!held.contains(label)) {
			return label;
		}
	}
}

/**
	@brief DeviceNaming::nameNewElement
	Give @p element, being placed, its default name and the first number
	free in its project. Called before the element's step of undo is
	pushed: undoing the placement takes the label away with the element.
*/
void DeviceNaming::nameNewElement(Element *element)
{
	nameNewElements({element});
}

/**
	@brief DeviceNaming::nameNewElements
	nameNewElement() for each of @p elements, placed together: each takes
	a number the ones before it did not.
*/
void DeviceNaming::nameNewElements(const QList<Element *> &elements)
{
	QSet<QString> held;
	bool read = false;
	for (Element *element : elements) {
		if (!element || !element->diagram() || !element->diagram()->project()) {
			continue;
		}
		if (!(element->linkType() & (Element::Simple | Element::Master))) {
			continue;
		}
		const QString base = baseName(element);
		if (base.isEmpty()) {
			continue;
		}
		if (!read) {
			held = ElementAutoNumSchemeCommand::labelsHeldBesides(
						element->diagram()->project(), element);
			read = true;
		}
		held.remove(element->elementInformations()
					.value(QETInformation::ELMT_LABEL).toString());

		const QString label = nextLabel(base, held);
		held.insert(label);
		DiagramContext information = element->elementInformations();
		information.addValue(QETInformation::ELMT_LABEL, label);
		information.remove(QETInformation::ELMT_FORMULA);
		information.remove(QETInformation::ELMT_FORMULA_ID);
		element->setElementInformations(information);
	}
}
