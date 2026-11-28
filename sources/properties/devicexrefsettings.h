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
#ifndef DEVICEXREFSETTINGS_H
#define DEVICEXREFSETTINGS_H

#include <QDomElement>
#include <QString>

/**
	@brief How the parts of one device (the symbols sharing a label, a PLC
	drawn as a supply, its inputs and its outputs) show where the others
	are (Brandon, 2026-11-28): the main part lists the places of the
	others, each other part the place of the main one. Off in a project
	saved without it, on in a new project.
*/
struct DeviceXRefSettings
{
	enum class Secondary { Main, All, None };
	enum class Place { UnderLabel, BelowSymbol };

	bool enabled = false;
	Secondary secondary = Secondary::Main;
	Place place = Place::UnderLabel;

	bool operator==(const DeviceXRefSettings &other) const {
		return enabled == other.enabled && secondary == other.secondary
				&& place == other.place;
	}
	bool operator!=(const DeviceXRefSettings &other) const {return !(*this == other);}

	static QString xmlTagName() {return QStringLiteral("device_xrefs");}

		/// Nothing is written for the settings a project without them reads
	bool isDefault() const {return *this == DeviceXRefSettings();}

	void toXml(QDomElement &parent) const
	{
		if (isDefault()) return;
		QDomElement e = parent.ownerDocument().createElement(xmlTagName());
		e.setAttribute(QStringLiteral("enabled"), enabled ? QStringLiteral("true")
														  : QStringLiteral("false"));
		e.setAttribute(QStringLiteral("secondary"),
					   secondary == Secondary::All ? QStringLiteral("all")
					   : secondary == Secondary::None ? QStringLiteral("none")
					   : QStringLiteral("main"));
		e.setAttribute(QStringLiteral("place"), place == Place::BelowSymbol
					   ? QStringLiteral("symbol") : QStringLiteral("label"));
		parent.appendChild(e);
	}

	static DeviceXRefSettings fromXml(const QDomElement &parent)
	{
		DeviceXRefSettings settings;
		const QDomElement e = parent.firstChildElement(xmlTagName());
		if (e.isNull()) return settings;
		settings.enabled = e.attribute(QStringLiteral("enabled")) == QLatin1String("true");
		const QString secondary = e.attribute(QStringLiteral("secondary"));
		settings.secondary = secondary == QLatin1String("all") ? Secondary::All
				: secondary == QLatin1String("none") ? Secondary::None : Secondary::Main;
		settings.place = e.attribute(QStringLiteral("place")) == QLatin1String("symbol")
				? Place::BelowSymbol : Place::UnderLabel;
		return settings;
	}
};

#endif // DEVICEXREFSETTINGS_H
