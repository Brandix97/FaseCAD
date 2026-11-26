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
#ifndef FASECADLICENCE_H
#define FASECADLICENCE_H

#include <QSettings>
#include <QString>

/**
	The licence key of the user (Brandon, 2026-11-25), sent to the services
	of fasecad.com (the parts catalog, the AI) as X-FaseCAD-Clave once
	they ask for one. Empty while none is needed.
*/
namespace FaseCadLicence
{
	inline QString key()
	{
		QSettings settings;
		return settings.value(QStringLiteral("fasecad/licence-key")).toString();
	}

	inline void setKey(const QString &key)
	{
		QSettings settings;
		if (key.trimmed().isEmpty())
			settings.remove(QStringLiteral("fasecad/licence-key"));
		else
			settings.setValue(QStringLiteral("fasecad/licence-key"), key.trimmed());
	}
}

#endif // FASECADLICENCE_H
