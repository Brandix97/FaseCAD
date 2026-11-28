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
#include "pluginmanifest.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

QStringList PluginManifest::knownPermissions()
{
	return {QStringLiteral("files")};
}

QString PluginManifest::permissionText(const QString &permission)
{
	if (permission == QLatin1String("files"))
		return tr("Write files on this computer (exports, reports)");
	return tr("Unknown: %1").arg(permission);
}

bool PluginManifest::isValidId(const QString &id)
{
	static const QRegularExpression valid(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$"));
	return valid.match(id).hasMatch() && !id.contains(QLatin1String(".."));
}

PluginManifest PluginManifest::parse(const QByteArray &json, QString *error)
{
	PluginManifest m;
	auto fail = [&](const QString &why) {
		if (error) *error = why;
		return PluginManifest();
	};

	QJsonParseError parse_error;
	const QJsonDocument doc = QJsonDocument::fromJson(json, &parse_error);
	if (doc.isNull() || !doc.isObject())
		return fail(tr("plugin.json is not a JSON object: %1").arg(parse_error.errorString()));
	const QJsonObject o = doc.object();

	m.id = o.value(QStringLiteral("id")).toString();
	m.name = o.value(QStringLiteral("name")).toString(m.id);
	m.version = o.value(QStringLiteral("version")).toString(QStringLiteral("1.0"));
	m.author = o.value(QStringLiteral("author")).toString();
	m.description = o.value(QStringLiteral("description")).toString();
	m.api = o.value(QStringLiteral("api")).toInt(1);
	m.tab = o.value(QStringLiteral("tab")).toString();
	for (const QJsonValue &p : o.value(QStringLiteral("permissions")).toArray())
		m.permissions << p.toString();

	if (!isValidId(m.id))
		return fail(tr("The id \"%1\" is not usable: letters, digits, \".\", \"-\" and \"_\"").arg(m.id));
	if (m.api < 1)
		return fail(tr("The plugin API %1 does not exist").arg(m.api));
	if (m.api > currentApi)
		return fail(tr("The plugin needs the plugin API %1; this FaseCAD gives %2: update FaseCAD")
					.arg(m.api).arg(currentApi));
	for (const QString &p : std::as_const(m.permissions)) {
		if (!knownPermissions().contains(p))
			return fail(tr("The plugin asks for a permission FaseCAD does not know: %1").arg(p));
	}

	QStringList ids;
	for (const QJsonValue &value : o.value(QStringLiteral("commands")).toArray()) {
		const QJsonObject c = value.toObject();
		Command command;
		command.id = c.value(QStringLiteral("id")).toString();
		command.label = c.value(QStringLiteral("label")).toString(command.id);
		command.tooltip = c.value(QStringLiteral("tooltip")).toString();
		command.icon = c.value(QStringLiteral("icon")).toString();
		command.group = c.value(QStringLiteral("group")).toString();
		command.large = c.value(QStringLiteral("size")).toString() == QLatin1String("large");
		command.script = c.value(QStringLiteral("script")).toString(QStringLiteral("main.js"));
		command.function = c.value(QStringLiteral("function")).toString();
		if (!isValidId(command.id) || ids.contains(command.id))
			return fail(tr("A command has no usable id, or the id \"%1\" is repeated").arg(command.id));
		static const QRegularExpression identifier(QStringLiteral("^[A-Za-z_$][A-Za-z0-9_$]*$"));
		if (!identifier.match(command.function).hasMatch())
			return fail(tr("The command \"%1\" names no function of its script").arg(command.id));
		if (command.script.contains(QLatin1String("..")) || command.script.startsWith(QLatin1Char('/'))
			|| command.icon.contains(QLatin1String("..")) || command.icon.startsWith(QLatin1Char('/')))
			return fail(tr("The command \"%1\" names a file outside the plugin").arg(command.id));
		ids << command.id;
		m.commands << command;
	}
	if (m.commands.isEmpty())
		return fail(tr("The plugin has no command"));
	if (error) error->clear();
	return m;
}
