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
#include "partsarchive.h"

#include "../container/qetzip.h"

#include <QCoreApplication>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace {

QString tr(const char *text)
{
	return QCoreApplication::translate("PartsArchive", text);
}

void report(QString *error, const QString &text)
{
	if (error) {
		*error = text;
	}
}

	/// The entry of the part in the zip
const QString json_entry = QStringLiteral("faspart.json");
	/// What faspart.json says it is, and the version this code writes
const QString format_name = QStringLiteral("faspart");
const int format_version = 1;

QString kindId(PartFileKind kind)
{
	switch (kind) {
		case PartFileKind::Image: return QStringLiteral("image");
		case PartFileKind::Dxf: return QStringLiteral("dxf");
		case PartFileKind::Step: return QStringLiteral("step");
	}
	return QStringLiteral("image");
}

std::optional<PartFileKind> kindFromId(const QString &id)
{
	if (id == QLatin1String("image")) return PartFileKind::Image;
	if (id == QLatin1String("dxf")) return PartFileKind::Dxf;
	if (id == QLatin1String("step")) return PartFileKind::Step;
	return std::nullopt;
}

	/// @p name with only letters, digits, dots, dashes and underscores
QString safeName(const QString &name)
{
	QString safe = name;
	safe.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")),
				 QStringLiteral("_"));
	return safe;
}

}

QString PartsArchive::suffix()
{
	return QStringLiteral("faspart");
}

/**
	@brief PartsArchive::fileNameFor
	@return the name of the .faspart of @p part: its manufacturer and order
	number, "ACME_PLC-1214.faspart"
*/
QString PartsArchive::fileNameFor(const Part &part)
{
	QStringList words;
	for (const QString &word : {part.manufacturer,
								part.order_number.isEmpty()
								? part.description : part.order_number}) {
		if (!word.trimmed().isEmpty()) {
			words << safeName(word.trimmed());
		}
	}
	if (words.isEmpty()) {
		words << QStringLiteral("part");
	}
	return words.join(QLatin1Char('_')) + QLatin1Char('.') + suffix();
}

/**
	@brief PartsArchive::contentOf
	@return what the .faspart of the part @p id of @p database holds: the
	part, its accessories by manufacturer and order number, its files
	(linked ones read from where they are; one that cannot be read is
	left out) and its symbols, read by @p reader
*/
std::optional<PartsArchive::Content> PartsArchive::contentOf(
		const PartsDatabase &database, qint64 id, const SymbolReader &reader,
		QString *error)
{
	const std::optional<Part> part = database.part(id);
	if (!part) {
		report(error, tr("The part no longer exists in the parts database."));
		return std::nullopt;
	}

	Content content;
	content.part = *part;
	content.part.id = 0;
	content.part.accessories.clear();
	content.part.symbols.clear();

	for (const PartAccessory &accessory : part->accessories) {
		if (const std::optional<Part> other = database.part(accessory.accessory_id)) {
			content.accessories.append({other->manufacturer, other->order_number,
										accessory.quantity});
		}
	}

	const QList<PartFile> files = database.files(id);
	for (const PartFile &file : files) {
		QString file_error;
		const QByteArray data = database.fileContent(file.id, &file_error);
		if (!file_error.isEmpty()) {
			continue;
		}
		content.files.append({file.kind, file.file_name, data});
	}

	for (const QString &symbol : part->symbols) {
		const SymbolFile read = reader ? reader(symbol) : SymbolFile();
		if (!read.name.isEmpty() && !read.definition.isEmpty()) {
			content.symbols.append(read);
		}
	}
	return content;
}

/**
	@brief PartsArchive::write
	Write @p content to the .faspart @p path, replacing it
*/
bool PartsArchive::write(const QString &path, const Content &content,
						 QString *error)
{
	QJsonObject root;
	root.insert(QStringLiteral("format"), format_name);
	root.insert(QStringLiteral("version"), format_version);
	root.insert(QStringLiteral("part"), PartsDatabase::toJson(content.part));

	QJsonArray accessories;
	for (const Accessory &accessory : content.accessories) {
		accessories.append(QJsonObject{
			{QStringLiteral("manufacturer"), accessory.manufacturer},
			{QStringLiteral("order_number"), accessory.order_number},
			{QStringLiteral("quantity"), accessory.quantity}});
	}
	root.insert(QStringLiteral("accessories"), accessories);

	QList<QetZip::Entry> entries;
	QJsonArray files;
	int index = 0;
	for (const File &file : content.files) {
		const QString entry = QStringLiteral("files/%1-%2")
				.arg(++index).arg(safeName(file.name));
		files.append(QJsonObject{
			{QStringLiteral("kind"), kindId(file.kind)},
			{QStringLiteral("name"), file.name},
			{QStringLiteral("entry"), entry}});
			//A picture is compressed already
		entries.append({entry, file.content, file.kind != PartFileKind::Image});
	}
	root.insert(QStringLiteral("files"), files);

	QJsonArray symbols;
	index = 0;
	for (const SymbolFile &symbol : content.symbols) {
		const QString entry = QStringLiteral("symbols/%1-%2")
				.arg(++index).arg(safeName(symbol.name));
		symbols.append(QJsonObject{
			{QStringLiteral("name"), symbol.name},
			{QStringLiteral("entry"), entry}});
		entries.append({entry, symbol.definition, true});
	}
	root.insert(QStringLiteral("symbols"), symbols);

	entries.prepend({json_entry, QJsonDocument(root).toJson(QJsonDocument::Indented), true});
	return QetZip::write(path, entries, error);
}

/**
	@brief PartsArchive::read
	@return what the .faspart @p path holds, nothing when it cannot be read
	or is no part file of this version or an older one
*/
std::optional<PartsArchive::Content> PartsArchive::read(const QString &path,
														QString *error)
{
	QList<QetZip::Entry> entries;
	QString zip_error;
	if (!QetZip::read(path, &entries, &zip_error)) {
		report(error, tr("The file cannot be read: %1").arg(zip_error));
		return std::nullopt;
	}
	QHash<QString, QByteArray> data;
	for (const QetZip::Entry &entry : entries) {
		data.insert(entry.name, entry.data);
	}

	const QJsonObject root = QJsonDocument::fromJson(data.value(json_entry)).object();
	if (root.value(QStringLiteral("format")).toString() != format_name) {
		report(error, tr("This is not a FaseCAD part file."));
		return std::nullopt;
	}
	if (root.value(QStringLiteral("version")).toInt() > format_version) {
		report(error, tr("This part file was made by a newer version of FaseCAD."));
		return std::nullopt;
	}

	Content content;
	content.part = PartsDatabase::fromJson(root.value(QStringLiteral("part")).toObject());

	const QJsonArray accessories = root.value(QStringLiteral("accessories")).toArray();
	for (const QJsonValue &value : accessories) {
		const QJsonObject accessory = value.toObject();
		content.accessories.append({
			accessory.value(QStringLiteral("manufacturer")).toString(),
			accessory.value(QStringLiteral("order_number")).toString(),
			accessory.value(QStringLiteral("quantity")).toDouble(1)});
	}

	const QJsonArray files = root.value(QStringLiteral("files")).toArray();
	for (const QJsonValue &value : files) {
		const QJsonObject file = value.toObject();
		const auto kind = kindFromId(file.value(QStringLiteral("kind")).toString());
		const QString entry = file.value(QStringLiteral("entry")).toString();
		if (kind && data.contains(entry)) {
			content.files.append({*kind, file.value(QStringLiteral("name")).toString(),
								  data.value(entry)});
		}
	}

	const QJsonArray symbols = root.value(QStringLiteral("symbols")).toArray();
	for (const QJsonValue &value : symbols) {
		const QJsonObject symbol = value.toObject();
		const QString entry = symbol.value(QStringLiteral("entry")).toString();
		const QString name = symbol.value(QStringLiteral("name")).toString();
		if (!name.isEmpty() && data.contains(entry)) {
			content.symbols.append({name, data.value(entry)});
		}
	}
	return content;
}

/**
	@brief PartsArchive::store
	Put @p content in @p database: a new part, or the part @p replace_id
	replaced, its files with it. The symbols go where @p writer puts them;
	an accessory is kept when @p database knows it.
	@return the id of the part, 0 when it could not be written
*/
qint64 PartsArchive::store(PartsDatabase &database, const Content &content,
						   qint64 replace_id, const SymbolWriter &writer,
						   QString *error)
{
	Part part = content.part;
	part.symbols.clear();
	for (const SymbolFile &symbol : content.symbols) {
		const QString location = writer ? writer(symbol, part) : QString();
		if (!location.isEmpty()) {
			part.symbols << location;
		}
	}
	part.accessories.clear();
	for (const Accessory &accessory : content.accessories) {
		const qint64 id = database.findPart(accessory.manufacturer,
											accessory.order_number);
		if (id && id != replace_id) {
			part.accessories.append({id, accessory.quantity});
		}
	}

	qint64 id = replace_id;
	if (replace_id) {
		part.id = replace_id;
		if (!database.updatePart(part, error)) {
			return 0;
		}
		const QList<PartFile> old_files = database.files(replace_id);
		for (const PartFile &file : old_files) {
			if (!database.removeFile(file.id, error)) {
				return 0;
			}
		}
	} else {
		part.id = 0;
		id = database.addPart(part, error);
		if (!id) {
			return 0;
		}
	}

	for (const File &file : content.files) {
		if (!database.addFile(id, file.kind, file.name, file.content, error)) {
			return 0;
		}
	}
	return id;
}
