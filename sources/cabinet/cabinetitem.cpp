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
#include "cabinetitem.h"

#include "../dataBase/projectdatabase.h"
#include "../diagram.h"
#include "../qetproject.h"
#include "cabinetcommands.h"
#include "cabinetdialogs.h"
#include "cabinetsnap.h"
#include "dxfreader.h"

#include <QApplication>
#include <QDomDocument>
#include <QDomElement>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPainter>
#include <QPainterPath>
#include <QSqlQuery>
#include <QStyleOptionGraphicsItem>

#include <cmath>

namespace {

	/// The stacking of the parts of a cabinet: the plate under what is
	/// mounted on it
qreal defaultZValue(CabinetItem::Kind kind)
{
	switch (kind) {
		case CabinetItem::Kind::MountingPlate: return 1;
		case CabinetItem::Kind::WireDuct:      return 5;
		case CabinetItem::Kind::DinRail:       return 6;
		case CabinetItem::Kind::Drawing:       return 8;
	}
	return 5;
}

	/// @p value without the zeros a whole number of millimetres ends with
QString mm(qreal value)
{
	return QString::number(std::round(value * 10.0) / 10.0, 'g', 8);
}

	/// How many pixels of the screen one pixel of the folio takes
qreal levelOfDetail(const QPainter *painter, const QStyleOptionGraphicsItem *option)
{
	Q_UNUSED(option)
	return QStyleOptionGraphicsItem::levelOfDetailFromTransform(painter->worldTransform());
}

} // namespace

bool CabinetItem::Properties::operator==(const Properties &other) const
{
	return kind == other.kind
			&& qFuzzyCompare(length + 1.0, other.length + 1.0)
			&& qFuzzyCompare(width + 1.0, other.width + 1.0)
			&& qFuzzyCompare(depth + 1.0, other.depth + 1.0)
			&& orientation == other.orientation
			&& profile == other.profile
			&& source == other.source
			&& device == other.device
			&& device_label == other.device_label
			&& drawing == other.drawing;
}

/**
	@brief CabinetItem::railProfiles
	@return the profiles of DIN rail a rail can be drawn as, the usual
	one first
*/
QList<CabinetItem::RailProfile> CabinetItem::railProfiles()
{
	return {
		{QStringLiteral("TH35-7.5"), 35.0, 7.5},
		{QStringLiteral("TH35-15"),  35.0, 15.0},
		{QStringLiteral("TH15-5.5"), 15.0, 5.5},
		{QStringLiteral("G32"),      32.0, 15.0}};
}

/**
	@brief CabinetItem::railProfile
	@return the profile named @p name, or the usual one if there is none
*/
CabinetItem::RailProfile CabinetItem::railProfile(const QString &name)
{
	const QList<RailProfile> profiles = railProfiles();
	for (const RailProfile &profile : profiles) {
		if (profile.name == name) return profile;
	}
	return profiles.first();
}

QString CabinetItem::defaultRailProfile()
{
	return railProfiles().first().name;
}

CabinetItem::CabinetItem(QGraphicsItem *parent) :
	CabinetItem(Properties(), parent)
{}

CabinetItem::CabinetItem(const Properties &properties, QGraphicsItem *parent) :
	QetGraphicsItem(parent),
	m_properties(properties),
	m_uuid(QUuid::createUuid())
{
	setFlags(ItemIsSelectable | ItemIsMovable);
	setAcceptHoverEvents(true);
	setZValue(defaultZValue(m_properties.kind));
}

CabinetItem::~CabinetItem()
{}

/**
	@brief CabinetItem::setProperties
	Make this part what @p properties say. Its top left corner stays.
*/
void CabinetItem::setProperties(const Properties &properties)
{
	if (m_properties == properties) return;
	prepareGeometryChange();
	m_properties = properties;
	watchDevice();
	emit propertiesChanged();
	update();
}

/**
	@brief CabinetItem::followDevice
	Write on a device's drawing the label its symbol has now: renaming the
	device on the schematic renames its drawing. The label is read from
	the project database, so the folio of the symbol need not be built. A
	symbol gone, or without a label, leaves the drawing as it is.
*/
void CabinetItem::followDevice()
{
	if (m_properties.kind != Kind::Drawing || m_properties.device.isNull()) return;
	const Diagram *folio = diagram();
	if (!folio || !folio->project()) return;
		//newQuery() runs the statement at once: the uuid is written in it
		//(hexadecimal digits, hyphens and braces only)
	QSqlQuery query = folio->project()->dataBase()->newQuery(QStringLiteral(
		"SELECT label FROM element_info WHERE element_uuid = '%1'")
		.arg(m_properties.device.toString()));
	if (!query.isActive() || !query.next()) return;
	const QString label = query.value(0).toString().trimmed();
	if (label.isEmpty() || label == m_properties.device_label) return;
		//Not a change of its own, as the cross references of a contact:
		//undoing the renaming renames the drawing back the same way
	prepareGeometryChange();
	m_properties.device_label = label;
	emit propertiesChanged();
	update();
}

/**
	@brief CabinetItem::watchDevice
	Follow the labels of the project while this part is a device's drawing
	on a folio, and take the label its device has now.
*/
void CabinetItem::watchDevice()
{
	disconnect(m_device_connection);
	m_device_connection = QMetaObject::Connection();
	if (m_properties.kind != Kind::Drawing || m_properties.device.isNull()) return;
	const Diagram *folio = diagram();
	if (!folio || !folio->project()) return;
	m_device_connection = connect(folio->project()->dataBase(),
								  &projectDataBase::dataBaseUpdated,
								  this, &CabinetItem::followDevice);
	followDevice();
}

/**
	@brief CabinetItem::sizeMm
	@return the width and the height of this part as it lies on its
	folio, in millimetres
*/
QSizeF CabinetItem::sizeMm() const
{
	if (m_properties.orientation == Qt::Vertical && isLinear())
		return QSizeF(m_properties.width, m_properties.length);
	return QSizeF(m_properties.length, m_properties.width);
}

/**
	@brief CabinetItem::pixelsPerMm
	@return the pixels one millimetre of this part takes: those of its
	folio, or of a cabinet folio at the default scale while it is on none
*/
qreal CabinetItem::pixelsPerMm() const
{
	if (const Diagram *folio = diagram())
		return folio->pixelsPerMm();
	return Diagram::pixelsPerPaperMm / Diagram::defaultCabinetScale;
}

/**
	@brief CabinetItem::rect
	@return the outline of this part, in its own coordinates: its top
	left corner is its position
*/
QRectF CabinetItem::rect() const
{
	const QSizeF size = sizeMm() * pixelsPerMm();
	return QRectF(QPointF(0, 0), size);
}

QRectF CabinetItem::sceneRect() const
{
	return rect().translated(pos());
}

/**
	@brief CabinetItem::scaleChanged
	The scale of the folio changed: one millimetre takes @p ratio times
	the pixels it took. This part stays where it is in the cabinet.
*/
void CabinetItem::scaleChanged(qreal ratio)
{
	prepareGeometryChange();
	QGraphicsObject::setPos(pos() * ratio);
	update();
}

QString CabinetItem::kindToString(Kind kind)
{
	switch (kind) {
		case Kind::MountingPlate: return QStringLiteral("mounting_plate");
		case Kind::DinRail:       return QStringLiteral("din_rail");
		case Kind::WireDuct:      return QStringLiteral("wire_duct");
		case Kind::Drawing:       return QStringLiteral("dxf_drawing");
	}
	return QStringLiteral("mounting_plate");
}

CabinetItem::Kind CabinetItem::kindFromString(const QString &text, bool *ok)
{
	if (ok) *ok = true;
	if (text == QLatin1String("mounting_plate")) return Kind::MountingPlate;
	if (text == QLatin1String("din_rail"))       return Kind::DinRail;
	if (text == QLatin1String("wire_duct"))      return Kind::WireDuct;
	if (text == QLatin1String("dxf_drawing"))    return Kind::Drawing;
	if (ok) *ok = false;
	return Kind::MountingPlate;
}

/**
	@brief CabinetItem::toXml
	@return this part as a <cabinet_item> of @p document: its measures
	in millimetres, its position in the pixels of its folio as every
	item of a folio
*/
QDomElement CabinetItem::toXml(QDomDocument &document) const
{
	QDomElement xml = document.createElement(QStringLiteral("cabinet_item"));
	xml.setAttribute(QStringLiteral("uuid"), m_uuid.toString());
	xml.setAttribute(QStringLiteral("kind"), kindToString(m_properties.kind));
	xml.setAttribute(QStringLiteral("x"), QString::number(pos().x(), 'f', 3));
	xml.setAttribute(QStringLiteral("y"), QString::number(pos().y(), 'f', 3));
	xml.setAttribute(QStringLiteral("z"), QString::number(zValue()));
	xml.setAttribute(QStringLiteral("length"), QString::number(m_properties.length, 'f', 2));
	xml.setAttribute(QStringLiteral("width"), QString::number(m_properties.width, 'f', 2));
	if (m_properties.depth > 0)
		xml.setAttribute(QStringLiteral("depth"), QString::number(m_properties.depth, 'f', 2));
	xml.setAttribute(QStringLiteral("orientation"),
					 m_properties.orientation == Qt::Vertical
					 ? QStringLiteral("vertical") : QStringLiteral("horizontal"));
	if (!m_properties.profile.isEmpty())
		xml.setAttribute(QStringLiteral("profile"), m_properties.profile);
	if (m_properties.kind == Kind::Drawing) {
			//The lines themselves: the project does not need the DXF file
		if (!m_properties.source.isEmpty())
			xml.setAttribute(QStringLiteral("source"), m_properties.source);
			//The device it stands for, written only for a device's drawing
		if (!m_properties.device.isNull())
			xml.setAttribute(QStringLiteral("device"), m_properties.device.toString());
		if (!m_properties.device_label.isEmpty())
			xml.setAttribute(QStringLiteral("device_label"), m_properties.device_label);
		QDomElement path = document.createElement(QStringLiteral("path"));
		path.setAttribute(QStringLiteral("d"), DxfReader::pathToString(m_properties.drawing));
		xml.appendChild(path);
	}
	return xml;
}

/**
	@brief CabinetItem::fromXml
	Read this part from @p element, a <cabinet_item>.
	@return false if @p element is not one or names a part this program
	does not know: the part is then left as it was
*/
bool CabinetItem::fromXml(const QDomElement &element)
{
	if (element.tagName() != QLatin1String("cabinet_item")) return false;
	bool known = false;
	Properties properties;
	properties.kind = kindFromString(element.attribute(QStringLiteral("kind")), &known);
	if (!known) return false;
	properties.length = qMax(0.0, element.attribute(QStringLiteral("length")).toDouble());
	properties.width = qMax(0.0, element.attribute(QStringLiteral("width")).toDouble());
	properties.depth = qMax(0.0, element.attribute(QStringLiteral("depth")).toDouble());
	properties.orientation =
			element.attribute(QStringLiteral("orientation")) == QLatin1String("vertical")
			? Qt::Vertical : Qt::Horizontal;
	properties.profile = element.attribute(QStringLiteral("profile"));
	if (properties.kind == Kind::Drawing) {
		bool path_read = false;
		properties.source = element.attribute(QStringLiteral("source"));
		properties.device = QUuid(element.attribute(QStringLiteral("device")));
		properties.device_label = element.attribute(QStringLiteral("device_label"));
		properties.drawing = DxfReader::pathFromString(
					element.firstChildElement(QStringLiteral("path"))
					.attribute(QStringLiteral("d")), &path_read);
		if (!path_read) return false;
	}

	prepareGeometryChange();
	m_properties = properties;
	watchDevice();
	const QUuid uuid(element.attribute(QStringLiteral("uuid")));
	if (!uuid.isNull()) m_uuid = uuid;
	QGraphicsObject::setPos(element.attribute(QStringLiteral("x")).toDouble(),
							element.attribute(QStringLiteral("y")).toDouble());
	bool z_read = false;
	const qreal z = element.attribute(QStringLiteral("z")).toDouble(&z_read);
	setZValue(z_read ? z : defaultZValue(properties.kind));
	emit propertiesChanged();
	update();
	return true;
}

QRectF CabinetItem::boundingRect() const
{
	return rect().adjusted(-2, -2, 2, 2);
}

QPainterPath CabinetItem::shape() const
{
	QPainterPath path;
	path.addRect(rect());
	return path;
}

void CabinetItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
						QWidget *widget)
{
	Q_UNUSED(widget)
	painter->save();
	painter->setRenderHint(QPainter::Antialiasing, true);
	const QRectF outline = rect();
		//A rail is not drawn where a device stands on it: the lines of
		//the device are seen alone, without the slots and edges of the
		//rail behind them
	const QPainterPath hidden = m_properties.kind == Kind::DinRail
			? hiddenByDrawings() : QPainterPath();
	if (!hidden.isEmpty()) {
		QPainterPath visible;
		visible.addRect(boundingRect());
		painter->setClipPath(visible.subtracted(hidden), Qt::IntersectClip);
	}
	switch (m_properties.kind) {
		case Kind::MountingPlate: paintPlate(painter, outline); break;
		case Kind::DinRail:       paintRail(painter, outline);  break;
		case Kind::WireDuct:      paintDuct(painter, outline);  break;
		case Kind::Drawing:       paintDrawing(painter);        break;
	}
	if (m_properties.kind == Kind::Drawing && !m_properties.device_label.isEmpty())
		paintDeviceLabel(painter);
	if (captionWidth(painter->font(), levelOfDetail(painter, option)) > 0)
		paintCaption(painter, outline);
	if (!hidden.isEmpty()) painter->setClipping(false);

	if (isSelected()) {
		QPen pen(QColor(0, 120, 215), 1.5, Qt::DashLine);
		pen.setCosmetic(true);
		painter->setPen(pen);
		painter->setBrush(Qt::NoBrush);
		painter->drawRect(outline);
	}
	painter->restore();
}

/**
	@brief CabinetItem::paintPlate
	A mounting plate: a sheet of metal, with its border
*/
void CabinetItem::paintPlate(QPainter *painter, const QRectF &rect) const
{
	painter->setPen(QPen(QColor(90, 98, 110), 1.0));
	painter->setBrush(QColor(190, 196, 204, 70));
	painter->drawRect(rect);
}

/**
	@brief CabinetItem::hiddenByDrawings
	@return the part of this item the DXF drawings over it cover, in its
	own coordinates: empty if none does
*/
QPainterPath CabinetItem::hiddenByDrawings() const
{
	QPainterPath hidden;
	if (!scene()) return hidden;
	const QRectF mine = sceneRect();
	const QList<QGraphicsItem *> items = scene()->items(mine, Qt::IntersectsItemBoundingRect);
	for (QGraphicsItem *item : items) {
		const auto *part = qgraphicsitem_cast<const CabinetItem *>(item);
		if (!part || part == this || part->kind() != Kind::Drawing) continue;
		if (!part->isVisible()) continue;
		const QRectF covered = part->sceneRect().intersected(mine);
		if (covered.isEmpty()) continue;
		hidden.addRect(covered.translated(-pos()));
	}
	return hidden.simplified();
}

/**
	@brief CabinetItem::paintDrawing
	The lines of a device read from a DXF file, at the scale of the folio
*/
void CabinetItem::paintDrawing(QPainter *painter) const
{
	const qreal pixels = pixelsPerMm();
	painter->save();
	painter->scale(pixels, pixels);
		//About a third of a millimetre of sheet, whatever the scale
	QPen pen(QColor(30, 34, 40), 0.7 / pixels);
	pen.setCapStyle(Qt::RoundCap);
	pen.setJoinStyle(Qt::RoundJoin);
	painter->setPen(pen);
	painter->setBrush(Qt::NoBrush);
	painter->drawPath(m_properties.drawing);
	painter->restore();
}

/**
	@brief CabinetItem::deviceLabelFont
	@return the font the label of a device's drawing is written in, from
	@p base: about a fifth of the height of the drawing, small enough for
	the label to fit inside its width
*/
QFont CabinetItem::deviceLabelFont(const QFont &base) const
{
	const QRectF outline = rect();
	QFont font = base;
	font.setBold(true);
	font.setHintingPreference(QFont::PreferNoHinting);
	qreal size = qBound(3.0, outline.height() * 0.2, 14.0);
	font.setPixelSize(qMax(1, int(std::round(size))));
	const qreal advance = QFontMetricsF(font).horizontalAdvance(m_properties.device_label);
	const qreal room = outline.width() * 0.9;
	if (advance > room && advance > 0) {
		size = qMax(2.0, size * room / advance);
		font.setPixelSize(qMax(1, int(std::floor(size))));
	}
	return font;
}

/**
	@brief CabinetItem::deviceLabelRect
	@return the box the label of a device's drawing takes, in the middle of
	the drawing, in its own coordinates; null if it has no label
*/
QRectF CabinetItem::deviceLabelRect(const QFont &base) const
{
	if (m_properties.kind != Kind::Drawing || m_properties.device_label.isEmpty())
		return QRectF();
	const QFont font = deviceLabelFont(base);
	const QFontMetricsF metrics(font);
	const qreal margin = qMax(1.0, font.pixelSize() * 0.2);
	QRectF box(0, 0,
			   metrics.horizontalAdvance(m_properties.device_label) + 2 * margin,
			   metrics.height() + margin);
	box.moveCenter(rect().center());
	return box;
}

/**
	@brief CabinetItem::paintDeviceLabel
	The label of the device a drawing stands for, in its middle, on a light
	box so the lines under it do not hide it
*/
void CabinetItem::paintDeviceLabel(QPainter *painter) const
{
	const QFont font = deviceLabelFont(painter->font());
	const QRectF box = deviceLabelRect(painter->font());
	if (box.isNull()) return;
	const QFontMetricsF metrics(font);
	painter->save();
	painter->setPen(Qt::NoPen);
	painter->setBrush(QColor(255, 255, 255, 225));
	const qreal radius = box.height() * 0.2;
	painter->drawRoundedRect(box, radius, radius);
		//Written as outlines, as the captions: sharp at any zoom
	QPainterPath letters;
	letters.addText(QPointF(box.center().x()
							- metrics.horizontalAdvance(m_properties.device_label) / 2.0,
							box.center().y() - metrics.height() / 2.0 + metrics.ascent()),
					font, m_properties.device_label);
	painter->setBrush(QColor(0, 84, 166));
	painter->drawPath(letters);
	painter->restore();
}

/**
	@brief CabinetItem::paintRail
	A DIN rail seen from the front: its two edges, and the slots of a
	perforated rail every 25 mm
*/
void CabinetItem::paintRail(QPainter *painter, const QRectF &rect) const
{
	const qreal pixels = pixelsPerMm();
	const bool vertical = m_properties.orientation == Qt::Vertical;
	painter->setPen(QPen(QColor(70, 76, 86), 0.8));
	painter->setBrush(QColor(205, 210, 216));
	painter->drawRect(rect);

		//Drawn along x, turned for a vertical rail
	painter->save();
	if (vertical) {
		painter->translate(rect.right(), rect.top());
		painter->rotate(90);
	}
	const qreal length = m_properties.length * pixels;
	const qreal width = m_properties.width * pixels;
	const qreal flange = width * 0.14;
	painter->setPen(QPen(QColor(110, 116, 126), 0.5));
	painter->drawLine(QPointF(0, flange), QPointF(length, flange));
	painter->drawLine(QPointF(0, width - flange), QPointF(length, width - flange));

	const qreal slot_width = width * 0.18;
	const qreal lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(
				painter->worldTransform());
	if (slot_width * lod >= 1.5 && m_properties.length >= 25.0) {
		const qreal pitch = 25.0 * pixels;
		const qreal slot_length = 15.0 * pixels;
		const int count = int(m_properties.length / 25.0);
		const qreal margin = (length - count * pitch) / 2.0;
			//No slot under the caption, written over the middle of the rail
		const qreal caption = captionWidth(painter->font(), lod);
		const qreal caption_from = (length - caption) / 2.0;
		const qreal caption_to = (length + caption) / 2.0;
		painter->setBrush(QColor(250, 250, 250));
		painter->setPen(QPen(QColor(70, 76, 86), 0.4));
		for (int i = 0 ; i < count ; ++i) {
			const QRectF slot(margin + i * pitch + (pitch - slot_length) / 2.0,
							  (width - slot_width) / 2.0,
							  slot_length, slot_width);
			if (caption > 0 && slot.right() > caption_from && slot.left() < caption_to)
				continue;
			painter->drawRoundedRect(slot, slot_width / 2.0, slot_width / 2.0);
		}
	}
	painter->restore();
}

/**
	@brief CabinetItem::paintDuct
	A slotted wire duct seen from the front: its cover between the two
	rows of fingers
*/
void CabinetItem::paintDuct(QPainter *painter, const QRectF &rect) const
{
	const qreal pixels = pixelsPerMm();
	const bool vertical = m_properties.orientation == Qt::Vertical;
	painter->setPen(QPen(QColor(70, 90, 120), 0.8));
	painter->setBrush(QColor(196, 208, 224));
	painter->drawRect(rect);

	painter->save();
	if (vertical) {
		painter->translate(rect.right(), rect.top());
		painter->rotate(90);
	}
	const qreal length = m_properties.length * pixels;
	const qreal width = m_properties.width * pixels;
	const qreal finger = qMin(width * 0.2, 8.0 * pixels);
	painter->setPen(QPen(QColor(110, 128, 156), 0.5));
	painter->drawLine(QPointF(0, finger), QPointF(length, finger));
	painter->drawLine(QPointF(0, width - finger), QPointF(length, width - finger));

	const qreal pitch = 12.5 * pixels;
	const qreal lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(
				painter->worldTransform());
	if (pitch * lod >= 4.0) {
		for (qreal x = pitch ; x < length - 0.01 ; x += pitch) {
			painter->drawLine(QPointF(x, 0), QPointF(x, finger));
			painter->drawLine(QPointF(x, width - finger), QPointF(x, width));
		}
	}
	painter->restore();
}

/**
	@brief CabinetItem::captionFont
	@return the font the caption of this part is written in, from @p base:
	small enough to stand between the edges of a rail or a duct
*/
QFont CabinetItem::captionFont(const QFont &base) const
{
	const QRectF outline = rect();
	const qreal across = m_properties.kind == Kind::MountingPlate
			? outline.height() : qMin(outline.width(), outline.height());
	const qreal size = m_properties.kind == Kind::MountingPlate
			? 7.0 : qBound(1.0, across * 0.42, 7.0);
	QFont font = base;
	font.setPixelSize(qMax(1, int(std::round(size))));
		//Seen at any zoom but its own size: the outlines as they are
	font.setHintingPreference(QFont::PreferNoHinting);
	return font;
}

/**
	@brief CabinetItem::captionWidth
	@return the room the caption takes along this part, margins included,
	in the pixels of the folio; 0 if it is not written: the part is too
	small for it, or seen from too far (@p lod) to read it
*/
qreal CabinetItem::captionWidth(const QFont &base, qreal lod) const
{
		//A drawing has its own lines: no caption over them
	if (m_properties.kind == Kind::Drawing) return 0;
	const QFont font = captionFont(base);
	if (font.pixelSize() < 3 || font.pixelSize() * lod < 5.0) return 0;
	const QRectF outline = rect();
	const bool vertical = m_properties.orientation == Qt::Vertical && isLinear();
	const qreal along = vertical ? outline.height() : outline.width();
	const qreal across = vertical ? outline.width() : outline.height();
	const QFontMetricsF metrics(font);
	const qreal width = metrics.horizontalAdvance(description()) + 8.0;
	if (width > along || metrics.height() > across) return 0;
	return width;
}

/**
	@brief CabinetItem::paintCaption
	What the part is and its length, written on it: in the top left corner
	of a plate, over the middle of a rail or a duct
*/
void CabinetItem::paintCaption(QPainter *painter, const QRectF &rect) const
{
	const bool plate = m_properties.kind == Kind::MountingPlate;
	const bool vertical = m_properties.orientation == Qt::Vertical && !plate;
	const qreal along = vertical ? rect.height() : rect.width();
	const qreal across = vertical ? rect.width() : rect.height();

	painter->save();
	if (vertical) {
		painter->translate(rect.left(), rect.bottom());
		painter->rotate(-90);
	}
		//Written as outlines: the letters are a few pixels of the folio
		//high and read zoomed in or printed, at any size but their own
	const QFont font = captionFont(painter->font());
	const QFontMetricsF metrics(font);
	const QString text = description();
	const QPointF baseline = plate
			? QPointF(4.0, 3.0 + metrics.ascent())
			: QPointF((along - metrics.horizontalAdvance(text)) / 2.0,
					  (across - metrics.height()) / 2.0 + metrics.ascent());
	QPainterPath letters;
	letters.addText(baseline, font, text);
	painter->setPen(Qt::NoPen);
	painter->setBrush(QColor(40, 48, 60));
	painter->drawPath(letters);
	painter->restore();
}

/**
	@brief CabinetItem::editProperty
	Open the measures of this part; a change goes to the undo stack.
*/
void CabinetItem::editProperty()
{
	if (!diagram() || diagram()->isReadOnly()) return;
		//A drawing is as its file made it
	if (m_properties.kind == Kind::Drawing) return;
	Properties properties = m_properties;
	if (!CabinetDialogs::editItem(properties, QApplication::activeWindow()))
		return;
	if (properties == m_properties) return;
	diagram()->undoStack().push(new ChangeCabinetItemCommand(this, properties));
}

/**
	@brief CabinetItem::name
	@return what this part is, as the undo captions say it ("Add a wire duct")
*/
QString CabinetItem::name() const
{
	switch (m_properties.kind) {
		case Kind::MountingPlate: return tr("a mounting plate");
		case Kind::DinRail:       return tr("a DIN rail");
		case Kind::WireDuct:      return tr("a wire duct");
		case Kind::Drawing:       return tr("a DXF drawing");
	}
	return QString();
}

/**
	@brief CabinetItem::description
	@return the measures of this part in a few words, as written on it:
	"600 × 800 mm" for a plate, "TH35-7.5 · 500 mm" for a rail,
	"40×60 · 500 mm" for a duct
*/
QString CabinetItem::description() const
{
	switch (m_properties.kind) {
		case Kind::MountingPlate:
			return tr("%1 × %2 mm", "width and height of a mounting plate")
					.arg(mm(m_properties.length), mm(m_properties.width));
		case Kind::DinRail:
			return tr("%1 · %2 mm", "profile and length of a DIN rail")
					.arg(m_properties.profile.isEmpty() ? mm(m_properties.width)
														: m_properties.profile,
						 mm(m_properties.length));
		case Kind::WireDuct:
			if (m_properties.depth > 0)
				return tr("%1×%2 · %3 mm", "width, height and length of a wire duct")
						.arg(mm(m_properties.width), mm(m_properties.depth),
							 mm(m_properties.length));
			return tr("%1 · %2 mm", "width and length of a wire duct")
					.arg(mm(m_properties.width), mm(m_properties.length));
		case Kind::Drawing:
			if (!m_properties.device_label.isEmpty())
				return tr("%1 · %2 × %3 mm", "device, width and height of its DXF drawing")
						.arg(m_properties.device_label, mm(m_properties.length),
							 mm(m_properties.width));
			if (m_properties.source.isEmpty())
				return tr("%1 × %2 mm", "width and height of a mounting plate")
						.arg(mm(m_properties.length), mm(m_properties.width));
			return tr("%1 · %2 × %3 mm", "file, width and height of a DXF drawing")
					.arg(m_properties.source, mm(m_properties.length),
						 mm(m_properties.width));
	}
	return QString();
}

/**
	@brief CabinetItem::setPos
	Move this part to @p p, as the mouse drags it: it catches the corners
	and the middles of the sides of the other parts of the cabinet
	(CabinetSnap), and a marker shows which one.
*/
void CabinetItem::setPos(const QPointF &p)
{
	if (!is_movable_) return;
	QSet<const QGraphicsItem *> ignored{this};
	if (scene()) {
			//What moves with this part does not hold it back
		const QList<QGraphicsItem *> selected = scene()->selectedItems();
		for (const QGraphicsItem *item : selected) ignored.insert(item);
	}
	const CabinetSnap::Snap snap = CabinetSnap::topLeft(rect().translated(p), diagram(), ignored);
	CabinetSnap::showMarker(diagram(), snap);
	if (snap.value == pos()) return;
	QGraphicsObject::setPos(snap.value);
}

void CabinetItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	CabinetSnap::hideMarker(diagram());
	QetGraphicsItem::mouseReleaseEvent(event);
}

/**
	@brief CabinetItem::placeAt
	Put the top left corner of this part at @p p exactly, whatever the grid.
*/
void CabinetItem::placeAt(const QPointF &p)
{
	QGraphicsObject::setPos(p);
}

QVariant CabinetItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
	if (change == ItemSelectedHasChanged) update();
		//On a folio of a project, a device's drawing follows its label
	if (change == ItemSceneHasChanged) watchDevice();
	return QetGraphicsItem::itemChange(change, value);
}
