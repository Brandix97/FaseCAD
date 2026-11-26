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
#ifndef PARTTHUMBNAILS_H
#define PARTTHUMBNAILS_H

#include <QHash>
#include <QImage>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QStyledItemDelegate>
#include <QThread>

#include <memory>

class PartsDatabase;
class PartThumbnailReader;

/**
	@brief The pictures of the parts in the table of the parts manager
	(Brandon, 2026-11-21): the first image of a part, made small.

	A part image may weigh megabytes and a database holds a thousand of
	them: a picture is read only when its row is drawn, in a thread of its
	own, and kept small in the cache folder of the computer, so the next
	time it comes at once. A picture of a file is never changed: a new
	image of a part is a new file.
*/
class PartThumbnails : public QObject
{
	Q_OBJECT

	public:
		explicit PartThumbnails(QObject *parent = nullptr);
		~PartThumbnails() override;

			/// Side of the pictures kept, in pixels
		static int side() {return 96;}
		void setDatabase(const QString &path);
			/// The picture of the image file @p file_id; null while it is
			/// read, ready() telling when it is there
		QPixmap picture(qint64 file_id);
			/// True once the picture of @p file_id was read, even when the
			/// file was no image
		bool isRead(qint64 file_id) const;
			/// What a part without image shows: "No Picture"
		static QPixmap noPicture(const QColor &frame, const QColor &text);
			/// The image @p data made a picture no larger than @p side
		static QImage scaled(const QByteArray &data, int side);

	signals:
		void ready(qint64 file_id);
		void read(const QString &database, qint64 file_id);

	private:
		QString m_database;
		QHash<qint64, QPixmap> m_pictures;
		QSet<qint64> m_asked;
		QThread m_thread;
		PartThumbnailReader *m_reader = nullptr;
};

/**
	@brief Reads the pictures in the thread of PartThumbnails
*/
class PartThumbnailReader : public QObject
{
	Q_OBJECT

	public:
		PartThumbnailReader();
		~PartThumbnailReader() override;
		void read(const QString &database, qint64 file_id);

	signals:
		void done(qint64 file_id, const QImage &image);

	private:
			/// A connection of this thread: one is never shared by two
		std::unique_ptr<PartsDatabase> m_db;
		QString m_path;
};

/**
	@brief Draws the picture of a part in its cell, centred, asking for it
	when it is not there yet
*/
class PartPictureDelegate : public QStyledItemDelegate
{
	public:
			/// Role of a cell holding the id of the image file shown, 0
			/// for a part without image
		static int fileRole() {return Qt::UserRole + 5;}
		PartPictureDelegate(PartThumbnails *thumbnails, QObject *parent = nullptr);
		void paint(QPainter *painter, const QStyleOptionViewItem &option,
				   const QModelIndex &index) const override;
		QSize sizeHint(const QStyleOptionViewItem &option,
					   const QModelIndex &index) const override;

	private:
		PartThumbnails *m_thumbnails;
		mutable QPixmap m_no_picture;
};

#endif // PARTTHUMBNAILS_H
