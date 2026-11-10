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
#ifndef PARTSBOM_H
#define PARTSBOM_H

#include "../cablelist/cablelistrows.h"
#include "../diagramcontext.h"

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

class PartsDatabase;
class QETProject;

/**
	@brief The PartsBom namespace works out the bill of materials of a
	project from the parts its devices are made of.

	Each device counts:
	 - the parts of its Parts tab (PartsLinks), with their units, when it
	   has any;
	 - otherwise the articles of its information: the main article and
	   the auxiliary articles 1 to 4, with their quantities.
	A device marked "exclude from the bill of materials" counts nothing,
	nor do folio reports, cabinet thumbnails and wire definitions.

	The lines are the parts, grouped by manufacturer and order number,
	their units added up, with the labels of the devices using them. What
	the parts database knows of a part (description, supplier...) fills
	the line; the device's own article fills what the database leaves
	empty.

	The project is read through its database, without building a folio.
*/
namespace PartsBom
{
		/// One device to count: its label and its information
	struct Device
	{
		QString label;
		DiagramContext information;
	};

		/// The identifier of the table model showing this list
	inline QString identifier() {return QStringLiteral("parts_bom");}

	const QList<CableList::Column> &columns();
	const QStringList &defaultKeys();
	QString labelOf(const QString &key);

		/// The devices of @p project to count, in the order of the folios
	QList<Device> devices(QETProject *project);
		/// The lines for @p devices; @p database, if open, describes the
		/// parts
	QVector<QMap<QString, QString>> rows(const QList<Device> &devices,
										 const PartsDatabase *database);
		/// The lines of @p project, described from the parts database when
		/// it is used
	QVector<QMap<QString, QString>> rows(QETProject *project);
	QByteArray toCsv(QETProject *project, const QStringList &keys,
					 bool include_headers = true);
}

#endif // PARTSBOM_H
