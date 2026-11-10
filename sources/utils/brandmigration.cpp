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
#include "brandmigration.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {

const char *migrated_key = "brand/copied_from_former_name";

const char *new_design_key = "brand/new_design_applied";

	/// The writable folder @p type would be under the names @p organization
	/// and @p application
QString locationUnder(QStandardPaths::StandardLocation type,
					  const QString &organization, const QString &application)
{
	const QString org = QCoreApplication::organizationName();
	const QString app = QCoreApplication::applicationName();
	QCoreApplication::setOrganizationName(organization);
	QCoreApplication::setApplicationName(application);
	const QString path = QStandardPaths::writableLocation(type);
	QCoreApplication::setOrganizationName(org);
	QCoreApplication::setApplicationName(app);
	return path;
}

	/// Copy the folder @p from to @p to, when @p to does not exist yet. The
	/// element cache is left out: it is worked out again from the folders.
void copyFolder(const QString &from, const QString &to)
{
	if (from.isEmpty() || to.isEmpty() || from == to
			|| !QFileInfo(from).isDir() || QFileInfo::exists(to)) {
		return;
	}
	QDir().mkpath(to);
	QDirIterator it(from, QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot,
					QDirIterator::Subdirectories);
	const QDir source(from);
	while (it.hasNext()) {
		const QString path = it.next();
		const QFileInfo info(path);
		const QString relative = source.relativeFilePath(path);
		const QString target = to + QLatin1Char('/') + relative;
		if (info.isDir()) {
			QDir().mkpath(target);
		} else if (!info.fileName().startsWith(QLatin1String("elements_cache"))) {
			QDir().mkpath(QFileInfo(target).absolutePath());
			QFile::copy(path, target);
		}
	}
}

} // namespace

void BrandMigration::run()
{
	QSettings settings;
	if (settings.value(QLatin1String(migrated_key)).toBool()) {
		return;
	}

		//The settings: copied when there are none under the new name yet
		//In the same format as the new ones: the INI file of
		//QET_SETTINGS_DIR when it is set (QSettings(organization,
		//application) alone would read the registry on Windows)
	QSettings former(QSettings::defaultFormat(), QSettings::UserScope,
					 formerName(), formerName());
	if (settings.allKeys().isEmpty()) {
		const QStringList keys = former.allKeys();
		for (const QString &key : keys) {
			settings.setValue(key, former.value(key));
		}
	}

		//The folders
	const QString former_name = formerName();
	for (const auto type : {QStandardPaths::AppConfigLocation,
							QStandardPaths::AppDataLocation,
							QStandardPaths::AppLocalDataLocation}) {
		copyFolder(locationUnder(type, former_name, former_name),
				   QStandardPaths::writableLocation(type));
	}

		//The crash backups of the projects open when it stopped
	const QString generic = QStandardPaths::writableLocation(
				QStandardPaths::GenericDataLocation) + QLatin1String("/stalefiles/");
	copyFolder(generic + former_name, generic + name());

	settings.setValue(QLatin1String(migrated_key), true);
	settings.sync();
}

void BrandMigration::applyFirstStartDefaults()
{
	QSettings settings;
	if (settings.value(QLatin1String(new_design_key)).toBool()) {
		return;
	}
		//Once for every set of settings, new or copied from QElectroTech
		//(which had none of these): what he changes afterwards stays
	settings.setValue(QStringLiteral("diagrameditor/command_tabs"), true);
	settings.setValue(QStringLiteral("parts-database/enabled"), true);
	if (!settings.contains(QStringLiteral("theme"))) {
			//QetSettings::Theme::System
		settings.setValue(QStringLiteral("theme"), 1);
	}
	settings.setValue(QLatin1String(new_design_key), true);
	settings.sync();
}
