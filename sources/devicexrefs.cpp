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
#include "devicexrefs.h"

#include "dataBase/projectdatabase.h"
#include "diagram.h"
#include "partsdb/partslinks.h"
#include "qetapp.h"
#include "qetgraphicsitem/dynamicelementtextitem.h"
#include "qetgraphicsitem/element.h"
#include "qetinformation.h"
#include "qetproject.h"
#include "summarysymbols.h"

#include <QFontMetricsF>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QTimer>
#include <QUndoStack>

#include <algorithm>

namespace {

	/// What follows a project for its references: one for each project
class Follower : public QObject
{
	public:
		explicit Follower(QETProject *project) :
			QObject(project),
			m_project(project)
		{
			setObjectName(QStringLiteral("device_xrefs"));
			m_timer.setSingleShot(true);
			m_timer.setInterval(150);
			connect(&m_timer, &QTimer::timeout, this, [this]() {
				DeviceXRefs::refresh(m_project);
			});
			connect(project->dataBase(), &projectDataBase::dataBaseUpdated,
					this, [this]() {schedule();});
			connect(project, &QETProject::XRefPropertiesChanged, this, [this]() {schedule();});
			connect(project, &QETProject::projectDiagramsOrderChanged, this, [this]() {schedule();});
			connect(project, &QETProject::diagramRemoved, this, [this]() {schedule();});
				//A move or an edit that leaves the database as it was (a
				//free text moved): the function texts may have changed
			if (project->undoStack()) {
				connect(project->undoStack(), &QUndoStack::indexChanged, this, [this]() {schedule();});
			}
		}

		void schedule() {m_timer.start();}

	private:
		QPointer<QETProject> m_project;
		QTimer m_timer;
};

Follower *followerOf(QETProject *project)
{
		//Only this file makes an object of that name
	return project ? static_cast<Follower *>(project->findChild<QObject *>(
												 QStringLiteral("device_xrefs"),
												 Qt::FindDirectChildrenOnly))
				   : nullptr;
}

DeviceXRefItem *itemOf(Element *element)
{
	for (QGraphicsItem *child : element->childItems()) {
		if (child->type() == DeviceXRefItem::Type) {
			return static_cast<DeviceXRefItem *>(child);
		}
	}
	return nullptr;
}

QString formulaOf(QETProject *project)
{
	const QString formula = project->defaultXRefProperties(QStringLiteral("coil")).masterLabel();
	return formula.isEmpty() ? QStringLiteral("%f-%l%c") : formula;
}

	/// The text of @p element showing its label, if it has one
DynamicElementTextItem *labelTextOf(Element *element)
{
	for (DynamicElementTextItem *text : element->dynamicTextItems()) {
		if (text->textFrom() == DynamicElementTextItem::ElementInfo
			&& text->infoName() == QETInformation::ELMT_LABEL) {
			return text;
		}
	}
	return nullptr;
}

}

QHash<QString, QList<DeviceXRefs::Part>> DeviceXRefs::devices(QETProject *project)
{
	QHash<QString, QList<Part>> result;
	if (!project || !project->dataBase()) {
		return result;
	}
	QSqlQuery query = project->dataBase()->newQuery(QStringLiteral(
		"SELECT e.uuid, e.diagram_uuid, d.pos, di.folio, e.pos, ei.label "
		"FROM element e "
		"JOIN diagram d ON d.uuid = e.diagram_uuid "
		"LEFT JOIN diagram_info di ON di.diagram_uuid = d.uuid "
		"LEFT JOIN element_info ei ON ei.element_uuid = e.uuid "
			//A contact is shown by its coil's own cross reference
		"WHERE e.type IN ('simple', 'master')"));
	if (!query.exec()) {
		return result;
	}
	while (query.next()) {
		const QString label = query.value(5).toString().trimmed();
		if (label.isEmpty()) {
			continue;
		}
		Part part;
		part.element = QUuid(query.value(0).toString());
		part.folio = QUuid(query.value(1).toString());
		part.folio_index = query.value(2).toInt() - 1;
		part.folio_label = query.value(3).toString().trimmed();
		part.cell = query.value(4).toString().trimmed();
		result[label] << part;
	}
	for (auto it = result.begin() ; it != result.end() ; ) {
		if (it.value().size() < 2) {
			it = result.erase(it);
			continue;
		}
		for (Part &part : it.value()) {
			part.has_parts = !project->dataBase()->elementInformation(part.element)
					.value(PartsLinks::infoKey()).toString().trimmed().isEmpty();
		}
		std::stable_sort(it.value().begin(), it.value().end(), [](const Part &a, const Part &b) {
			if (a.folio_index != b.folio_index) return a.folio_index < b.folio_index;
				//Column first, as the folio is read from left to right
			static const QRegularExpression digits(QStringLiteral("(\\d+)"));
			const int ca = digits.match(a.cell).captured(1).toInt();
			const int cb = digits.match(b.cell).captured(1).toInt();
			if (ca != cb) return ca < cb;
			return a.cell < b.cell;
		});
		++it;
	}
	return result;
}

int DeviceXRefs::mainOf(const QList<Part> &parts)
{
	for (int i = 0 ; i < parts.size() ; ++i) {
		if (parts.at(i).has_parts) {
			return i;
		}
	}
	return 0;
}

QString DeviceXRefs::placeText(const QString &formula, const Part &part)
{
	static const QRegularExpression letters(QStringLiteral("^([A-Za-z]*)(\\d*)$"));
	const QRegularExpressionMatch match = letters.match(part.cell);
	const QString row = match.hasMatch() ? match.captured(1) : QString();
	const QString column = match.hasMatch() ? match.captured(2) : part.cell;
	const QString number = QString::number(part.folio_index + 1);
	QString text = formula;
	text.replace(QStringLiteral("%id"), number);
	text.replace(QStringLiteral("%F"), part.folio_label.isEmpty() ? number : part.folio_label);
	text.replace(QStringLiteral("%f"), part.folio_label.isEmpty() ? number : part.folio_label);
	text.replace(QStringLiteral("%l"), row);
	text.replace(QStringLiteral("%c"), column);
	return text;
}

QStringList DeviceXRefs::placesFor(const QUuid &element, const QHash<QString, QList<Part>> &devices,
								   const QString &label, const DeviceXRefSettings &settings,
								   const QString &formula, QList<QUuid> *targets)
{
	QStringList places;
	const auto found = devices.constFind(label);
	if (!settings.enabled || found == devices.constEnd()) {
		return places;
	}
	const QList<Part> &parts = found.value();
	const int main = mainOf(parts);
	int self = -1;
	for (int i = 0 ; i < parts.size() ; ++i) {
		if (parts.at(i).element == element) self = i;
	}
	if (self < 0) {
		return places;
	}
	QList<int> shown;
	if (self == main || settings.secondary == DeviceXRefSettings::Secondary::All) {
		for (int i = 0 ; i < parts.size() ; ++i) {
			if (i != self) shown << i;
		}
	} else if (settings.secondary == DeviceXRefSettings::Secondary::Main) {
		shown << main;
	}
	for (int i : std::as_const(shown)) {
		places << placeText(formula, parts.at(i));
		if (targets) *targets << parts.at(i).element;
	}
	return places;
}

void DeviceXRefs::follow(QETProject *project)
{
	if (project && !followerOf(project)) {
		new Follower(project);
	}
}

void DeviceXRefs::scheduleRefresh(QETProject *project)
{
	if (Follower *follower = followerOf(project)) {
		follower->schedule();
	}
}

void DeviceXRefs::refresh(QETProject *project)
{
	if (!project) {
		return;
	}
	const DeviceXRefSettings settings = project->deviceXRefs();
	const QHash<QString, QList<Part>> parts = settings.enabled
			? devices(project) : QHash<QString, QList<Part>>();
	const QString formula = formulaOf(project);
	for (Diagram *folio : project->builtFolios()) {
		for (Element *element : folio->elements()) {
			const QString label = element->elementInformations()
					.value(QETInformation::ELMT_LABEL).toString().trimmed();
			QList<QUuid> targets;
			const QStringList places = (element->linkType() & (Element::Simple | Element::Master))
					? placesFor(element->uuid(), parts, label, settings, formula, &targets)
					: QStringList();
			DeviceXRefItem *item = itemOf(element);
			if (places.isEmpty()) {
				delete item;
				continue;
			}
			if (!item) {
				item = new DeviceXRefItem(element);
			}
			item->setPlaces(places, targets, settings.place);
		}
	}
		//And the function texts and summary symbols, with the same changes
	SummarySymbols::refresh(project);
}

DeviceXRefItem::DeviceXRefItem(Element *element) :
	QGraphicsObject(element),
	m_element(element)
{
	setFlag(QGraphicsItem::ItemIsSelectable, false);
	setFlag(QGraphicsItem::ItemIsMovable, false);
	setAcceptedMouseButtons(Qt::LeftButton);
	setToolTip(tr("Where the other parts of this device are. Double-click one to show it."));
}

QFont DeviceXRefItem::font() const
{
	return QETApp::diagramTextsFont(6);
}

void DeviceXRefItem::setPlaces(const QStringList &places, const QList<QUuid> &targets,
							   DeviceXRefSettings::Place place)
{
	if (places == m_places && targets == m_targets && place == m_place) {
		reposition();
		return;
	}
	prepareGeometryChange();
	m_places = places;
	m_targets = targets;
	m_place = place;
	const QFontMetricsF metrics(font());
	m_line_height = metrics.height();
	qreal width = 0.0;
	for (const QString &text : std::as_const(m_places)) {
		width = qMax(width, metrics.horizontalAdvance(text));
	}
	m_rect = QRectF(0, 0, width + 2, m_line_height * m_places.size());
	reposition();
	update();
}

void DeviceXRefItem::reposition()
{
	if (!m_element) {
		return;
	}
	QPointF at;
	DynamicElementTextItem *label = m_place == DeviceXRefSettings::Place::UnderLabel
			? labelTextOf(m_element) : nullptr;
	if (label && label->isVisible()) {
		at = label->mapToItem(m_element, label->boundingRect().bottomLeft());
	} else {
		const QRectF symbol = m_element->boundingRect();
		at = QPointF(symbol.left(), symbol.bottom() + 1);
	}
	setPos(at);
}

QRectF DeviceXRefItem::boundingRect() const
{
	return m_rect;
}

void DeviceXRefItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
						   QWidget *widget)
{
	Q_UNUSED(option)
	Q_UNUSED(widget)
	painter->save();
	painter->setFont(font());
	painter->setPen(Qt::black);
	for (int i = 0 ; i < m_places.size() ; ++i) {
		painter->drawText(QRectF(1, i * m_line_height, m_rect.width(), m_line_height),
						  Qt::AlignLeft | Qt::AlignVCenter, m_places.at(i));
	}
	painter->restore();
}

void DeviceXRefItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
	event->accept();
	const int line = int(event->pos().y() / qMax(1.0, m_line_height));
	if (!m_element || line < 0 || line >= m_targets.size() || !m_element->diagram()) {
		return;
	}
	QETProject *project = m_element->diagram()->project();
	const QUuid target = m_targets.at(line);
	for (Diagram *folio : project->folios()) {
		const QList<Diagram *> one{folio};
		if (!folio->isLoaded()) {
				//Built only when it holds the part looked for
			if (!project->dataBase()) continue;
			QSqlQuery query = project->dataBase()->newQuery();
			query.prepare(QStringLiteral(
				"SELECT 1 FROM element WHERE uuid = :uuid AND diagram_uuid = :folio"));
			query.bindValue(QStringLiteral(":uuid"), target.toString());
			query.bindValue(QStringLiteral(":folio"), folio->uuid().toString());
			if (!query.exec() || !query.next()) continue;
			project->buildFolios(one);
		}
		for (Element *element : folio->elements()) {
			if (element->uuid() == target) {
				QetGraphicsItem::showItem(element);
				return;
			}
		}
	}
}
