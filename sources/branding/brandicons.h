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
#ifndef BRANDICONS_H
#define BRANDICONS_H

#include <QIcon>
#include <QPixmap>
#include <QString>

namespace BrandIcons
{
	/**
		@return the logo for a tree of collections : 16 px, the size of
		the folders beside it, in the frameless version the brand gives
		for small sizes. Drawn from the SVG at every usual screen scale,
		so a scaled screen gets a sharp icon instead of an enlarged one,
		and only at that size, as a tree draws an icon at the largest size
		it is given.
	*/
	inline QIcon treeLogo()
	{
		const QIcon source(QStringLiteral(":/ico/branding/logo-small.svg"));
		QIcon icon;
		for (qreal scale : {1.0, 1.25, 1.5, 1.75, 2.0}) {
			icon.addPixmap(source.pixmap(QSize(16, 16), scale));
		}
		return icon;
	}
}

#endif // BRANDICONS_H
