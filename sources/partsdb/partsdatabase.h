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
#include <QJsonObject>
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
	@brief What a function of a part does, as an id kept in the database.
	The list is FaseCAD's own; the ids never change, their names are
	translated (see PartFunction::kindName()).
*/
enum class PartFunctionKind {
	Other,
	SupplyPlus,			///< + of a supply : L+, 24 V
	SupplyMinus,		///< - of a supply : M, 0 V
	FunctionalEarth,	///< FE
	ProtectiveEarth,	///< PE
	Phase,				///< L1, L2, L3
	Neutral,			///< N
	DigitalInput,
	DigitalOutput,
	AnalogInput,
	AnalogOutput,
	BusPort,			///< Profinet, Ethernet, Modbus...
	Coil,
	PowerContact,
	NormallyOpen,
	NormallyClosed,
	Changeover,
	Terminal			///< A plain connection point
};

/**
	@brief One function a part brings, as its manufacturer describes it : a
	digital input on the pin 7 of the plug X10 described "DI a.0", the coil
	A1-A2 of a contactor...

	A device placed in a project has these functions, in this order. They
	are what the pins of its symbols are checked against, and what PLC
	boxes are made from.
*/
struct PartFunction
{
	PartFunctionKind kind = PartFunctionKind::Other;
	QString plug;			///< X10
	QString pin;			///< 7, or A1
	QString description;	///< DI a.0
	QString address;		///< Default PLC address, I0.0

	bool operator==(const PartFunction &other) const {
		return kind == other.kind && plug == other.plug && pin == other.pin
				&& description == other.description
				&& address == other.address;
	}

	static QString kindId(PartFunctionKind kind);
	static PartFunctionKind kindFromId(const QString &id);
	static QString kindName(PartFunctionKind kind);
	static QList<PartFunctionKind> kinds();
};

/**
	@brief A connection point of a part : where a wire is screwed or pushed
	on the real device, with what it takes.

	The position is in mm from the top left corner of the front of the
	device, z towards the front. The direction is where the wire comes
	from, as "x y z" (e.g. "0 1 0" from below), kept for the 3D.
*/
struct PartConnection
{
	QString plug;
	QString pin;
	std::optional<double> section_min;		///< mm²
	std::optional<double> section_max;		///< mm²
	std::optional<int> max_wires;
	std::optional<double> strip_length;		///< mm
	std::optional<double> torque;			///< N·m
	QString screwdriver;
	std::optional<double> x;				///< mm
	std::optional<double> y;				///< mm
	std::optional<double> z;				///< mm
	QString direction;

	bool operator==(const PartConnection &other) const {
		return plug == other.plug && pin == other.pin
				&& section_min == other.section_min
				&& section_max == other.section_max
				&& max_wires == other.max_wires
				&& strip_length == other.strip_length
				&& torque == other.torque
				&& screwdriver == other.screwdriver
				&& x == other.x && y == other.y && z == other.z
				&& direction == other.direction;
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
	QString gtin;			///< GTIN / EAN code of the article
		/// Letter of the device tag : K for a PLC or a relay, Q for a
		/// breaker... A placed device gets it, e.g. -K1
	QString code_letter;
		/// The symbols the part is placed with from the parts collection
		/// of the collection panel, as ElementsLocation strings
		/// ("common://10_electric/..."), the first one first: Tab goes
		/// from one to the next while the part is placed
	QStringList symbols;
	std::optional<double> power_loss;		///< W, for the cabinet heat
		/// Room to leave free around the device in the cabinet, in mm
	std::optional<double> clearance_top;
	std::optional<double> clearance_bottom;
	std::optional<double> clearance_left;
	std::optional<double> clearance_right;
	std::optional<double> clearance_front;
	std::optional<double> clearance_rear;
		///Free technical data, in the order the user wrote it.
	QList<QPair<QString, QString>> technical_data;
	QList<PartAccessory> accessories;
		///What the part does, pin by pin, in the manufacturer's order.
	QList<PartFunction> functions;
	QList<PartConnection> connections;
};

/**
	@brief What a file attached to a part is for.
*/
enum class PartFileKind {
	Image,
	Dxf,
	Step	///< 3D model, for the cabinet in 3D
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
	accessories, functions, connection points, images, DXF drawings and
	3D models.

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
		static QJsonObject toJson(const Part &part);
		static Part fromJson(const QJsonObject &object);
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
		bool upgradeSchema(int version, QString *error);
		static QStringList functionTables();
		static QStringList symbolTables();
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
