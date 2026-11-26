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
#include "partsdbsymbols.h"

#include "partsdatabase.h"

#include <QDomDocument>
#include <QUrl>

namespace PartsDbSymbols
{
namespace
{
	const QString prefix = QStringLiteral("partsdb:");

	QString encode(const QString &text)
	{
		return QString::fromLatin1(QUrl::toPercentEncoding(text));
	}
}

bool isStored(const QString &location)
{
	return location.startsWith(prefix);
}

QString location(qint64 part_id, const QString &file_name)
{
	return prefix + QString::number(part_id) + QLatin1Char('/') + encode(file_name);
}

bool decode(const QString &location, qint64 *part_id, QString *file_name)
{
	if (!isStored(location)) return false;
	const QStringList parts = location.mid(prefix.size()).split(QLatin1Char('/'));
	bool ok = false;
	const qint64 id = parts.size() == 2 ? parts.at(0).toLongLong(&ok) : 0;
	if (!ok || id <= 0 || parts.at(1).isEmpty()) return false;
	if (part_id) *part_id = id;
	if (file_name) *file_name = QUrl::fromPercentEncoding(parts.at(1).toLatin1());
	return true;
}

QString fileName(const QString &location)
{
	QString name;
	return decode(location, nullptr, &name) ? name : QString();
}

bool isSymbol(const QByteArray &definition)
{
	QDomDocument document;
	if (!document.setContent(definition)) return false;
	const QDomElement root = document.documentElement();
	return root.tagName() == QLatin1String("definition")
			&& root.attribute(QStringLiteral("type")) == QLatin1String("element");
}

PartsArchive::SymbolFile read(const PartsDatabase &database, const QString &location)
{
	PartsArchive::SymbolFile symbol;
	qint64 part = 0;
	QString name;
	if (!decode(location, &part, &name)) return symbol;
	for (const PartFile &file : database.files(part))
	{
		if (file.kind != PartFileKind::Symbol || file.file_name != name) continue;
		symbol.name = name;
		symbol.definition = database.fileContent(file.id);
		return symbol;
	}
	return symbol;
}
	//resolve() is in partssymbolfiles.cpp, with the collection: what is
	//here is read by the tests of the database alone
}
