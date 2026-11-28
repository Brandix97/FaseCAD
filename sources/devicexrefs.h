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
#ifndef DEVICEXREFS_H
#define DEVICEXREFS_H

#include "properties/devicexrefsettings.h"

#include <QGraphicsObject>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QUuid>

class Element;
class QETProject;
class QTimer;

/**
	@brief The DeviceXRefs namespace makes the cross references between
	the parts of one device: the symbols sharing a label that are no
	contact of a coil (a PLC drawn as its supply, its inputs and its
	outputs, a breaker and its auxiliary block...). The main part, the
	one made of parts or else the first, lists where the others are;
	each other one says where the main part is (DeviceXRefSettings).

	The places are read from the project database, so the parts on
	folios not built count; the references are drawn on the built
	folios, again each time the project changes. They are not saved.
*/
namespace DeviceXRefs
{
		/// One part of a device, as the database gives it
	struct Part
	{
		QUuid element;
		QUuid folio;
		int folio_index = 0;	///< 0 for the first folio
		QString folio_label;	///< what the title block calls the folio
		QString cell;			///< e.g. B4
		bool has_parts = false;	///< made of parts (its Parts tab)
	};

		/// The parts of every device of @p project with more than one,
		/// by label, in the order of the folios
	QHash<QString, QList<Part>> devices(QETProject *project);
		/// Which of @p parts is the main one
	int mainOf(const QList<Part> &parts);
		/// @p part written with @p formula (the cross reference format of
		/// the project: %f folio, %l row, %c column, %id folio number)
	QString placeText(const QString &formula, const Part &part);
		/// The places @p element shows: none when it is no part of a
		/// device with others
	QStringList placesFor(const QUuid &element, const QHash<QString, QList<Part>> &devices,
						  const QString &label, const DeviceXRefSettings &settings,
						  const QString &formula, QList<QUuid> *targets = nullptr);

		/// Follow the changes of @p project to draw its references again
	void follow(QETProject *project);
		/// Draw the references of the built folios of @p project again, soon
	void scheduleRefresh(QETProject *project);
		/// Draw them again now
	void refresh(QETProject *project);
}

/**
	@brief The DeviceXRefItem class draws, under the label of a part of a
	device (or below the symbol), the places of the other parts; a double
	click on one shows that part on its folio. A child of its element,
	not one of its saved texts.
*/
class DeviceXRefItem : public QGraphicsObject
{
	Q_OBJECT

	public:
		enum { Type = UserType + 1401 };
		int type() const override { return Type; }

		DeviceXRefItem(Element *element);

		void setPlaces(const QStringList &places, const QList<QUuid> &targets,
					   DeviceXRefSettings::Place place);
		QStringList places() const {return m_places;}

		QRectF boundingRect() const override;
		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
				   QWidget *widget) override;

	protected:
		void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

	private:
		void reposition();
		QFont font() const;

		QPointer<Element> m_element;
		QStringList m_places;
		QList<QUuid> m_targets;
		DeviceXRefSettings::Place m_place = DeviceXRefSettings::Place::UnderLabel;
		QRectF m_rect;
		qreal m_line_height = 10.0;
};

#endif // DEVICEXREFS_H
