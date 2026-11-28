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
#ifndef PARTCATEGORIES_H
#define PARTCATEGORIES_H

#include <QCoreApplication>
#include <QDialog>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QList>
#include <QString>

class PartsDatabase;
class QListWidget;
class QPushButton;
class QTreeWidget;

/**
	@brief The icons of the categories of parts (Brandon, 2026-11-21): an
	icon is an image file kept in the parts database, SVG or PNG; FaseCAD
	brings a set of them to choose from.
*/
namespace PartCategoryIcons
{
	struct Builtin
	{
		QString id;
		QString name;
	};

		/// The icon an image file makes, a null icon when it is none
	QIcon icon(const QByteArray &data);
		/// The same, @p side points at most, sharp on a screen of any
		/// scale: the folders of the parts collection
	QIcon smallIcon(const QByteArray &data, int side = 16);
		/// @p image as an icon of @p side points at most: a picture for
		/// each scale of screen, so a view never shows it larger (a
		/// single picture counts its pixels as points)
	QIcon sized(const QImage &image, int side);
		/// The icons FaseCAD brings, with the name the user reads
	QList<Builtin> builtins();
		/// The file of the icon @p id FaseCAD brings, empty when unknown
	QByteArray builtinData(const QString &id);
}

/**
	@brief The PartCategoriesDialog class is the window where the user adds,
	renames and removes the categories of the parts, and gives each its
	icon (Brandon, 2026-11-21). Every change is written to the database at
	once; a category removed leaves its parts without category, a category
	renamed takes its parts along.
*/
class PartCategoriesDialog : public QDialog
{
	Q_OBJECT

	public:
		PartCategoriesDialog(PartsDatabase &database, QWidget *parent = nullptr);

			/// True when something was written to the database
		bool hasChanged() const {return m_changed;}
			/// The categories renamed or moved (old path, new path) and
			/// removed (old path, the one its parts went to), for the
			/// caller to follow them
		QHash<QString, QString> renamed() const {return m_renamed;}

			/// @p name a path ("Relays › Interface") makes a subcategory
		void addCategory(const QString &name);
		void renameCategory(const QString &name, const QString &new_name);
		void removeCategory(const QString &name);
		void setIcon(const QString &name, const QByteArray &icon);
		QString current() const;

	private:
		void reload(const QString &select = QString());
		void add();
		void addSubcategory();
		void rename();
		void remove();
		void chooseBuiltin();
		void chooseFile();
		void follow(const QHash<QString, QString> &moved);
		bool warn(bool ok, const QString &error);

		PartsDatabase &m_db;
		QTreeWidget *m_tree = nullptr;
		QList<QPushButton *> m_on_selection;
		void updateButtons();
		QHash<QString, QString> m_renamed;
		bool m_changed = false;
};

/**
	@brief The window where the user picks one of the icons FaseCAD brings.
*/
class PartCategoryIconPicker : public QDialog
{
	Q_OBJECT

	public:
		PartCategoryIconPicker(QWidget *parent = nullptr);
			/// The id of the icon picked, empty when none
		QString picked() const;

	private:
		QListWidget *m_list = nullptr;
};

#endif // PARTCATEGORIES_H
