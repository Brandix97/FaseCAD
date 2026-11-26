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
#include "partthumbnails.h"

#include "partsdatabase.h"

#include <QApplication>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QImageReader>
#include <QPainter>
#include <QStandardPaths>

PartThumbnails::PartThumbnails(QObject *parent) :
	QObject(parent)
{
	m_reader = new PartThumbnailReader();
	m_reader->moveToThread(&m_thread);
	connect(&m_thread, &QThread::finished, m_reader, &QObject::deleteLater);
	connect(this, &PartThumbnails::read, m_reader, &PartThumbnailReader::read);
	connect(m_reader, &PartThumbnailReader::done, this,
			[this](qint64 file_id, const QImage &image) {
			//A file which is no image: "No Picture", never asked again
		m_pictures.insert(file_id, image.isNull() ? QPixmap() : QPixmap::fromImage(image));
		emit ready(file_id);
	});
	m_thread.start(QThread::LowPriority);
}

PartThumbnails::~PartThumbnails()
{
	m_thread.quit();
	m_thread.wait();
}

void PartThumbnails::setDatabase(const QString &path)
{
	if (path != m_database) {
		m_database = path;
		m_pictures.clear();
		m_asked.clear();
	}
}

QPixmap PartThumbnails::picture(qint64 file_id)
{
	const auto it = m_pictures.constFind(file_id);
	if (it != m_pictures.constEnd()) {
		return it.value();
	}
	if (!m_asked.contains(file_id) && !m_database.isEmpty()) {
		m_asked.insert(file_id);
		emit read(m_database, file_id);
	}
	return QPixmap();
}

bool PartThumbnails::isRead(qint64 file_id) const
{
	return m_pictures.contains(file_id);
}

QImage PartThumbnails::scaled(const QByteArray &data, int side)
{
	QBuffer buffer;
	buffer.setData(data);
	if (!buffer.open(QIODevice::ReadOnly)) {
		return QImage();
	}
	QImageReader reader(&buffer);
	reader.setAutoTransform(true);
		//A photo of megabytes read small at once (a JPEG is decoded at
		//the size asked), then made smooth
	const QSize full = reader.size();
	if (full.isValid() && (full.width() > 2 * side || full.height() > 2 * side)) {
		reader.setScaledSize(full.scaled(2 * side, 2 * side, Qt::KeepAspectRatio));
	}
	QImage image = reader.read();
	if (!image.isNull() && (image.width() > side || image.height() > side)) {
		image = image.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	}
	return image;
}

QPixmap PartThumbnails::noPicture(const QColor &frame, const QColor &text)
{
	const int s = side();
	QPixmap pixmap(s, s);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setRenderHint(QPainter::TextAntialiasing, true);
	QPen pen(frame, 3, Qt::DashLine);
	painter.setPen(pen);
	painter.setBrush(Qt::NoBrush);
	painter.drawRoundedRect(QRectF(4, 4, s - 8, s - 8), 10, 10);
	QFont font = QApplication::font();
	font.setPixelSize(s / 5);
	font.setBold(true);
	painter.setFont(font);
	painter.setPen(text);
	painter.drawText(QRect(8, 8, s - 16, s - 16), Qt::AlignCenter | Qt::TextWordWrap,
					 QCoreApplication::translate("PartThumbnails", "No Picture"));
	return pixmap;
}

PartThumbnailReader::PartThumbnailReader() = default;
PartThumbnailReader::~PartThumbnailReader() = default;

/**
	@brief PartThumbnailReader::read
	In the thread of the pictures: the picture of the image file @p file_id
	of the database @p database, from the cache folder or made there
*/
void PartThumbnailReader::read(const QString &database, qint64 file_id)
{
	if (database != m_path || !m_db) {
		m_path = database;
		m_db = std::make_unique<PartsDatabase>();
		if (!m_db->open(database)) {
			m_db.reset();
			emit done(file_id, QImage());
			return;
		}
	}
	const PartFile file = m_db->fileOf(file_id);
	if (!file.id) {
		emit done(file_id, QImage());
		return;
	}
		//The same file of the same database, at the same size
	const QByteArray key = QCryptographicHash::hash(
				QStringLiteral("%1|%2|%3|%4|%5").arg(database).arg(file_id)
				.arg(file.size).arg(file.file_name).arg(PartThumbnails::side()).toUtf8(),
				QCryptographicHash::Sha1).toHex();
	const QString folder = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
			+ QStringLiteral("/part-pictures");
	const QString cached = folder + QLatin1Char('/') + QString::fromLatin1(key)
			+ QStringLiteral(".png");
	QImage image;
	if (image.load(cached, "PNG")) {
		emit done(file_id, image);
		return;
	}
	image = PartThumbnails::scaled(m_db->fileContent(file_id), PartThumbnails::side());
	if (!image.isNull() && QDir().mkpath(folder)) {
		image.save(cached, "PNG");
	}
	emit done(file_id, image);
}

PartPictureDelegate::PartPictureDelegate(PartThumbnails *thumbnails, QObject *parent) :
	QStyledItemDelegate(parent),
	m_thumbnails(thumbnails)
{}

void PartPictureDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
								const QModelIndex &index) const
{
		//The cell as the others, selected or not, without text
	QStyleOptionViewItem cell(option);
	initStyleOption(&cell, index);
	cell.text.clear();
	cell.icon = QIcon();
	cell.features &= ~QStyleOptionViewItem::HasDecoration;
	const QWidget *widget = option.widget;
	QStyle *style = widget ? widget->style() : QApplication::style();
	style->drawControl(QStyle::CE_ItemViewItem, &cell, painter, widget);

	const qint64 file_id = index.data(fileRole()).toLongLong();
	QPixmap pixmap;
	if (file_id) {
		pixmap = m_thumbnails->picture(file_id);
			//Coming: drawn when it is there
		if (pixmap.isNull() && !m_thumbnails->isRead(file_id)) {
			return;
		}
	}
	if (pixmap.isNull()) {
		if (m_no_picture.isNull()) {
			m_no_picture = PartThumbnails::noPicture(option.palette.color(QPalette::Mid),
													 option.palette.color(QPalette::Text));
		}
		pixmap = m_no_picture;
	}
	const QRect area = option.rect.adjusted(3, 3, -3, -3);
	QSize size = pixmap.size();
	size.scale(area.size(), Qt::KeepAspectRatio);
	const QRect target(QPoint(area.x() + (area.width() - size.width()) / 2,
							  area.y() + (area.height() - size.height()) / 2), size);
	painter->save();
	painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
	painter->drawPixmap(target, pixmap);
	painter->restore();
}

QSize PartPictureDelegate::sizeHint(const QStyleOptionViewItem &option,
									const QModelIndex &index) const
{
	Q_UNUSED(option)
	Q_UNUSED(index)
	return QSize(64, 56);
}
