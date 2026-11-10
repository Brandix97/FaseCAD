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
#ifndef PARTSDATABASE_H
#define PARTSDATABASE_H

#include <QByteArray>
#include <QCoreApplication>
#include <QList>
#include <QPair>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>

#include <optional>

/**
	@brief An accessory of a part : another part of the database, needed
	with it in a given quantity (a cover, a marker, an end plate...).
*/
struct PartAccessory
{
	qint64 accessory_id = 0;
	double quantity = 1;

	bool operator==(const PartAccessory &other) const {
		return accessory_id == other.accessory_id
				&& quantity == other.quantity;
	}
};

/**
	@brief One part of the parts database.

	The manufacturer and the order number identify the part : they are the
	two element informations that already exist in every project, so an
	element keeps pointing to its part without any change to the file
	format. Numbers the user left empty are empty optionals, not zero.
*/
struct Part
{
	qint64 id = 0;
	QString manufacturer;
	QString order_number;
	QString internal_reference;
	QString supplier;
	QString supplier_reference;
	QString description;
	QString designation;
	QString category;
	QString model;
	QString voltage;
	QString current;
	QString unit;
	QString datasheet_url;
	QString notes;
	std::optional<double> width;	///< mm
	std::optional<double> height;	///< mm
	std::optional<double> depth;	///< mm
	std::optional<double> weight;	///< kg
	std::optional<double> price;
		/// A cable : how many cores it has, 0 for a part which is no
		/// cable. A part with cores is offered as a cable type.
	int core_count = 0;
		/// The colour of each core, in core order, separated by |
	QString core_colors;
	QString section;		///< Cross-section, e.g. 2,5 mm²
	QString sheath_color;
		///Free technical data, in the order the user wrote it.
	QList<QPair<QString, QString>> technical_data;
	QList<PartAccessory> accessories;
};

/**
	@brief What a file attached to a part is for.
*/
enum class PartFileKind {
	Image,
	Dxf
};

/**
	@brief A file attached to a part.

	By default the file is copied inside the database, so it travels with
	it, also when the database is shared on a network folder. A file can
	instead be linked : only its path is kept, and it is read from disk.
*/
struct PartFile
{
	qint64 id = 0;
	qint64 part_id = 0;
	PartFileKind kind = PartFileKind::Image;
	QString file_name;
		///Empty for a copied file.
	QString linked_path;
		///Size of the copied content, in bytes.
	qint64 size = 0;

	bool isLinked() const {return !linked_path.isEmpty();}
};

/**
	@brief The PartsDatabase class reads and writes the parts database, an
	SQLite file listing the parts the user buys, with their technical data,
	accessories, images and DXF drawings.

	This is a pure data class : it does not know anything about widgets,
	so it stays usable from the tests and from the command line.

	The database can be on a network folder used by several people : it is
	opened with a busy timeout, every change is one transaction, and a
	database kept busy by someone else gives a clear error instead of a
	silent failure.
*/
class PartsDatabase
{
	Q_DECLARE_TR_FUNCTIONS(PartsDatabase)

	public:
		PartsDatabase();
		~PartsDatabase();
		PartsDatabase(const PartsDatabase &) = delete;
		PartsDatabase &operator=(const PartsDatabase &) = delete;

		//Settings
		static bool isEnabled();
		static void setEnabled(bool enabled);
		static QString configuredPath();
		static void setConfiguredPath(const QString &path);
		static QString defaultPath();
		static QString path();

		//Database
		static int schemaVersion();
		bool open(const QString &path, QString *error = nullptr);
		void close();
		bool isOpen() const;
		QString fileName() const;

		//Parts
		qint64 addPart(const Part &part, QString *error = nullptr);
		bool updatePart(const Part &part, QString *error = nullptr);
		bool removePart(qint64 id, QString *error = nullptr);
		std::optional<Part> part(qint64 id) const;
		qint64 findPart(const QString &manufacturer,
						const QString &order_number) const;
		QList<Part> parts(const QString &search = QString()) const;
		QStringList categories() const;
		QStringList manufacturers() const;

		//Files
		qint64 addFile(qint64 part_id, PartFileKind kind,
					   const QString &file_name, const QByteArray &content,
					   QString *error = nullptr);
		qint64 addFileFromDisk(qint64 part_id, PartFileKind kind,
							   const QString &path, QString *error = nullptr);
		qint64 linkFile(qint64 part_id, PartFileKind kind,
						const QString &path, QString *error = nullptr);
		bool removeFile(qint64 file_id, QString *error = nullptr);
		QList<PartFile> files(qint64 part_id) const;
		QByteArray fileContent(qint64 file_id, QString *error = nullptr) const;
		bool saveFileAs(qint64 file_id, const QString &path,
						QString *error = nullptr) const;

	private:
		bool createSchema(QString *error);
		bool runStatements(const QStringList &statements, QString *error);
		bool writeDetails(const Part &part, QString *error);
		void readDetails(Part *part) const;
		std::optional<PartFile> file(qint64 file_id) const;
		static QString errorText(const QString &driver_text);
		static QString tooBigText(const QString &file_name);
		static QString missingPartText(const QString &driver_text);

		QString m_connection_name;
		QSqlDatabase m_db;
};

#endif // PARTSDATABASE_H
