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
#include "partsdatabase.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QSaveFile>
#include <QSettings>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QUuid>
#include <QVariant>

namespace {

	///A copied file larger than this would make a shared database slow
	///for everyone : such a file should be linked instead.
const qint64 max_copied_file_size = 64 * 1024 * 1024;

struct TextColumn
{
	const char *name;
	QString Part::*member;
};

struct NumberColumn
{
	const char *name;
	std::optional<double> Part::*member;
};

const TextColumn text_columns[] = {
	{"manufacturer", &Part::manufacturer},
	{"order_number", &Part::order_number},
	{"internal_reference", &Part::internal_reference},
	{"supplier", &Part::supplier},
	{"supplier_reference", &Part::supplier_reference},
	{"description", &Part::description},
	{"designation", &Part::designation},
	{"category", &Part::category},
	{"model", &Part::model},
	{"voltage", &Part::voltage},
	{"current", &Part::current},
	{"unit", &Part::unit},
	{"datasheet_url", &Part::datasheet_url},
	{"notes", &Part::notes},
	{"core_colors", &Part::core_colors},
	{"section", &Part::section},
	{"sheath_color", &Part::sheath_color},
	{"gtin", &Part::gtin},
	{"code_letter", &Part::code_letter}
};

const NumberColumn number_columns[] = {
	{"width", &Part::width},
	{"height", &Part::height},
	{"depth", &Part::depth},
	{"weight", &Part::weight},
	{"price", &Part::price},
	{"power_loss", &Part::power_loss},
	{"clearance_top", &Part::clearance_top},
	{"clearance_bottom", &Part::clearance_bottom},
	{"clearance_left", &Part::clearance_left},
	{"clearance_right", &Part::clearance_right},
	{"clearance_front", &Part::clearance_front},
	{"clearance_rear", &Part::clearance_rear}
};

	///The text columns a search looks into.
const char *const searched_columns[] = {
	"manufacturer", "order_number", "internal_reference",
	"supplier_reference", "description", "designation", "category", "model",
	"gtin"
};

struct FunctionKindEntry
{
	PartFunctionKind kind;
	const char *id;		///< Written in the database, never changes
	const char *name;
};

const FunctionKindEntry function_kinds[] = {
	{PartFunctionKind::Other, "other",
	 QT_TRANSLATE_NOOP("PartFunction", "Other")},
	{PartFunctionKind::SupplyPlus, "supply_plus",
	 QT_TRANSLATE_NOOP("PartFunction", "Supply + (L+, 24 V)")},
	{PartFunctionKind::SupplyMinus, "supply_minus",
	 QT_TRANSLATE_NOOP("PartFunction", "Supply - (M, 0 V)")},
	{PartFunctionKind::FunctionalEarth, "functional_earth",
	 QT_TRANSLATE_NOOP("PartFunction", "Functional earth (FE)")},
	{PartFunctionKind::ProtectiveEarth, "protective_earth",
	 QT_TRANSLATE_NOOP("PartFunction", "Protective earth (PE)")},
	{PartFunctionKind::Phase, "phase",
	 QT_TRANSLATE_NOOP("PartFunction", "Phase (L)")},
	{PartFunctionKind::Neutral, "neutral",
	 QT_TRANSLATE_NOOP("PartFunction", "Neutral (N)")},
	{PartFunctionKind::DigitalInput, "digital_input",
	 QT_TRANSLATE_NOOP("PartFunction", "Digital input")},
	{PartFunctionKind::DigitalOutput, "digital_output",
	 QT_TRANSLATE_NOOP("PartFunction", "Digital output")},
	{PartFunctionKind::AnalogInput, "analog_input",
	 QT_TRANSLATE_NOOP("PartFunction", "Analog input")},
	{PartFunctionKind::AnalogOutput, "analog_output",
	 QT_TRANSLATE_NOOP("PartFunction", "Analog output")},
	{PartFunctionKind::BusPort, "bus_port",
	 QT_TRANSLATE_NOOP("PartFunction", "Bus port")},
	{PartFunctionKind::Coil, "coil",
	 QT_TRANSLATE_NOOP("PartFunction", "Coil")},
	{PartFunctionKind::PowerContact, "power_contact",
	 QT_TRANSLATE_NOOP("PartFunction", "Power contact")},
	{PartFunctionKind::NormallyOpen, "normally_open",
	 QT_TRANSLATE_NOOP("PartFunction", "Normally open contact")},
	{PartFunctionKind::NormallyClosed, "normally_closed",
	 QT_TRANSLATE_NOOP("PartFunction", "Normally closed contact")},
	{PartFunctionKind::Changeover, "changeover",
	 QT_TRANSLATE_NOOP("PartFunction", "Changeover contact")},
	{PartFunctionKind::Terminal, "terminal",
	 QT_TRANSLATE_NOOP("PartFunction", "Terminal")}
};

void report(QString *error, const QString &text)
{
	if (error) {
		*error = text;
	}
}

QString now()
{
	return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

QVariant toVariant(const std::optional<double> &value)
{
	return value ? QVariant(*value) : QVariant(QMetaType::fromType<double>());
}

std::optional<double> toNumber(const QVariant &value)
{
	if (value.isNull()) {
		return std::nullopt;
	}
	return value.toDouble();
}

QVariant toVariant(const std::optional<int> &value)
{
	return value ? QVariant(*value) : QVariant(QMetaType::fromType<int>());
}

std::optional<int> toInteger(const QVariant &value)
{
	if (value.isNull()) {
		return std::nullopt;
	}
	return value.toInt();
}

	///A text for a NOT NULL column : never bound as NULL.
QString notNull(const QString &text)
{
	const QString trimmed = text.trimmed();
	return trimmed.isNull() ? QStringLiteral("") : trimmed;
}

QString kindName(PartFileKind kind)
{
	switch (kind) {
		case PartFileKind::Dxf: return QStringLiteral("dxf");
		case PartFileKind::Step: return QStringLiteral("step");
		case PartFileKind::Image: break;
	}
	return QStringLiteral("image");
}

PartFileKind kindFromName(const QString &name)
{
	if (name == QLatin1String("dxf")) {
		return PartFileKind::Dxf;
	}
	if (name == QLatin1String("step")) {
		return PartFileKind::Step;
	}
	return PartFileKind::Image;
}

	///Every column of the part table but id, as a comma separated list.
QString columnList()
{
	QStringList columns;
	for (const TextColumn &column : text_columns) {
		columns << QLatin1String(column.name);
	}
	for (const NumberColumn &column : number_columns) {
		columns << QLatin1String(column.name);
	}
	columns << QStringLiteral("core_count");
	return columns.join(QStringLiteral(", "));
}

void bindColumns(QSqlQuery *query, const Part &part)
{
	for (const TextColumn &column : text_columns) {
			//A null QString would be bound as NULL : an empty text is
			//written instead, so empty and unknown are the same thing.
		const QString text = (part.*column.member).trimmed();
		query->bindValue(QLatin1Char(':') + QLatin1String(column.name),
						 text.isNull() ? QStringLiteral("") : text);
	}
	for (const NumberColumn &column : number_columns) {
		query->bindValue(QLatin1Char(':') + QLatin1String(column.name),
						 toVariant(part.*column.member));
	}
	query->bindValue(QStringLiteral(":core_count"),
					 qMax(0, part.core_count));
}

Part readRow(const QSqlQuery &query)
{
	const QSqlRecord record = query.record();
	Part part;
	part.id = query.value(record.indexOf(QStringLiteral("id"))).toLongLong();
	for (const TextColumn &column : text_columns) {
		part.*column.member = query.value(
					record.indexOf(QLatin1String(column.name))).toString();
	}
	for (const NumberColumn &column : number_columns) {
		part.*column.member = toNumber(query.value(
					record.indexOf(QLatin1String(column.name))));
	}
	part.core_count = query.value(
				record.indexOf(QStringLiteral("core_count"))).toInt();
	return part;
}

	///@p text with the LIKE wildcards escaped by a backslash.
QString escapedForLike(QString text)
{
	text.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
	text.replace(QLatin1Char('%'), QStringLiteral("\\%"));
	text.replace(QLatin1Char('_'), QStringLiteral("\\_"));
	return text;
}

} // namespace

/**
	@brief PartFunction::kindId
	@return the id of @p kind written in the database.
*/
QString PartFunction::kindId(PartFunctionKind kind)
{
	for (const FunctionKindEntry &entry : function_kinds) {
		if (entry.kind == kind) {
			return QLatin1String(entry.id);
		}
	}
	return QStringLiteral("other");
}

/**
	@brief PartFunction::kindFromId
	@return the kind of the id @p id, Other for an id this version does not
	know (written by a newer one).
*/
PartFunctionKind PartFunction::kindFromId(const QString &id)
{
	for (const FunctionKindEntry &entry : function_kinds) {
		if (id == QLatin1String(entry.id)) {
			return entry.kind;
		}
	}
	return PartFunctionKind::Other;
}

/**
	@brief PartFunction::kindName
	@return the name of @p kind, in the language of the user.
*/
QString PartFunction::kindName(PartFunctionKind kind)
{
	for (const FunctionKindEntry &entry : function_kinds) {
		if (entry.kind == kind) {
			return QCoreApplication::translate("PartFunction", entry.name);
		}
	}
	return QString();
}

/**
	@brief PartFunction::kinds
	@return every kind, in the order they are offered to the user.
*/
QList<PartFunctionKind> PartFunction::kinds()
{
	QList<PartFunctionKind> result;
	for (const FunctionKindEntry &entry : function_kinds) {
		result << entry.kind;
	}
	return result;
}

PartsDatabase::PartsDatabase()
{}

PartsDatabase::~PartsDatabase()
{
	close();
}

/**
	@brief PartsDatabase::isEnabled
	@return true when the user chose to use the parts database in the
	preferences. Off by default, so nothing changes for whoever does not
	ask for it.
*/
bool PartsDatabase::isEnabled()
{
	QSettings settings;
	return settings.value(QStringLiteral("parts-database/enabled"),
						  false).toBool();
}

void PartsDatabase::setEnabled(bool enabled)
{
	QSettings settings;
	settings.setValue(QStringLiteral("parts-database/enabled"), enabled);
}

/**
	@brief PartsDatabase::configuredPath
	@return the database file chosen in the preferences, or a null QString
	when the user keeps the default one.
*/
QString PartsDatabase::configuredPath()
{
	QSettings settings;
	return settings.value(QStringLiteral("parts-database/path")).toString();
}

void PartsDatabase::setConfiguredPath(const QString &path)
{
	QSettings settings;
	if (path.trimmed().isEmpty()) {
		settings.remove(QStringLiteral("parts-database/path"));
	} else {
		settings.setValue(QStringLiteral("parts-database/path"),
						  path.trimmed());
	}
}

/**
	@brief PartsDatabase::defaultPath
	@return the database used when the user did not choose one : a file in
	the application data folder of this computer.
*/
QString PartsDatabase::defaultPath()
{
	return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
			+ QStringLiteral("/parts.sqlite");
}

/**
	@brief PartsDatabase::path
	@return the database to open : the configured one, or the default one.
*/
QString PartsDatabase::path()
{
	const QString configured = configuredPath();
	return configured.isEmpty() ? defaultPath() : configured;
}

/**
	@brief PartsDatabase::schemaVersion
	@return the version of the tables this code writes. A database written
	by a newer version is not opened, so it is never damaged.
*/
int PartsDatabase::schemaVersion()
{
	return 5;
}

/**
	@brief PartsDatabase::toJson
	@return @p part as JSON, for a file of parts (PartsArchive): every
	field but its id, its accessories, its files and its symbols, which
	name what this database and this computer hold and are written by the
	file itself. An empty field is left out.
*/
QJsonObject PartsDatabase::toJson(const Part &part)
{
	QJsonObject object;
	for (const TextColumn &column : text_columns) {
		const QString value = part.*column.member;
		if (!value.isEmpty()) {
			object.insert(QLatin1String(column.name), value);
		}
	}
	for (const NumberColumn &column : number_columns) {
		if (const std::optional<double> value = part.*column.member) {
			object.insert(QLatin1String(column.name), *value);
		}
	}
	if (part.core_count) {
		object.insert(QStringLiteral("core_count"), part.core_count);
	}

	QJsonArray technical;
	for (const auto &data : part.technical_data) {
		technical.append(QJsonArray{data.first, data.second});
	}
	if (!technical.isEmpty()) {
		object.insert(QStringLiteral("technical_data"), technical);
	}

	QJsonArray functions;
	for (const PartFunction &function : part.functions) {
		functions.append(QJsonObject{
			{QStringLiteral("kind"), PartFunction::kindId(function.kind)},
			{QStringLiteral("plug"), function.plug},
			{QStringLiteral("pin"), function.pin},
			{QStringLiteral("description"), function.description},
			{QStringLiteral("address"), function.address}});
	}
	if (!functions.isEmpty()) {
		object.insert(QStringLiteral("functions"), functions);
	}

	QJsonArray connections;
	for (const PartConnection &connection : part.connections) {
		QJsonObject point{
			{QStringLiteral("plug"), connection.plug},
			{QStringLiteral("pin"), connection.pin},
			{QStringLiteral("screwdriver"), connection.screwdriver},
			{QStringLiteral("direction"), connection.direction}};
		const std::pair<const char *, std::optional<double>> numbers[] = {
			{"section_min", connection.section_min},
			{"section_max", connection.section_max},
			{"strip_length", connection.strip_length},
			{"torque", connection.torque},
			{"x", connection.x}, {"y", connection.y}, {"z", connection.z}};
		for (const auto &[name, value] : numbers) {
			if (value) {
				point.insert(QLatin1String(name), *value);
			}
		}
		if (connection.max_wires) {
			point.insert(QStringLiteral("max_wires"), *connection.max_wires);
		}
		connections.append(point);
	}
	if (!connections.isEmpty()) {
		object.insert(QStringLiteral("connections"), connections);
	}
	return object;
}

/**
	@brief PartsDatabase::fromJson
	@return the part @p object (toJson()) holds, without an id, an
	accessory, a file nor a symbol. What it does not name stays empty.
*/
Part PartsDatabase::fromJson(const QJsonObject &object)
{
	Part part;
	for (const TextColumn &column : text_columns) {
		part.*column.member = object.value(QLatin1String(column.name)).toString();
	}
	for (const NumberColumn &column : number_columns) {
		const QJsonValue value = object.value(QLatin1String(column.name));
		if (value.isDouble()) {
			part.*column.member = value.toDouble();
		}
	}
	part.core_count = object.value(QStringLiteral("core_count")).toInt();

	const QJsonArray technical = object.value(QStringLiteral("technical_data")).toArray();
	for (const QJsonValue &data : technical) {
		const QJsonArray pair = data.toArray();
		if (!pair.at(0).toString().isEmpty()) {
			part.technical_data.append({pair.at(0).toString(), pair.at(1).toString()});
		}
	}

	const QJsonArray functions = object.value(QStringLiteral("functions")).toArray();
	for (const QJsonValue &value : functions) {
		const QJsonObject function = value.toObject();
		part.functions.append({
			PartFunction::kindFromId(function.value(QStringLiteral("kind")).toString()),
			function.value(QStringLiteral("plug")).toString(),
			function.value(QStringLiteral("pin")).toString(),
			function.value(QStringLiteral("description")).toString(),
			function.value(QStringLiteral("address")).toString()});
	}

	const QJsonArray connections = object.value(QStringLiteral("connections")).toArray();
	for (const QJsonValue &value : connections) {
		const QJsonObject point = value.toObject();
		PartConnection connection;
		connection.plug = point.value(QStringLiteral("plug")).toString();
		connection.pin = point.value(QStringLiteral("pin")).toString();
		connection.screwdriver = point.value(QStringLiteral("screwdriver")).toString();
		connection.direction = point.value(QStringLiteral("direction")).toString();
		const auto number = [&point](const char *name) -> std::optional<double> {
			const QJsonValue v = point.value(QLatin1String(name));
			return v.isDouble() ? std::optional<double>(v.toDouble()) : std::nullopt;
		};
		connection.section_min = number("section_min");
		connection.section_max = number("section_max");
		connection.strip_length = number("strip_length");
		connection.torque = number("torque");
		connection.x = number("x");
		connection.y = number("y");
		connection.z = number("z");
		if (point.value(QStringLiteral("max_wires")).isDouble()) {
			connection.max_wires = point.value(QStringLiteral("max_wires")).toInt();
		}
		part.connections.append(connection);
	}
	return part;
}

/**
	@brief PartsDatabase::open
	Open the database @p path, creating the file and its tables when it
	does not exist yet.
	@return true on success, otherwise false and @p error says why.
*/
bool PartsDatabase::open(const QString &path, QString *error)
{
	close();

	const QFileInfo info(path);
	if (!QDir().mkpath(info.absolutePath())) {
		report(error, tr("The folder %1 cannot be created.")
			   .arg(QDir::toNativeSeparators(info.absolutePath())));
		return false;
	}

	m_connection_name = QStringLiteral("qet_parts_")
			+ QUuid::createUuid().toString(QUuid::WithoutBraces);
	m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
									 m_connection_name);
	m_db.setDatabaseName(path);
		//Someone else writing the same shared file makes us wait a
		//little instead of failing at once.
	m_db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
	if (!m_db.open()) {
		report(error, tr("The parts database %1 cannot be opened: %2")
			   .arg(QDir::toNativeSeparators(path),
					m_db.lastError().text()));
		close();
		return false;
	}

	QSqlQuery pragma(m_db);
	pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));

	if (!createSchema(error)) {
		close();
		return false;
	}
	return true;
}

void PartsDatabase::close()
{
	if (m_connection_name.isEmpty()) {
		return;
	}
	m_db.close();
	m_db = QSqlDatabase();
	QSqlDatabase::removeDatabase(m_connection_name);
	m_connection_name.clear();
}

bool PartsDatabase::isOpen() const
{
	return m_db.isOpen();
}

QString PartsDatabase::fileName() const
{
	return m_db.databaseName();
}

/**
	@brief PartsDatabase::createSchema
	Create the tables of an empty database, and refuse a database written
	by a newer version of the program.
*/
bool PartsDatabase::createSchema(QString *error)
{
	QSqlQuery query(m_db);
	int version = 0;
	if (m_db.tables().contains(QStringLiteral("meta"))) {
		query.exec(QStringLiteral(
				"SELECT value FROM meta WHERE key = 'schema_version'"));
		if (query.next()) {
			version = query.value(0).toInt();
		}
			//No statement left open while the tables change
		query.finish();
	}
	if (version > schemaVersion()) {
		report(error, tr("The parts database %1 was made by a newer version"
						 " of the program and cannot be opened.")
			   .arg(QDir::toNativeSeparators(m_db.databaseName())));
		return false;
	}
	if (version == schemaVersion()) {
		return true;
	}
	if (version > 0) {
		return upgradeSchema(version, error);
	}

	const QStringList statements = {
		QStringLiteral("CREATE TABLE meta ("
					   "key TEXT PRIMARY KEY, value TEXT)"),
		QStringLiteral("CREATE TABLE part ("
					   "id INTEGER PRIMARY KEY, "
					   "manufacturer TEXT NOT NULL DEFAULT '', "
					   "order_number TEXT NOT NULL DEFAULT '', "
					   "internal_reference TEXT NOT NULL DEFAULT '', "
					   "supplier TEXT NOT NULL DEFAULT '', "
					   "supplier_reference TEXT NOT NULL DEFAULT '', "
					   "description TEXT NOT NULL DEFAULT '', "
					   "designation TEXT NOT NULL DEFAULT '', "
					   "category TEXT NOT NULL DEFAULT '', "
					   "model TEXT NOT NULL DEFAULT '', "
					   "voltage TEXT NOT NULL DEFAULT '', "
					   "current TEXT NOT NULL DEFAULT '', "
					   "unit TEXT NOT NULL DEFAULT '', "
					   "datasheet_url TEXT NOT NULL DEFAULT '', "
					   "notes TEXT NOT NULL DEFAULT '', "
					   "width REAL, height REAL, depth REAL, "
					   "weight REAL, price REAL, "
					   "core_count INTEGER NOT NULL DEFAULT 0, "
					   "core_colors TEXT NOT NULL DEFAULT '', "
					   "section TEXT NOT NULL DEFAULT '', "
					   "sheath_color TEXT NOT NULL DEFAULT '', "
					   "gtin TEXT NOT NULL DEFAULT '', "
					   "code_letter TEXT NOT NULL DEFAULT '', "
					   "power_loss REAL, "
					   "clearance_top REAL, clearance_bottom REAL, "
					   "clearance_left REAL, clearance_right REAL, "
					   "clearance_front REAL, clearance_rear REAL, "
					   "created TEXT NOT NULL, modified TEXT NOT NULL)"),
			//The manufacturer and the order number identify a part, as
			//they identify it in the element informations.
		QStringLiteral("CREATE UNIQUE INDEX part_identity ON part("
					   "manufacturer COLLATE NOCASE, "
					   "order_number COLLATE NOCASE) "
					   "WHERE order_number <> ''"),
		QStringLiteral("CREATE TABLE part_data ("
					   "part_id INTEGER NOT NULL "
					   "REFERENCES part(id) ON DELETE CASCADE, "
					   "position INTEGER NOT NULL, "
					   "key TEXT NOT NULL, "
					   "value TEXT NOT NULL DEFAULT '', "
					   "PRIMARY KEY (part_id, position))"),
		QStringLiteral("CREATE TABLE part_accessory ("
					   "part_id INTEGER NOT NULL "
					   "REFERENCES part(id) ON DELETE CASCADE, "
					   "position INTEGER NOT NULL, "
					   "accessory_id INTEGER NOT NULL "
					   "REFERENCES part(id) ON DELETE CASCADE, "
					   "quantity REAL NOT NULL DEFAULT 1, "
					   "PRIMARY KEY (part_id, position))"),
			//Files are kept out of the part table, so listing parts
			//never reads them.
		QStringLiteral("CREATE TABLE part_file ("
					   "id INTEGER PRIMARY KEY, "
					   "part_id INTEGER NOT NULL "
					   "REFERENCES part(id) ON DELETE CASCADE, "
					   "kind TEXT NOT NULL, "
					   "file_name TEXT NOT NULL, "
					   "linked_path TEXT NOT NULL DEFAULT '', "
					   "content BLOB, "
					   "size INTEGER NOT NULL DEFAULT 0, "
					   "added TEXT NOT NULL)"),
		QStringLiteral("CREATE INDEX part_file_part ON part_file(part_id)")
	};
	return runStatements(statements + functionTables() + symbolTables()
						 + QStringList{QStringLiteral(
							"INSERT INTO meta (key, value) "
							"VALUES ('schema_version', '5')")},
						 error);
}

/**
	@brief PartsDatabase::upgradeSchema
	Bring a database of the older @p version to the current one, in one
	transaction. Its parts keep everything they had; what is new stays
	empty until the user fills it.
*/
bool PartsDatabase::upgradeSchema(int version, QString *error)
{
	QStringList statements;
		//Version 2 : the cable fields. The parts of a version 1 are no
		//cables until told so
	if (version < 2) {
		statements << QStringLiteral("ALTER TABLE part ADD COLUMN "
									 "core_count INTEGER NOT NULL DEFAULT 0")
				   << QStringLiteral("ALTER TABLE part ADD COLUMN "
									 "core_colors TEXT NOT NULL DEFAULT ''")
				   << QStringLiteral("ALTER TABLE part ADD COLUMN "
									 "section TEXT NOT NULL DEFAULT ''")
				   << QStringLiteral("ALTER TABLE part ADD COLUMN "
									 "sheath_color TEXT NOT NULL DEFAULT ''");
	}
		//Version 3 : article code, device letter, heat, clearances,
		//functions and connection points
	if (version < 3) {
		statements << QStringLiteral("ALTER TABLE part ADD COLUMN "
									 "gtin TEXT NOT NULL DEFAULT ''")
				   << QStringLiteral("ALTER TABLE part ADD COLUMN "
									 "code_letter TEXT NOT NULL DEFAULT ''");
		for (const char *column : {"power_loss", "clearance_top",
								   "clearance_bottom", "clearance_left",
								   "clearance_right", "clearance_front",
								   "clearance_rear"}) {
			statements << QStringLiteral("ALTER TABLE part ADD COLUMN %1 REAL")
						  .arg(QLatin1String(column));
		}
		statements << functionTables();
	}
		//Version 4 : the symbol a part is placed with, from the parts
		//collection of the collection panel
	if (version < 4) {
		statements << QStringLiteral("ALTER TABLE part ADD COLUMN "
									 "symbol TEXT NOT NULL DEFAULT ''");
	}
		//Version 5 : several symbols a part is placed with, the one of
		//version 4 first. The column of version 4 is read no more
	if (version < 5) {
		statements << symbolTables()
				   << QStringLiteral("INSERT INTO part_symbol "
									 "(part_id, position, location) "
									 "SELECT id, 0, symbol FROM part "
									 "WHERE symbol <> ''");
	}
	statements << QStringLiteral("UPDATE meta SET value = '%1' "
								 "WHERE key = 'schema_version'")
				  .arg(schemaVersion());
	return runStatements(statements, error);
}

/**
	@brief PartsDatabase::symbolTables
	@return the statements creating the table of the symbols a part is
	placed with (version 5), in the order the user gave them.
*/
QStringList PartsDatabase::symbolTables()
{
	return {
		QStringLiteral("CREATE TABLE part_symbol ("
					   "part_id INTEGER NOT NULL "
					   "REFERENCES part(id) ON DELETE CASCADE, "
					   "position INTEGER NOT NULL, "
					   "location TEXT NOT NULL, "
					   "PRIMARY KEY (part_id, position))")
	};
}

/**
	@brief PartsDatabase::functionTables
	@return the statements creating the tables of the functions and of the
	connection points of the parts (version 3).
*/
QStringList PartsDatabase::functionTables()
{
	return {
		QStringLiteral("CREATE TABLE part_function ("
					   "part_id INTEGER NOT NULL "
					   "REFERENCES part(id) ON DELETE CASCADE, "
					   "position INTEGER NOT NULL, "
					   "kind TEXT NOT NULL DEFAULT 'other', "
					   "plug TEXT NOT NULL DEFAULT '', "
					   "pin TEXT NOT NULL DEFAULT '', "
					   "description TEXT NOT NULL DEFAULT '', "
					   "address TEXT NOT NULL DEFAULT '', "
					   "PRIMARY KEY (part_id, position))"),
		QStringLiteral("CREATE TABLE part_connection ("
					   "part_id INTEGER NOT NULL "
					   "REFERENCES part(id) ON DELETE CASCADE, "
					   "position INTEGER NOT NULL, "
					   "plug TEXT NOT NULL DEFAULT '', "
					   "pin TEXT NOT NULL DEFAULT '', "
					   "section_min REAL, section_max REAL, "
					   "max_wires INTEGER, "
					   "strip_length REAL, torque REAL, "
					   "screwdriver TEXT NOT NULL DEFAULT '', "
					   "x REAL, y REAL, z REAL, "
					   "direction TEXT NOT NULL DEFAULT '', "
					   "PRIMARY KEY (part_id, position))")
	};
}

/**
	@brief PartsDatabase::runStatements
	Run @p statements as one transaction : all of them or none.
*/
bool PartsDatabase::runStatements(const QStringList &statements,
								  QString *error)
{
	QSqlQuery query(m_db);
	if (!m_db.transaction()) {
		report(error, errorText(m_db.lastError().text()));
		return false;
	}
	for (const QString &statement : statements) {
		if (!query.exec(statement)) {
			report(error, errorText(query.lastError().text()));
			m_db.rollback();
			return false;
		}
	}
	if (!m_db.commit()) {
		report(error, errorText(m_db.lastError().text()));
		m_db.rollback();
		return false;
	}
	return true;
}

/**
	@brief PartsDatabase::errorText
	@return a message the user can act on for an error of the driver : a
	database kept busy by someone else is said so plainly.
*/
QString PartsDatabase::errorText(const QString &driver_text)
{
	if (driver_text.contains(QLatin1String("locked"), Qt::CaseInsensitive)
			|| driver_text.contains(QLatin1String("busy"), Qt::CaseInsensitive)) {
		return tr("The parts database is being used by someone else."
				  " Try again in a moment.");
	}
	return tr("The parts database cannot be written: %1").arg(driver_text);
}

QString PartsDatabase::tooBigText(const QString &file_name)
{
	return tr("The file %1 is too big to be copied in the parts database"
			  " (more than %2 MB). Link it instead.")
			.arg(file_name)
			.arg(max_copied_file_size / (1024 * 1024));
}

/**
	@brief PartsDatabase::missingPartText
	@return the message for a file attached to a part that does not exist,
	which the database reports as a broken foreign key.
*/
QString PartsDatabase::missingPartText(const QString &driver_text)
{
	if (driver_text.contains(QLatin1String("FOREIGN KEY"),
							 Qt::CaseInsensitive)) {
		return tr("This part no longer exists in the parts database.");
	}
	return errorText(driver_text);
}

/**
	@brief PartsDatabase::addPart
	Add @p part, with its technical data and accessories. Its id is ignored.
	@return the id of the new part, or 0 on failure.
*/
qint64 PartsDatabase::addPart(const Part &part, QString *error)
{
	if (!part.order_number.trimmed().isEmpty()
			&& findPart(part.manufacturer, part.order_number)) {
		report(error, tr("A part with the manufacturer \"%1\" and the order"
						 " number \"%2\" already exists.")
			   .arg(part.manufacturer.trimmed(),
					part.order_number.trimmed()));
		return 0;
	}
	if (!m_db.transaction()) {
		report(error, errorText(m_db.lastError().text()));
		return 0;
	}

	QString columns = columnList();
	QString values = QLatin1Char(':') + columns;
	values.replace(QStringLiteral(", "), QStringLiteral(", :"));

	QSqlQuery query(m_db);
	query.prepare(QStringLiteral("INSERT INTO part (%1, created, modified) "
								 "VALUES (%2, :created, :modified)")
				  .arg(columns, values));
	bindColumns(&query, part);
	const QString time = now();
	query.bindValue(QStringLiteral(":created"), time);
	query.bindValue(QStringLiteral(":modified"), time);
	if (!query.exec()) {
		report(error, errorText(query.lastError().text()));
		m_db.rollback();
		return 0;
	}

	Part written = part;
	written.id = query.lastInsertId().toLongLong();
	if (!writeDetails(written, error) || !m_db.commit()) {
		if (error && error->isEmpty()) {
			report(error, errorText(m_db.lastError().text()));
		}
		m_db.rollback();
		return 0;
	}
	return written.id;
}

/**
	@brief PartsDatabase::updatePart
	Replace the part of id @p part.id by @p part, with its technical data
	and accessories.
*/
bool PartsDatabase::updatePart(const Part &part, QString *error)
{
	if (!PartsDatabase::part(part.id)) {
		report(error, tr("This part no longer exists in the parts"
						 " database."));
		return false;
	}
	const qint64 same = findPart(part.manufacturer, part.order_number);
	if (same && same != part.id) {
		report(error, tr("A part with the manufacturer \"%1\" and the order"
						 " number \"%2\" already exists.")
			   .arg(part.manufacturer.trimmed(),
					part.order_number.trimmed()));
		return false;
	}
	if (!m_db.transaction()) {
		report(error, errorText(m_db.lastError().text()));
		return false;
	}

	QStringList assignments;
	for (const QString &column : columnList().split(QStringLiteral(", "))) {
		assignments << column + QStringLiteral(" = :") + column;
	}
	QSqlQuery query(m_db);
	query.prepare(QStringLiteral("UPDATE part SET %1, modified = :modified "
								 "WHERE id = :id")
				  .arg(assignments.join(QStringLiteral(", "))));
	bindColumns(&query, part);
	query.bindValue(QStringLiteral(":modified"), now());
	query.bindValue(QStringLiteral(":id"), part.id);
	if (!query.exec()) {
		report(error, errorText(query.lastError().text()));
		m_db.rollback();
		return false;
	}

	QSqlQuery clear(m_db);
	for (const QString &table : {QStringLiteral("part_data"),
								 QStringLiteral("part_accessory"),
								 QStringLiteral("part_function"),
								 QStringLiteral("part_connection"),
								 QStringLiteral("part_symbol")}) {
		clear.prepare(QStringLiteral("DELETE FROM %1 WHERE part_id = ?")
					  .arg(table));
		clear.addBindValue(part.id);
		if (!clear.exec()) {
			report(error, errorText(clear.lastError().text()));
			m_db.rollback();
			return false;
		}
	}

	if (!writeDetails(part, error) || !m_db.commit()) {
		if (error && error->isEmpty()) {
			report(error, errorText(m_db.lastError().text()));
		}
		m_db.rollback();
		return false;
	}
	return true;
}

/**
	@brief PartsDatabase::writeDetails
	Write the technical data, the accessories, the functions and the
	connection points of @p part, inside the transaction of the caller.
*/
bool PartsDatabase::writeDetails(const Part &part, QString *error)
{
	QSqlQuery query(m_db);
	int position = 0;
	for (const auto &data : part.technical_data) {
		if (data.first.trimmed().isEmpty()) {
			continue;
		}
		query.prepare(QStringLiteral("INSERT INTO part_data "
									 "(part_id, position, key, value) "
									 "VALUES (?, ?, ?, ?)"));
		query.addBindValue(part.id);
		query.addBindValue(position++);
		query.addBindValue(data.first.trimmed());
		query.addBindValue(data.second.trimmed());
		if (!query.exec()) {
			report(error, errorText(query.lastError().text()));
			return false;
		}
	}

	position = 0;
	for (const PartAccessory &accessory : part.accessories) {
		if (accessory.accessory_id == part.id) {
			report(error, tr("A part cannot be its own accessory."));
			return false;
		}
		query.prepare(QStringLiteral("INSERT INTO part_accessory "
									 "(part_id, position, accessory_id, "
									 "quantity) VALUES (?, ?, ?, ?)"));
		query.addBindValue(part.id);
		query.addBindValue(position++);
		query.addBindValue(accessory.accessory_id);
		query.addBindValue(accessory.quantity);
		if (!query.exec()) {
			report(error, query.lastError().text().contains(
					   QLatin1String("FOREIGN KEY"), Qt::CaseInsensitive)
				   ? tr("An accessory of this part no longer exists in the"
						" parts database.")
				   : errorText(query.lastError().text()));
			return false;
		}
	}

	position = 0;
	for (const PartFunction &function : part.functions) {
		query.prepare(QStringLiteral("INSERT INTO part_function "
									 "(part_id, position, kind, plug, pin, "
									 "description, address) "
									 "VALUES (?, ?, ?, ?, ?, ?, ?)"));
		query.addBindValue(part.id);
		query.addBindValue(position++);
		query.addBindValue(PartFunction::kindId(function.kind));
		query.addBindValue(notNull(function.plug));
		query.addBindValue(notNull(function.pin));
		query.addBindValue(notNull(function.description));
		query.addBindValue(notNull(function.address));
		if (!query.exec()) {
			report(error, errorText(query.lastError().text()));
			return false;
		}
	}

	position = 0;
	for (const PartConnection &connection : part.connections) {
		query.prepare(QStringLiteral("INSERT INTO part_connection "
									 "(part_id, position, plug, pin, "
									 "section_min, section_max, max_wires, "
									 "strip_length, torque, screwdriver, "
									 "x, y, z, direction) "
									 "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
									 "?, ?, ?, ?)"));
		query.addBindValue(part.id);
		query.addBindValue(position++);
		query.addBindValue(notNull(connection.plug));
		query.addBindValue(notNull(connection.pin));
		query.addBindValue(toVariant(connection.section_min));
		query.addBindValue(toVariant(connection.section_max));
		query.addBindValue(toVariant(connection.max_wires));
		query.addBindValue(toVariant(connection.strip_length));
		query.addBindValue(toVariant(connection.torque));
		query.addBindValue(notNull(connection.screwdriver));
		query.addBindValue(toVariant(connection.x));
		query.addBindValue(toVariant(connection.y));
		query.addBindValue(toVariant(connection.z));
		query.addBindValue(notNull(connection.direction));
		if (!query.exec()) {
			report(error, errorText(query.lastError().text()));
			return false;
		}
	}

	position = 0;
	for (const QString &symbol : part.symbols) {
		if (symbol.trimmed().isEmpty()) {
			continue;
		}
		query.prepare(QStringLiteral("INSERT INTO part_symbol "
									 "(part_id, position, location) "
									 "VALUES (?, ?, ?)"));
		query.addBindValue(part.id);
		query.addBindValue(position++);
		query.addBindValue(symbol.trimmed());
		if (!query.exec()) {
			report(error, errorText(query.lastError().text()));
			return false;
		}
	}
	return true;
}

/**
	@brief PartsDatabase::removePart
	Remove the part @p id, with its files, technical data and accessories.
	The parts that had it as an accessory lose it.
*/
bool PartsDatabase::removePart(qint64 id, QString *error)
{
	QSqlQuery query(m_db);
	query.prepare(QStringLiteral("DELETE FROM part WHERE id = ?"));
	query.addBindValue(id);
	if (!query.exec()) {
		report(error, errorText(query.lastError().text()));
		return false;
	}
	return true;
}

/**
	@brief PartsDatabase::part
	@return the part @p id with its technical data and accessories, or no
	value when it does not exist.
*/
std::optional<Part> PartsDatabase::part(qint64 id) const
{
	QSqlQuery query(m_db);
	query.prepare(QStringLiteral("SELECT id, %1 FROM part WHERE id = ?")
				  .arg(columnList()));
	query.addBindValue(id);
	if (!query.exec() || !query.next()) {
		return std::nullopt;
	}
	Part part = readRow(query);
	readDetails(&part);
	return part;
}

void PartsDatabase::readDetails(Part *part) const
{
	QSqlQuery query(m_db);
	query.prepare(QStringLiteral("SELECT key, value FROM part_data "
								 "WHERE part_id = ? ORDER BY position"));
	query.addBindValue(part->id);
	if (query.exec()) {
		while (query.next()) {
			part->technical_data.append({query.value(0).toString(),
										 query.value(1).toString()});
		}
	}

	query.prepare(QStringLiteral("SELECT accessory_id, quantity "
								 "FROM part_accessory WHERE part_id = ? "
								 "ORDER BY position"));
	query.addBindValue(part->id);
	if (query.exec()) {
		while (query.next()) {
			PartAccessory accessory;
			accessory.accessory_id = query.value(0).toLongLong();
			accessory.quantity = query.value(1).toDouble();
			part->accessories.append(accessory);
		}
	}

	query.prepare(QStringLiteral("SELECT kind, plug, pin, description, "
								 "address FROM part_function "
								 "WHERE part_id = ? ORDER BY position"));
	query.addBindValue(part->id);
	if (query.exec()) {
		while (query.next()) {
			PartFunction function;
			function.kind = PartFunction::kindFromId(
						query.value(0).toString());
			function.plug = query.value(1).toString();
			function.pin = query.value(2).toString();
			function.description = query.value(3).toString();
			function.address = query.value(4).toString();
			part->functions.append(function);
		}
	}

	query.prepare(QStringLiteral("SELECT plug, pin, section_min, "
								 "section_max, max_wires, strip_length, "
								 "torque, screwdriver, x, y, z, direction "
								 "FROM part_connection WHERE part_id = ? "
								 "ORDER BY position"));
	query.addBindValue(part->id);
	if (query.exec()) {
		while (query.next()) {
			PartConnection connection;
			connection.plug = query.value(0).toString();
			connection.pin = query.value(1).toString();
			connection.section_min = toNumber(query.value(2));
			connection.section_max = toNumber(query.value(3));
			connection.max_wires = toInteger(query.value(4));
			connection.strip_length = toNumber(query.value(5));
			connection.torque = toNumber(query.value(6));
			connection.screwdriver = query.value(7).toString();
			connection.x = toNumber(query.value(8));
			connection.y = toNumber(query.value(9));
			connection.z = toNumber(query.value(10));
			connection.direction = query.value(11).toString();
			part->connections.append(connection);
		}
	}

	query.prepare(QStringLiteral("SELECT location FROM part_symbol "
								 "WHERE part_id = ? ORDER BY position"));
	query.addBindValue(part->id);
	if (query.exec()) {
		while (query.next()) {
			part->symbols.append(query.value(0).toString());
		}
	}
}

/**
	@brief PartsDatabase::findPart
	@return the id of the part with this manufacturer and order number,
	compared without case, or 0 when there is none. An empty order number
	never identifies a part.
*/
qint64 PartsDatabase::findPart(const QString &manufacturer,
							   const QString &order_number) const
{
	if (order_number.trimmed().isEmpty()) {
		return 0;
	}
	QSqlQuery query(m_db);
	query.prepare(QStringLiteral("SELECT id FROM part "
								 "WHERE manufacturer = ? COLLATE NOCASE "
								 "AND order_number = ? COLLATE NOCASE "
								 "AND order_number <> ''"));
	query.addBindValue(manufacturer.trimmed());
	query.addBindValue(order_number.trimmed());
	if (query.exec() && query.next()) {
		return query.value(0).toLongLong();
	}
	return 0;
}

/**
	@brief PartsDatabase::parts
	@param search : words that must all be found, each in any of the
	identifying or describing texts of a part. Empty lists every part.
	@return the parts, sorted by manufacturer then order number. Their
	technical data and accessories are not read : part() gives them.
*/
QList<Part> PartsDatabase::parts(const QString &search) const
{
	QStringList conditions;
	QVariantList values;
	const QStringList words = search.split(QLatin1Char(' '),
										   Qt::SkipEmptyParts);
	for (const QString &word : words) {
		QStringList any;
		for (const char *column : searched_columns) {
			any << QLatin1String(column)
				+ QStringLiteral(" LIKE ? ESCAPE '\\'");
			values << QLatin1Char('%') + escapedForLike(word)
					  + QLatin1Char('%');
		}
		conditions << QLatin1Char('(') + any.join(QStringLiteral(" OR "))
					  + QLatin1Char(')');
	}

	QString sql = QStringLiteral("SELECT id, %1 FROM part").arg(columnList());
	if (!conditions.isEmpty()) {
		sql += QStringLiteral(" WHERE ")
				+ conditions.join(QStringLiteral(" AND "));
	}
	sql += QStringLiteral(" ORDER BY manufacturer COLLATE NOCASE, "
						  "order_number COLLATE NOCASE, id");

	QSqlQuery query(m_db);
	query.prepare(sql);
	for (const QVariant &value : values) {
		query.addBindValue(value);
	}
	QList<Part> result;
	if (query.exec()) {
		while (query.next()) {
			result.append(readRow(query));
		}
	}
	return result;
}

/**
	@brief PartsDatabase::categories
	@return the categories in use, sorted, without the empty one.
*/
QStringList PartsDatabase::categories() const
{
	QStringList result;
	QSqlQuery query(m_db);
	if (query.exec(QStringLiteral("SELECT DISTINCT category FROM part "
								  "WHERE category <> '' "
								  "ORDER BY category COLLATE NOCASE"))) {
		while (query.next()) {
			result << query.value(0).toString();
		}
	}
	return result;
}

/**
	@brief PartsDatabase::manufacturers
	@return the manufacturers in use, sorted, without the empty one.
*/
QStringList PartsDatabase::manufacturers() const
{
	QStringList result;
	QSqlQuery query(m_db);
	if (query.exec(QStringLiteral("SELECT DISTINCT manufacturer FROM part "
								  "WHERE manufacturer <> '' "
								  "ORDER BY manufacturer COLLATE NOCASE"))) {
		while (query.next()) {
			result << query.value(0).toString();
		}
	}
	return result;
}

/**
	@brief PartsDatabase::addFile
	Copy @p content inside the database, as the file @p file_name of the
	part @p part_id.
	@return the id of the file, or 0 on failure.
*/
qint64 PartsDatabase::addFile(qint64 part_id, PartFileKind kind,
							  const QString &file_name,
							  const QByteArray &content, QString *error)
{
	if (content.size() > max_copied_file_size) {
		report(error, tooBigText(file_name));
		return 0;
	}
	QSqlQuery query(m_db);
	query.prepare(QStringLiteral("INSERT INTO part_file "
								 "(part_id, kind, file_name, content, size, "
								 "added) VALUES (?, ?, ?, ?, ?, ?)"));
	query.addBindValue(part_id);
	query.addBindValue(kindName(kind));
	query.addBindValue(file_name);
	query.addBindValue(content);
	query.addBindValue(qint64(content.size()));
	query.addBindValue(now());
	if (!query.exec()) {
		report(error, missingPartText(query.lastError().text()));
		return 0;
	}
	return query.lastInsertId().toLongLong();
}

/**
	@brief PartsDatabase::addFileFromDisk
	Copy the file @p path inside the database, for the part @p part_id.
	@return the id of the file, or 0 on failure.
*/
qint64 PartsDatabase::addFileFromDisk(qint64 part_id, PartFileKind kind,
									  const QString &path, QString *error)
{
	QFile file(path);
		//Checked before reading, so a huge file is never loaded
	if (file.size() > max_copied_file_size) {
		report(error, tooBigText(QFileInfo(path).fileName()));
		return 0;
	}
	if (!file.open(QIODevice::ReadOnly)) {
		report(error, tr("The file %1 cannot be read.")
			   .arg(QDir::toNativeSeparators(path)));
		return 0;
	}
	return addFile(part_id, kind, QFileInfo(path).fileName(),
				   file.readAll(), error);
}

/**
	@brief PartsDatabase::linkFile
	Attach the file @p path to the part @p part_id without copying it : only
	its path is kept.
	@return the id of the file, or 0 on failure.
*/
qint64 PartsDatabase::linkFile(qint64 part_id, PartFileKind kind,
							   const QString &path, QString *error)
{
	const QFileInfo info(path);
	if (!info.isFile()) {
		report(error, tr("The file %1 cannot be found.")
			   .arg(QDir::toNativeSeparators(path)));
		return 0;
	}
	QSqlQuery query(m_db);
	query.prepare(QStringLiteral("INSERT INTO part_file "
								 "(part_id, kind, file_name, linked_path, "
								 "size, added) VALUES (?, ?, ?, ?, ?, ?)"));
	query.addBindValue(part_id);
	query.addBindValue(kindName(kind));
	query.addBindValue(info.fileName());
	query.addBindValue(info.absoluteFilePath());
	query.addBindValue(info.size());
	query.addBindValue(now());
	if (!query.exec()) {
		report(error, missingPartText(query.lastError().text()));
		return 0;
	}
	return query.lastInsertId().toLongLong();
}

/**
	@brief PartsDatabase::removeFile
	Remove the file @p file_id from its part. A linked file stays on disk.
*/
bool PartsDatabase::removeFile(qint64 file_id, QString *error)
{
	QSqlQuery query(m_db);
	query.prepare(QStringLiteral("DELETE FROM part_file WHERE id = ?"));
	query.addBindValue(file_id);
	if (!query.exec()) {
		report(error, errorText(query.lastError().text()));
		return false;
	}
	return true;
}

/**
	@brief PartsDatabase::files
	@return the files of the part @p part_id, images first, then DXF
	drawings, then 3D models, without their content.
*/
QList<PartFile> PartsDatabase::files(qint64 part_id) const
{
	QList<PartFile> result;
	QSqlQuery query(m_db);
	query.prepare(QStringLiteral("SELECT id, kind, file_name, linked_path, "
								 "size FROM part_file WHERE part_id = ? "
								 "ORDER BY CASE kind WHEN 'image' THEN 0 "
								 "WHEN 'dxf' THEN 1 ELSE 2 END, id"));
	query.addBindValue(part_id);
	if (query.exec()) {
		while (query.next()) {
			PartFile file;
			file.id = query.value(0).toLongLong();
			file.part_id = part_id;
			file.kind = kindFromName(query.value(1).toString());
			file.file_name = query.value(2).toString();
			file.linked_path = query.value(3).toString();
			file.size = query.value(4).toLongLong();
			result.append(file);
		}
	}
	return result;
}

std::optional<PartFile> PartsDatabase::file(qint64 file_id) const
{
	QSqlQuery query(m_db);
	query.prepare(QStringLiteral("SELECT part_id FROM part_file "
								 "WHERE id = ?"));
	query.addBindValue(file_id);
	if (!query.exec() || !query.next()) {
		return std::nullopt;
	}
	for (const PartFile &file : files(query.value(0).toLongLong())) {
		if (file.id == file_id) {
			return file;
		}
	}
	return std::nullopt;
}

/**
	@brief PartsDatabase::fileContent
	@return the content of the file @p file_id : read from the database for
	a copied file, from disk for a linked one.
*/
QByteArray PartsDatabase::fileContent(qint64 file_id, QString *error) const
{
	const std::optional<PartFile> info = file(file_id);
	if (!info) {
		report(error, tr("This file no longer exists in the parts"
						 " database."));
		return QByteArray();
	}
	if (info->isLinked()) {
		QFile disk_file(info->linked_path);
		if (!disk_file.open(QIODevice::ReadOnly)) {
			report(error, tr("The linked file %1 cannot be found.")
				   .arg(QDir::toNativeSeparators(info->linked_path)));
			return QByteArray();
		}
		return disk_file.readAll();
	}

	QSqlQuery query(m_db);
	query.prepare(QStringLiteral("SELECT content FROM part_file "
								 "WHERE id = ?"));
	query.addBindValue(file_id);
	if (!query.exec() || !query.next()) {
		report(error, errorText(query.lastError().text()));
		return QByteArray();
	}
	return query.value(0).toByteArray();
}

/**
	@brief PartsDatabase::saveFileAs
	Write the content of the file @p file_id to @p path, for instance to
	use a DXF drawing in another program.
*/
bool PartsDatabase::saveFileAs(qint64 file_id, const QString &path,
							   QString *error) const
{
	QString read_error;
	const QByteArray content = fileContent(file_id, &read_error);
	if (!read_error.isEmpty()) {
		report(error, read_error);
		return false;
	}
	QSaveFile out(path);
	if (!out.open(QIODevice::WriteOnly)
			|| out.write(content) != content.size()
			|| !out.commit()) {
		report(error, tr("The file %1 cannot be written.")
			   .arg(QDir::toNativeSeparators(path)));
		return false;
	}
	return true;
}
