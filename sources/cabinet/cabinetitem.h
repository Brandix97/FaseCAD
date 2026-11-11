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
#ifndef CABINETITEM_H
#define CABINETITEM_H

#include "../qetgraphicsitem/qetgraphicsitem.h"

#include <QList>
#include <QPainterPath>
#include <QSet>
#include <QUuid>
#include <QVector>

class QDomDocument;
class QDomElement;

/**
	@brief The CabinetItem class
	A part of the construction of a cabinet, drawn on a cabinet folio: a
	mounting plate, a DIN rail, a wire duct, or the drawing of a device
	read from a DXF file (DxfReader).

	Its measures are kept in millimetres, as the part is in the cabinet,
	and it is drawn at the scale of its folio (Diagram::pixelsPerMm()):
	changing that scale redraws it, its measures stay.
*/
class CabinetItem : public QetGraphicsItem
{
	Q_OBJECT

	public:
		enum { Type = UserType + 1016 };
		int type() const override { return Type; }

		enum class Kind { MountingPlate, DinRail, WireDuct, Drawing };

		/// What the part is, in millimetres
		struct Properties {
			Kind kind = Kind::MountingPlate;
				/// Along the part; the width of a mounting plate
			qreal length = 100;
				/// Across the part; the height of a mounting plate
			qreal width = 35;
				/// How far it stands off the plate: the height of a wire
				/// duct, the depth of a rail. 0 if not known.
			qreal depth = 0;
				/// The way the length runs. A plate is always horizontal.
			Qt::Orientation orientation = Qt::Horizontal;
				/// The profile of a rail (TH35-7.5...), free otherwise
			QString profile;
				/// The lines of a drawing, in millimetres, its top left
				/// corner at 0; its size is length x width
			QPainterPath drawing;
				/// The name of the file a drawing was read from
			QString source;

			bool operator==(const Properties &other) const;
			bool operator!=(const Properties &other) const {return !(*this == other);}
		};

		/// A profile of DIN rail and its measures in millimetres
		struct RailProfile {
			QString name;
			qreal width;
			qreal depth;
		};
		static QList<RailProfile> railProfiles();
		static RailProfile railProfile(const QString &name);
		static QString defaultRailProfile();

		explicit CabinetItem(QGraphicsItem *parent = nullptr);
		explicit CabinetItem(const Properties &properties,
							 QGraphicsItem *parent = nullptr);
		~CabinetItem() override;

		Properties properties() const {return m_properties;}
		void setProperties(const Properties &properties);
		Kind kind() const {return m_properties.kind;}
			/// A part drawn from one end to the other: a rail, a duct
		bool isLinear() const {return isLinear(m_properties.kind);}
		static bool isLinear(Kind kind)
		{return kind == Kind::DinRail || kind == Kind::WireDuct;}

		QSizeF sizeMm() const;
		QRectF rect() const;
		QRectF sceneRect() const;
		qreal pixelsPerMm() const;
		void scaleChanged(qreal ratio);

		QUuid uuid() const {return m_uuid;}
		void setUuid(const QUuid &uuid) {m_uuid = uuid; emit uuidChanged();}
		void newUuid() {setUuid(QUuid::createUuid());}

		QDomElement toXml(QDomDocument &document) const;
		bool fromXml(const QDomElement &element);
		static QString kindToString(Kind kind);
		static Kind kindFromString(const QString &text, bool *ok = nullptr);

		QRectF boundingRect() const override;
		QPainterPath shape() const override;
		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
				   QWidget *widget = nullptr) override;

		void editProperty() override;
		QString name() const override;
		QString description() const;

		using QetGraphicsItem::setPos;
		void setPos(const QPointF &p) override;
		void placeAt(const QPointF &p);

	signals:
		void propertiesChanged();
		void uuidChanged();

	protected:
		QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
		void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

	private:
		void paintPlate(QPainter *painter, const QRectF &rect) const;
		void paintRail(QPainter *painter, const QRectF &rect) const;
		void paintDuct(QPainter *painter, const QRectF &rect) const;
		void paintDrawing(QPainter *painter) const;
		QPainterPath hiddenByDrawings() const;
		void paintCaption(QPainter *painter, const QRectF &rect) const;
		QFont captionFont(const QFont &base) const;
		qreal captionWidth(const QFont &base, qreal lod) const;

		Properties m_properties;
		QUuid m_uuid;
};

#endif // CABINETITEM_H
