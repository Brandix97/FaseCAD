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

#include <functional>
#include <QByteArray>
#include <QHash>
#include <QCoreApplication>
#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QSet>
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
		/// A, the most current downstream of the device: the wires it
		/// feeds get the section of it (WireSections, Brandon, 2026-11-27)
	std::optional<double> max_current;
		/// The textual description and the model in other languages, as
		/// JSON {"description": {"en": "..."}, "model": {...}}
		/// (PartTranslations, Brandon, 2026-11-27)
	QString translations;
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
	@brief The textual description and the model of a part in other
	languages (Brandon, 2026-11-27): the folios and the lists of a project
	show them in the language of its part data (QETProject::partsLanguage),
	the text of the part itself when it has none in that language.
*/
namespace PartTranslations
{
		/// The fields that can be translated: "description", "model"
	QStringList fields();
		/// The languages offered: "es", "en", "de", "fr", "it", "pt",
		/// "tr", "zh"
	QStringList languages();
		/// The name of @p language, in it ("Deutsch")
	QString languageName(const QString &language);
		/// @p field of @p part in @p language; empty when it has none
	QString translation(const Part &part, const QString &field, const QString &language);
	void setTranslation(Part &part, const QString &field, const QString &language,
						const QString &text);
		/// @p field of @p part in @p language, or its own text
	QString text(const Part &part, const QString &field, const QString &language);
}

/**
	@brief What a file attached to a part is for.
*/
enum class PartFileKind {
	Image,
	Dxf,
	Step,	///< 3D model, for the cabinet in 3D
	Symbol	///< a symbol (.elmt) of the part, kept in the database (PartsDbSymbols)
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
	@brief A category inside another (Brandon, 2026-11-21): its path, the
	categories it is in first, joined by " › " ("Relays › Interface"). It
	is kept as the category of its parts, so an older FaseCAD reads it as
	a category of its own.
*/
namespace PartCategoryPath
{
	QString separator();
		/// The names of @p path, from the outer category in
	QStringList split(const QString &path);
	QString join(const QStringList &names);
		/// @p path written the one way: names trimmed, one separator
	QString normalized(const QString &path);
		/// The category @p path is in, empty for one at the top
	QString parent(const QString &path);
		/// The last name of @p path
	QString leaf(const QString &path);
		/// Whether @p path is @p ancestor or one of its subcategories,
		/// whatever the case
	bool isWithin(const QString &path, const QString &ancestor);
		/// The order of a tree: a category, then its subcategories
	bool less(const QString &a, const QString &b);
}

/**
	@brief A category of parts (Brandon, 2026-11-21): its name and its
	icon, an image file (SVG or PNG) kept in the database, empty for none.
	The categories are those made in the categories manager and those the
	parts have.
*/
struct PartCategory
{
	QString name;
	QByteArray icon;
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
			/// The database to open: the server when useServer(), else
			/// the configured file, else the default one
		static QString path();

			/// A parts database on a PostgreSQL server (Brandon,
			/// 2026-11-25), for several computers at once
		struct Server
		{
			QString host;
			int port = 5432;
			QString database;
			QString user;
			QString password;
			bool isValid() const {return !host.isEmpty() && !database.isEmpty();}
				/// postgresql://user@host:port/database, no password
			QString toUrl() const;
				/// The password of the configured server when the URL
				/// has none and names that server
			static Server fromUrl(const QString &url);
		};
		static bool isServerPath(const QString &path);
		static Server configuredServer();
			/// The password is kept apart in the settings
		static void setConfiguredServer(const Server &server);
		static bool useServer();
		static void setUseServer(bool use);
			/// @p statement, written for SQLite, as PostgreSQL reads it
		static QString serverSql(const QString &statement);

		//Database
		static int schemaVersion();
		static QJsonObject toJson(const Part &part);
		static Part fromJson(const QJsonObject &object);
		bool open(const QString &path, QString *error = nullptr);
		bool openServer(const Server &server, QString *error = nullptr);
		void close();
		bool isServer() const {return m_server;}
			/// Every part, file and category of @p source copied here
		int copyFrom(const PartsDatabase &source, QString *error = nullptr,
					 const std::function<bool(int, int)> &progress = {});
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

		//Categories
			/// "Cables": always in a database, never removed nor renamed;
			/// its parts are the ones with the Cable tab (Brandon,
			/// 2026-11-27)
		static QString cablesCategory();
		static bool isDefaultCategory(const QString &name);
		QList<PartCategory> categoryList() const;
			/// The parts of category @p name, and of its subcategories
			/// with @p with_subcategories
		int partsInCategory(const QString &name, bool with_subcategories = false) const;
			/// @p name a path makes a subcategory, its categories with it
		bool addCategory(const QString &name, QString *error = nullptr);
			/// Its subcategories, icons and parts follow; each path
			/// changed into @p moved (old, new)
		bool renameCategory(const QString &name, const QString &new_name,
							QString *error = nullptr, QHash<QString, QString> *moved = nullptr);
			/// Its parts go to the category it is in (none at the top),
			/// its subcategories up one level, with their parts; each path
			/// changed into @p moved (old, new; empty for none)
		bool removeCategory(const QString &name, QString *error = nullptr,
							QHash<QString, QString> *moved = nullptr);
		bool setCategoryIcon(const QString &name, const QByteArray &icon,
							 QString *error = nullptr);

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
			/// The file @p file_id, an id of 0 when there is none
		PartFile fileOf(qint64 file_id) const;
			/// The first image of every part that has one: part id, file id
		QHash<qint64, qint64> firstImages() const;
			/// The parts with at least one file of @p kind
		QSet<qint64> partsWithFile(PartFileKind kind) const;
		QByteArray fileContent(qint64 file_id, QString *error = nullptr) const;
		bool saveFileAs(qint64 file_id, const QString &path,
						QString *error = nullptr) const;

	private:
		bool createSchema(QString *error);
		void addDefaultCategories();
		bool upgradeSchema(int version, QString *error);
		static QStringList functionTables();
		static QStringList symbolTables();
		static QString categoryTable();
		bool runStatements(const QStringList &statements, QString *error);
		bool writeDetails(const Part &part, QString *error);
		void readDetails(Part *part) const;
		std::optional<PartFile> file(qint64 file_id) const;
		static QString errorText(const QString &driver_text);
		static QString tooBigText(const QString &file_name);
		static QString missingPartText(const QString &driver_text);

		QString sql(const QString &statement) const;
		qint64 insertedId(QSqlQuery &query) const;

		QString m_connection_name;
		QSqlDatabase m_db;
		bool m_server = false;
		QString m_server_url;
};

#endif // PARTSDATABASE_H
