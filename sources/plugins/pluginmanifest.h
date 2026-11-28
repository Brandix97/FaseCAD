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
#ifndef PLUGINMANIFEST_H
#define PLUGINMANIFEST_H

#include <QCoreApplication>
#include <QList>
#include <QString>
#include <QStringList>

/**
	@brief The PluginManifest struct is the plugin.json of a plugin: who
	made it, which version of the plugin API it needs, and the commands it
	adds, each a function of one of its scripts.
	@code
	{
	  "id": "acme.erp-export",
	  "name": "ERP export",
	  "version": "1.0.0",
	  "author": "Acme",
	  "description": "Writes the parts of the project for the ERP.",
	  "api": 1,
	  "permissions": ["files"],
	  "tab": "Acme",
	  "commands": [
	    {"id": "export", "label": "Export\nto ERP", "tooltip": "...",
	     "icon": "erp.svg", "group": "ERP", "size": "large",
	     "script": "main.js", "function": "exportErp"}
	  ]
	}
	@endcode
	Without "tab", the commands go to the Plugins tab, in a group named
	after the plugin unless a command names its own.
*/
struct PluginManifest
{
	Q_DECLARE_TR_FUNCTIONS(PluginManifest)

	public:
		/// The version of the plugin API this FaseCAD gives
		static constexpr int currentApi = 1;
		/// What a plugin may ask for, and what each one means
		static QStringList knownPermissions();
		static QString permissionText(const QString &permission);

		struct Command {
			QString id;
			QString label;
			QString tooltip;
			QString icon;      ///< a file of the plugin, empty for none
			QString group;
			bool large = false;
			QString script;
			QString function;
		};

		QString id;
		QString name;
		QString version;
		QString author;
		QString description;
		int api = 1;
		QStringList permissions;
		QString tab;
		QList<Command> commands;

		/// Read @p json; @p error says why it is not usable, if it is not
		static PluginManifest parse(const QByteArray &json, QString *error);
		/// The id a plugin may have: letters, digits, ".", "-" and "_"
		static bool isValidId(const QString &id);
};

#endif // PLUGINMANIFEST_H
