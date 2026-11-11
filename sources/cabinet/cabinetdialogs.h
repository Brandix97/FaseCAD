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
#ifndef CABINETDIALOGS_H
#define CABINETDIALOGS_H

#include "cabinetitem.h"

#include <QColor>
#include <QCoreApplication>
#include <QIcon>
#include <QList>

class QWidget;

/**
	The windows asking for the measures of the parts of a cabinet and for
	the scale of a cabinet folio. Each returns false if the user gave up.
	What was last chosen is offered again the next time.
*/
class CabinetDialogs
{
	Q_DECLARE_TR_FUNCTIONS(CabinetDialogs)

	public:
		static QList<qreal> ductWidths();
		static QList<qreal> scales();
		static QList<QSizeF> plateSizes();

		static bool askDuct(qreal &width, qreal &depth, QWidget *parent);
		static bool askPlate(QSizeF &size, QWidget *parent);
		static bool askScale(qreal &scale, QWidget *parent);
		static bool editItem(CabinetItem::Properties &properties, QWidget *parent);

		static QString scaleText(qreal scale);
		static QIcon folioIcon(const QColor &color = QColor());
		static QIcon icon(CabinetItem::Kind kind);
		static QIcon dxfIcon();
};

#endif // CABINETDIALOGS_H
