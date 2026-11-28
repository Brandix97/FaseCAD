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
#ifndef PLUGINLIBRARY_H
#define PLUGINLIBRARY_H

#include "pluginmanifest.h"

#include <QIcon>
#include <QObject>

class QETProject;
class DiagramView;

/**
	@brief The PluginLibrary class keeps the plugins installed: one folder
	each, named after its id, in folder(), holding its plugin.json, its
	scripts and its icons. A plugin comes as a .fasplugin file, a zip of
	that folder. A plugin switched off stays installed and adds nothing.

	One instance for the application, shared by every editor window.
*/
class PluginLibrary : public QObject
{
	Q_OBJECT

	public:
		struct Plugin {
			PluginManifest manifest;
			QString path;
			bool enabled = true;
		};

		static PluginLibrary &instance();
		static QString folder();
			/// For the tests: the plugins of another folder
		static void setFolder(const QString &folder);
			/// The ShortcutManager id of a command of a plugin
		static QString actionId(const QString &plugin_id, const QString &command_id);
		static QIcon icon(const Plugin &plugin, const PluginManifest::Command &command);

		QList<Plugin> plugins() const {return m_plugins;}
			/// The folders which hold no usable plugin, and why
		QStringList errors() const {return m_errors;}
		const Plugin *find(const QString &id) const;

			/// The manifest in the .fasplugin @p path, to show before
			/// installing it
		static PluginManifest readArchive(const QString &path, QString *error);
			/// Install the .fasplugin @p path; a plugin with its id is
			/// replaced only with @p replace
		bool install(const QString &path, bool replace, QString *id, QString *error);
		bool uninstall(const QString &id, QString *error);
		void setEnabled(const QString &id, bool enabled);

			/// Run @p command of @p plugin on @p project
		static bool run(const Plugin &plugin, const PluginManifest::Command &command,
						QETProject *project, DiagramView *view, QString *error = nullptr);

	public slots:
		void rescan();

	signals:
		void changed();

	private:
		PluginLibrary();
		QList<Plugin> m_plugins;
		QStringList m_errors;
};

#endif // PLUGINLIBRARY_H
