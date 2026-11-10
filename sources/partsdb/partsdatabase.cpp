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
	{"sheath_color", &Part::sheath_color}
};

const NumberColumn number_columns[] = {
	{"width", &Part::width},
	{"height", &Part::height},
	{"depth", &Part::depth},
	{"weight", &Part::weight},
	{"price", &Part::price}
};

	///The text columns a search looks into.
const char *const searched_columns[] = {
	"manufacturer", "order_number", "internal_reference",
	"supplier_reference", "description", "designation", "category", "model"
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

QString kindName(PartFileKind kind)
{
	return kind == PartFileKind::Dxf ? QStringLiteral("dxf")
									 : QStringLiteral("image");
}

PartFileKind kindFromName(const QString &name)
{
	return name == QLatin1String("dxf") ? PartFileKind::Dxf
										: PartFileKind::Image;
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
	return 2;
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

		//Version 2 : the cable fields. A database of version 1 gets the
		//new columns, its parts keep everything they had and are no
		//cables until told so
	if (version == 1) {
		return runStatements({
			QStringLiteral("ALTER TABLE part ADD COLUMN "
						   "core_count INTEGER NOT NULL DEFAULT 0"),
			QStringLiteral("ALTER TABLE part ADD COLUMN "
						   "core_colors TEXT NOT NULL DEFAULT ''"),
			QStringLiteral("ALTER TABLE part ADD COLUMN "
						   "section TEXT NOT NULL DEFAULT ''"),
			QStringLiteral("ALTER TABLE part ADD COLUMN "
						   "sheath_color TEXT NOT NULL DEFAULT ''"),
			QStringLiteral("UPDATE meta SET value = '2' "
						   "WHERE key = 'schema_version'")}, error);
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
		QStringLiteral("CREATE INDEX part_file_part ON part_file(part_id)"),
		QStringLiteral("INSERT INTO meta (key, value) "
					   "VALUES ('schema_version', '2')")
	};
	return runStatements(statements, error);
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
								 QStringLiteral("part_accessory")}) {
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
	Write the technical data and the accessories of @p part, inside the
	transaction of the caller.
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
	@return the files of the part @p part_id, images first, without their
	content.
*/
QList<PartFile> PartsDatabase::files(qint64 part_id) const
{
	QList<PartFile> result;
	QSqlQuery query(m_db);
	query.prepare(QStringLiteral("SELECT id, kind, file_name, linked_path, "
								 "size FROM part_file WHERE part_id = ? "
								 "ORDER BY kind DESC, id"));
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
