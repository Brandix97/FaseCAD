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
#include "cabinetparts.h"

#include "../diagram.h"
#include "../qetgraphicsitem/element.h"
#include "../qetproject.h"

#include <QSet>

CabinetParts::CabinetParts(Diagram *folio, const QSet<QUuid> &symbols)
{
	QETProject *project = folio ? folio->project() : nullptr;
	if (!project || !PartsDatabase::isEnabled()) return;
	m_database = std::make_unique<PartsDatabase>();
	if (!m_database->open(PartsDatabase::path())) {
		m_database.reset();
		return;
	}
	if (!symbols.isEmpty()) {
		project->buildFoliosHoldingSymbols(symbols);
	}
	for (Diagram *diagram : project->diagrams()) {
		if (!diagram->isLoaded()) continue;
		for (Element *element : diagram->elements()) {
			const QString parts = element->elementInformations()
					.value(PartsLinks::infoKey()).toString();
			if (parts.trimmed().isEmpty()) continue;
			m_by_symbol.insert(element->uuid(), parts);
			const QString label = element->actualLabel().trimmed();
			if (!label.isEmpty() && !m_by_label.contains(label))
				m_by_label.insert(label, parts);
		}
	}
}

QList<PartsLinks::Link> CabinetParts::linksOf(const QUuid &symbol, const QString &label) const
{
	QString parts = symbol.isNull() ? QString() : m_by_symbol.value(symbol);
	if (parts.isEmpty())
		parts = m_by_label.value(label.trimmed());
	return PartsLinks::decode(parts);
}

std::optional<Part> CabinetParts::part(const PartsLinks::Link &link) const
{
	return part(link.manufacturer, link.order_number);
}

std::optional<Part> CabinetParts::part(const QString &manufacturer, const QString &order_number) const
{
	if (!m_database || order_number.isEmpty()) return std::nullopt;
	const qint64 id = m_database->findPart(manufacturer, order_number);
	return id > 0 ? m_database->part(id) : std::nullopt;
}
