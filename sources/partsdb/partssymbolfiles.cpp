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
#include "partssymbolfiles.h"

#include "../ElementsCollection/elementslocation.h"
#include "../qetapp.h"

#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>

namespace {

	/// A folder of the collection named @p names (language, name) in its
	/// qet_directory, made when missing
bool ensureFolder(const QString &path, const QList<QPair<QString, QString>> &names)
{
	QDir dir(path);
	if (!dir.exists() && !QDir().mkpath(path)) {
		return false;
	}
	const QString description = dir.filePath(QStringLiteral("qet_directory"));
	if (QFile::exists(description)) {
		return true;
	}
	QString xml = QStringLiteral("<qet-directory>\n    <names>\n");
	for (const auto &[language, name] : names) {
		xml += QStringLiteral("        <name lang=\"%1\">%2</name>\n")
				.arg(language, name.toHtmlEscaped());
	}
	xml += QStringLiteral("    </names>\n</qet-directory>\n");
	QSaveFile file(description);
	return file.open(QIODevice::WriteOnly)
			&& file.write(xml.toUtf8()) >= 0 && file.commit();
}

QString safeName(const QString &name)
{
	QString safe = name.trimmed();
	safe.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._ -]+")),
				 QStringLiteral("_"));
	return safe.isEmpty() ? QStringLiteral("symbol") : safe;
}

}

/**
	@brief PartsSymbolFiles::importFolderName
	@return the folder of the user's collection the symbols of imported
	parts go to
*/
QString PartsSymbolFiles::importFolderName()
{
	return QStringLiteral("imported_parts");
}

/**
	@brief PartsSymbolFiles::read
	@return the file name and the definition of the symbol at @p location,
	a SymbolFile without a name when there is none
*/
PartsArchive::SymbolFile PartsSymbolFiles::read(const QString &location)
{
	PartsArchive::SymbolFile file;
	const ElementsLocation symbol(location);
	if (!symbol.isElement() || !symbol.exist()) {
		return file;
	}
	const QDomElement definition = symbol.xml();
	if (definition.isNull()) {
		return file;
	}
	QDomDocument document;
	document.appendChild(document.importNode(definition, true));
	file.name = symbol.fileName();
	file.definition = document.toByteArray(4);
	return file;
}

/**
	@brief PartsSymbolFiles::write
	Put @p symbol in the user's collection, in the folder of the
	manufacturer of @p part under importFolderName().
	@return its location ("custom://imported_parts/ACME/x.elmt"), empty
	when it cannot be written
*/
QString PartsSymbolFiles::write(const PartsArchive::SymbolFile &symbol, const Part &part)
{
	const QString root = QETApp::customElementsDirN() + QLatin1Char('/')
			+ importFolderName();
	if (!ensureFolder(root, {{QStringLiteral("en"), QStringLiteral("Imported parts")},
							 {QStringLiteral("es"), QStringLiteral("Piezas importadas")},
							 {QStringLiteral("fr"), QStringLiteral("Pièces importées")}})) {
		return QString();
	}
	const QString manufacturer = part.manufacturer.trimmed().isEmpty()
			? QStringLiteral("other") : part.manufacturer.trimmed();
	const QString folder = root + QLatin1Char('/') + safeName(manufacturer);
	if (!ensureFolder(folder, {{QStringLiteral("en"), manufacturer},
							   {QStringLiteral("es"), manufacturer}})) {
		return QString();
	}

	QString base = safeName(symbol.name);
	if (base.endsWith(QLatin1String(".elmt"), Qt::CaseInsensitive)) {
		base.chop(5);
	}
	for (int attempt = 1 ; attempt < 1000 ; ++attempt) {
		const QString name = attempt == 1
				? base + QStringLiteral(".elmt")
				: QStringLiteral("%1-%2.elmt").arg(base).arg(attempt);
		const QString path = folder + QLatin1Char('/') + name;
		QFile existing(path);
		if (existing.exists()) {
				//The same symbol imported before is the one used
			if (existing.open(QIODevice::ReadOnly)
				&& existing.readAll() == symbol.definition) {
				return ElementsLocation(path).toString();
			}
			continue;
		}
		QSaveFile file(path);
		if (!file.open(QIODevice::WriteOnly)
			|| file.write(symbol.definition) < 0 || !file.commit()) {
			return QString();
		}
		return ElementsLocation(path).toString();
	}
	return QString();
}
