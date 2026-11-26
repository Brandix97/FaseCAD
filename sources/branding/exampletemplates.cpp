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
#include "exampletemplates.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

namespace {

	///The examples copied so far, by file name
const char *installed_key = "templates/installed_examples";

} // namespace

QString ExampleTemplates::resourceDir()
{
	return QStringLiteral(":/templates/examples");
}

QString ExampleTemplates::folderName()
{
	return QStringLiteral("Default");
}

/**
	@brief ExampleTemplates::install
	See the header. A file standing in the folder already, under the same
	name, is the user's: it is not replaced.
*/
QStringList ExampleTemplates::install(const QString &source_dir,
									  const QString &templates_dir)
{
	QSettings settings;
	QStringList installed = settings.value(
				QLatin1String(installed_key)).toStringList();
	QStringList copied;

	const QDir target(QDir(templates_dir).filePath(folderName()));
	const QStringList files = QDir(source_dir).entryList(
				{QStringLiteral("*.qetmak")}, QDir::Files, QDir::Name);
	for (const QString &name : files) {
		if (installed.contains(name)) {
			continue;
		}
		installed << name;
		const QString destination = target.filePath(name);
		if (QFileInfo::exists(destination)
				|| !QDir().mkpath(target.path())
				|| !QFile::copy(QDir(source_dir).filePath(name),
								destination)) {
			continue;
		}
			//A file copied from the resources is read only: the user
			//may well want to change this copy
		QFile::setPermissions(destination,
							  QFile::permissions(destination)
							  | QFileDevice::WriteOwner
							  | QFileDevice::WriteUser);
		copied << name;
	}
	settings.setValue(QLatin1String(installed_key), installed);
	return copied;
}
