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
#include "pluginlibrary.h"

#include "../container/qetzip.h"
#include "../qetapp.h"
#include "../scripting/qetscripting.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>

namespace {
	QString folder_override;

	QString disabledKey() {return QStringLiteral("plugins/disabled");}

		/// The entries of a .fasplugin under the folder holding its
		/// plugin.json, at the root or in one folder
	bool pluginEntries(const QString &path, QList<QetZip::Entry> *entries, QString *error)
	{
		QList<QetZip::Entry> all;
		if (!QetZip::read(path, &all, error)) return false;
		QString root;
		bool found = false;
		for (const QetZip::Entry &e : std::as_const(all)) {
			if (e.name == QLatin1String("plugin.json")) {root.clear(); found = true; break;}
			if (e.name.count(QLatin1Char('/')) == 1 && e.name.endsWith(QLatin1String("/plugin.json"))) {
				root = e.name.section(QLatin1Char('/'), 0, 0) + QLatin1Char('/');
				found = true;
			}
		}
		if (!found) {
			if (error) *error = PluginManifest::tr("The file holds no plugin.json");
			return false;
		}
		entries->clear();
		for (QetZip::Entry e : std::as_const(all)) {
			if (!e.name.startsWith(root) || e.name.endsWith(QLatin1Char('/'))) continue;
			e.name = e.name.mid(root.size());
				//Nothing may land outside the folder of the plugin
			if (e.name.isEmpty() || e.name.contains(QLatin1String("..")) || e.name.startsWith(QLatin1Char('/'))
				|| e.name.contains(QLatin1Char(':')) || e.name.contains(QLatin1Char('\\'))) {
				if (error) *error = PluginManifest::tr("The file holds a path FaseCAD refuses: %1").arg(e.name);
				return false;
			}
			*entries << e;
		}
		return true;
	}
}

PluginLibrary &PluginLibrary::instance()
{
	static PluginLibrary library;
	return library;
}

PluginLibrary::PluginLibrary()
{
	rescan();
}

QString PluginLibrary::folder()
{
	return folder_override.isEmpty() ? QETApp::dataDir() + QStringLiteral("/plugins")
									 : folder_override;
}

void PluginLibrary::setFolder(const QString &folder)
{
	folder_override = folder;
	instance().rescan();
}

QString PluginLibrary::actionId(const QString &plugin_id, const QString &command_id)
{
	return QStringLiteral("diagrameditor.plugin.") + plugin_id + QLatin1Char('.') + command_id;
}

QIcon PluginLibrary::icon(const Plugin &plugin, const PluginManifest::Command &command)
{
	if (command.icon.isEmpty()) return QIcon(QStringLiteral(":/ico/plugin.svg"));
	const QString file = QDir(plugin.path).filePath(command.icon);
	return QFileInfo::exists(file) ? QIcon(file) : QIcon(QStringLiteral(":/ico/plugin.svg"));
}

const PluginLibrary::Plugin *PluginLibrary::find(const QString &id) const
{
	for (const Plugin &p : m_plugins)
		if (p.manifest.id == id) return &p;
	return nullptr;
}

void PluginLibrary::rescan()
{
	m_plugins.clear();
	m_errors.clear();
	const QStringList disabled = QSettings().value(disabledKey()).toStringList();
	const QDir dir(folder());
	for (const QString &name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
		QFile file(dir.filePath(name + QStringLiteral("/plugin.json")));
		if (!file.open(QIODevice::ReadOnly)) {
			m_errors << tr("%1: no plugin.json").arg(name);
			continue;
		}
		QString error;
		const PluginManifest manifest = PluginManifest::parse(file.readAll(), &error);
		if (!error.isEmpty()) {
			m_errors << QStringLiteral("%1: %2").arg(name, error);
			continue;
		}
		if (manifest.id != name) {
			m_errors << tr("%1: the folder is not named after the id of the plugin (%2)")
						.arg(name, manifest.id);
			continue;
		}
		Plugin plugin;
		plugin.manifest = manifest;
		plugin.path = dir.filePath(name);
		plugin.enabled = !disabled.contains(manifest.id);
		m_plugins << plugin;
	}
	emit changed();
}

PluginManifest PluginLibrary::readArchive(const QString &path, QString *error)
{
	QList<QetZip::Entry> entries;
	if (!pluginEntries(path, &entries, error)) return PluginManifest();
	for (const QetZip::Entry &e : std::as_const(entries))
		if (e.name == QLatin1String("plugin.json"))
			return PluginManifest::parse(e.data, error);
	if (error) *error = PluginManifest::tr("The file holds no plugin.json");
	return PluginManifest();
}

bool PluginLibrary::install(const QString &path, bool replace, QString *id, QString *error)
{
	QString why;
	const PluginManifest manifest = readArchive(path, &why);
	if (!why.isEmpty()) {
		if (error) *error = why;
		return false;
	}
	if (id) *id = manifest.id;
	QDir target(QDir(folder()).filePath(manifest.id));
	if (target.exists()) {
		if (!replace) {
			if (error) *error = tr("The plugin %1 is already installed").arg(manifest.id);
			return false;
		}
		if (!target.removeRecursively()) {
			if (error) *error = tr("The plugin installed before cannot be removed");
			return false;
		}
	}
	QList<QetZip::Entry> entries;
	pluginEntries(path, &entries, nullptr);
	for (const QetZip::Entry &e : std::as_const(entries)) {
		const QString file_path = target.filePath(e.name);
		QDir().mkpath(QFileInfo(file_path).absolutePath());
		QSaveFile file(file_path);
		if (!file.open(QIODevice::WriteOnly) || file.write(e.data) != e.data.size() || !file.commit()) {
			if (error) *error = tr("%1 cannot be written").arg(file_path);
			target.removeRecursively();
			rescan();
			return false;
		}
	}
	rescan();
	return true;
}

bool PluginLibrary::uninstall(const QString &id, QString *error)
{
	if (!PluginManifest::isValidId(id)) return false;
	QDir target(QDir(folder()).filePath(id));
	if (!target.exists() || !target.removeRecursively()) {
		if (error) *error = tr("The plugin %1 cannot be removed").arg(id);
		rescan();
		return false;
	}
	QSettings settings;
	QStringList disabled = settings.value(disabledKey()).toStringList();
	disabled.removeAll(id);
	settings.setValue(disabledKey(), disabled);
	rescan();
	return true;
}

void PluginLibrary::setEnabled(const QString &id, bool enabled)
{
	QSettings settings;
	QStringList disabled = settings.value(disabledKey()).toStringList();
	disabled.removeAll(id);
	if (!enabled) disabled << id;
	settings.setValue(disabledKey(), disabled);
	rescan();
}

bool PluginLibrary::run(const Plugin &plugin, const PluginManifest::Command &command,
						QETProject *project, DiagramView *view, QString *error)
{
	QFile file(QDir(plugin.path).filePath(command.script));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		if (error) *error = tr("%1 cannot be read").arg(file.fileName());
		return false;
	}
		//The script, then the call of the command's function
	const QString source = QString::fromUtf8(file.readAll())
			+ QStringLiteral("\n;if (typeof %1 !== 'function') throw new Error('%2 has no function %1');\n%1();\n")
			  .arg(command.function, command.script);
	return QetScripting::runPlugin(source, file.fileName(), command.label.simplified(),
								   plugin.path, project, view);
}
