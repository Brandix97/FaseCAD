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
#include "diagrameventaddcabinetitem.h"

#include "../cabinet/cabinetsnap.h"
#include "../diagram.h"
#include "../qetapp.h"
#include "../qetdiagrameditor.h"
#include "../undocommand/addgraphicsobjectcommand.h"

#include <QFontMetricsF>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QPainter>
#include <QStatusBar>

#include <cmath>

/**
	@brief The CabinetReadoutItem class
	The box under the mouse saying how long the part being drawn is. It
	keeps its size on the screen whatever the zoom.
*/
class CabinetReadoutItem : public QGraphicsItem
{
	public:
		CabinetReadoutItem()
		{
			setFlag(ItemIgnoresTransformations, true);
			setAcceptedMouseButtons(Qt::NoButton);
			setZValue(100000);
			m_font.setPixelSize(13);
			m_font.setBold(true);
		}

		QString text() const {return m_text;}
		void setText(const QString &text)
		{
			if (text == m_text) return;
			prepareGeometryChange();
			m_text = text;
			update();
		}

		QRectF boundingRect() const override
		{
			return box().adjusted(-2, -2, 2, 2);
		}

		void paint(QPainter *painter, const QStyleOptionGraphicsItem *,
				   QWidget *) override
		{
			if (m_text.isEmpty()) return;
			painter->setRenderHint(QPainter::Antialiasing, true);
			painter->setPen(QPen(QColor(40, 48, 60), 1));
			painter->setBrush(QColor(255, 250, 205));
			painter->drawRoundedRect(box(), 3, 3);
			painter->setFont(m_font);
			painter->setPen(QColor(20, 24, 30));
			painter->drawText(box(), Qt::AlignCenter, m_text);
		}

	private:
			/// Under the mouse and a little to its right, clear of the pointer
		QRectF box() const
		{
			const QFontMetricsF metrics(m_font);
			return QRectF(14, 20, metrics.horizontalAdvance(m_text) + 14,
						  metrics.height() + 6);
		}

		QString m_text;
		QFont m_font;
};

namespace {

QString mm(qreal value)
{
	return QString::number(std::round(value * 10.0) / 10.0, 'g', 8);
}

} // namespace

/**
	@brief DiagramEventAddCabinetItem::DiagramEventAddCabinetItem
	@param diagram : the folio the part is added to
	@param properties : what the part is. The length and the direction of
	a wire duct or a DIN rail are those the user draws.
*/
DiagramEventAddCabinetItem::DiagramEventAddCabinetItem(
		Diagram *diagram, const CabinetItem::Properties &properties) :
	DiagramEventInterface(diagram),
	m_properties(properties)
{
	m_running = true;
	init();
}

DiagramEventAddCabinetItem::~DiagramEventAddCabinetItem()
{
		//A folio being destroyed has deleted its items already
	if (!m_diagram) return;
	CabinetSnap::hideMarker(m_diagram);
	dropPreview();
	if (m_readout) {
		if (m_readout->scene()) m_readout->scene()->removeItem(m_readout);
		delete m_readout;
		m_readout = nullptr;
	}
	{
		const QList<QGraphicsView *> views = m_diagram->views();
		for (QGraphicsView *view : views) {
			view->viewport()->unsetCursor();
			view->viewport()->update();
		}
		if (!views.isEmpty()) {
			if (auto *editor = QETApp::diagramEditorAncestorOf(views.constFirst()))
				editor->statusBar()->clearMessage();
		}
	}
}

void DiagramEventAddCabinetItem::init()
{
	m_running = true;
	if (!m_diagram) return;
	const QList<QGraphicsView *> views = m_diagram->views();
	for (QGraphicsView *view : views)
		view->viewport()->setCursor(Qt::CrossCursor);

		//Called again when this tool takes the place of another one
		//(Diagram::setEventInterface()): what it shows is made once
	if (!m_readout) {
		m_readout = new CabinetReadoutItem();
		m_readout->hide();
		m_diagram->QGraphicsScene::addItem(m_readout);

			//A plate has its measures: it follows the mouse from the start
		if (!isDrawn()) {
			m_item = new CabinetItem(m_properties);
			m_item->hide();
			m_diagram->addItem(m_item);
		}
	}
	updateStatusHint();
}

/**
	@brief DiagramEventAddCabinetItem::isDrawn
	@return true for a part the user draws from one end to the other (a
	wire duct, a DIN rail), false for one put down whole (a mounting plate)
*/
bool DiagramEventAddCabinetItem::isDrawn() const
{
	return CabinetItem::isLinear(m_properties.kind);
}

QString DiagramEventAddCabinetItem::readout() const
{
	return m_readout ? m_readout->text() : QString();
}

void DiagramEventAddCabinetItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	if (Q_UNLIKELY(!m_diagram || m_diagram->isReadOnly())) return;

	if (event->button() == Qt::RightButton) {
			//Gives up the part being drawn, or leaves the tool
		if (isDrawn() && m_drawing) {
			dropPreview();
			m_drawing = false;
			m_typed.clear();
			showReadout(event->scenePos());
			updateStatusHint();
		} else {
			endTool(true);
		}
		event->setAccepted(true);
		return;
	}
	if (event->button() != Qt::LeftButton) return;

	m_press_screen_pos = event->screenPos();
	if (!isDrawn()) {
		track(event->scenePos());
		if (commit()) endTool(false);
	} else if (!m_drawing) {
		begin(event->scenePos());
	} else {
		track(event->scenePos());
		commit();
	}
	event->setAccepted(true);
}

void DiagramEventAddCabinetItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	if (!m_diagram) return;
	track(event->scenePos());
	event->setAccepted(true);
}

void DiagramEventAddCabinetItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	if (event->button() != Qt::LeftButton || !m_diagram) return;
		//Pressed, dragged, released: the part ends where the button came up
	if (isDrawn() && m_drawing
			&& (event->screenPos() - m_press_screen_pos).manhattanLength() > 8) {
		track(event->scenePos());
		commit();
	}
	event->setAccepted(true);
}

void DiagramEventAddCabinetItem::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Escape) {
		if (isDrawn() && m_drawing) {
			dropPreview();
			m_drawing = false;
			m_typed.clear();
			showReadout(m_last_pos);
			updateStatusHint();
		} else {
			endTool(true);
		}
		event->setAccepted(true);
		return;
	}
	if (!isDrawn() || !m_drawing) return;

		//A length typed while the part is drawn
	const QString text = event->text();
	if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
		bool ok = false;
		typedLength(&ok);
		if (ok) {
			track(m_last_pos);
			commit();
		}
		event->setAccepted(true);
	} else if (event->key() == Qt::Key_Backspace) {
		m_typed.chop(1);
		track(m_last_pos);
		event->setAccepted(true);
	} else if (text.size() == 1
			   && (text.at(0).isDigit() || text.at(0) == QLatin1Char('.')
				   || text.at(0) == QLatin1Char(','))
			   && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) {
		if (m_typed.size() < 8) m_typed += text;
		track(m_last_pos);
		event->setAccepted(true);
	}
}

/**
	@brief DiagramEventAddCabinetItem::wantsKey
	@return true for the keys typing a length while a part is drawn: a
	shortcut of the window on a digit must not take them
*/
bool DiagramEventAddCabinetItem::wantsKey(const QKeyEvent *event) const
{
	if (!isDrawn() || !m_drawing) return false;
	if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) return false;
	switch (event->key()) {
		case Qt::Key_Return:
		case Qt::Key_Enter:
		case Qt::Key_Backspace:
		case Qt::Key_Period:
		case Qt::Key_Comma:
			return true;
		default:
			return event->key() >= Qt::Key_0 && event->key() <= Qt::Key_9;
	}
}

/**
	@brief DiagramEventAddCabinetItem::typedLength
	@return the length the user typed, in millimetres; @p ok says whether
	there is one
*/
qreal DiagramEventAddCabinetItem::typedLength(bool *ok) const
{
	QString text = m_typed;
	text.replace(QLatin1Char(','), QLatin1Char('.'));
	bool converted = false;
	const qreal value = text.toDouble(&converted);
	if (ok) *ok = converted && value > 0;
	return value;
}

/**
	@brief DiagramEventAddCabinetItem::begin
	The first click of a wire duct or a DIN rail: it starts at @p scene_pos.
*/
void DiagramEventAddCabinetItem::begin(const QPointF &scene_pos)
{
	m_start = CabinetSnap::point(scene_pos, m_diagram).value;
	m_typed.clear();
	m_drawing = true;
	CabinetItem::Properties properties = m_properties;
	properties.length = 0;
	m_item = new CabinetItem(properties);
	m_diagram->addItem(m_item);
	track(scene_pos);
	updateStatusHint();
}

/**
	@brief DiagramEventAddCabinetItem::track
	The mouse is at @p scene_pos: show the part as a click would leave it.
*/
void DiagramEventAddCabinetItem::track(const QPointF &scene_pos)
{
	m_last_pos = scene_pos;
	if (!m_item) {
			//Where a duct or a rail would start: the corner or the middle
			//it would catch is shown before the click
		CabinetSnap::showMarker(m_diagram, isDrawn() ? CabinetSnap::point(scene_pos, m_diagram)
													 : CabinetSnap::Snap());
		showReadout(scene_pos);
		return;
	}
	const qreal pixels = m_diagram->pixelsPerMm();
	const QSet<const QGraphicsItem *> ignored{m_item};

	if (!isDrawn()) {
			//The plate or the drawing hangs from the mouse by its top
			//left corner
		const CabinetSnap::Snap snap = CabinetSnap::topLeft(
					m_item->rect().translated(scene_pos), m_diagram, ignored);
		CabinetSnap::showMarker(m_diagram, snap);
		m_item->placeAt(snap.value);
		m_item->show();
		showReadout(scene_pos);
		return;
	}

		//The way the mouse went furthest; the one it had while it has
		//hardly moved
	CabinetItem::Properties properties = m_item->properties();
	const QPointF delta = scene_pos - m_start;
	if (std::abs(delta.x()) > 1.0 || std::abs(delta.y()) > 1.0) {
		properties.orientation = std::abs(delta.y()) > std::abs(delta.x())
				? Qt::Vertical : Qt::Horizontal;
	}
	const bool vertical = properties.orientation == Qt::Vertical;

		//The line through the middle of the part starts where the first
		//click was (caught already) and ends at the mouse, caught by a
		//corner or a middle as well
	const qreal from = vertical ? m_start.y() : m_start.x();
	const qreal across_size = properties.width * pixels;
	const qreal across = (vertical ? m_start.x() : m_start.y()) - across_size / 2.0;
	qreal to = vertical ? scene_pos.y() : scene_pos.x();

	bool typed = false;
	const qreal typed_length = typedLength(&typed);
	CabinetSnap::Snap snap;
	if (typed) {
		to = from + (to < from ? -1.0 : 1.0) * typed_length * pixels;
	} else {
		snap = CabinetSnap::point(scene_pos, m_diagram, ignored);
		const bool caught = vertical ? snap.caught_y : snap.caught_x;
		const qreal snapped = vertical ? snap.value.y() : snap.value.x();
		if (caught) {
			to = snapped;
		} else if (CabinetSnap::active()) {
				//A whole number of millimetres from where it starts
			to = from + CabinetSnap::roundMm(to - from, m_diagram);
		}
			//The marker for what decides the length only
		if (snap.found && std::abs((vertical ? snap.at.y() : snap.at.x()) - to) > 1e-6)
			snap.found = false;
	}
	CabinetSnap::showMarker(m_diagram, snap);

	properties.length = typed ? typed_length
							  : std::round(std::abs(to - from) / pixels * 10.0) / 10.0;
	const qreal low = qMin(from, to);
	m_item->setProperties(properties);
	m_item->placeAt(vertical ? QPointF(across, low) : QPointF(low, across));
	showReadout(scene_pos);
}

/**
	@brief DiagramEventAddCabinetItem::commit
	Leave the part shown on the folio, through the undo stack.
	@return false if there is nothing to leave: a part without length
*/
bool DiagramEventAddCabinetItem::commit()
{
	if (!m_item) return false;
	if (isDrawn() && m_item->properties().length < 1.0) return false;

	m_item->show();
		//The command puts the part where it is told, each time it is redone
	m_diagram->undoStack().push(
				new AddGraphicsObjectCommand(m_item, m_diagram, m_item->pos()));
	m_item = nullptr;   //The folio and the undo stack have it now
	CabinetSnap::hideMarker(m_diagram);
	m_drawing = false;
	m_typed.clear();
	showReadout(m_last_pos);
	updateStatusHint();
	return true;
}

void DiagramEventAddCabinetItem::dropPreview()
{
	if (!m_item || !m_diagram) return;
	m_diagram->removeItem(m_item);
	delete m_item;
	m_item = nullptr;
}

void DiagramEventAddCabinetItem::endTool(bool aborted)
{
	dropPreview();
	m_running = false;
	m_abort = aborted;
	emit finish();
}

/**
	@brief DiagramEventAddCabinetItem::showReadout
	Say under the mouse how long the part being drawn is.
*/
void DiagramEventAddCabinetItem::showReadout(const QPointF &scene_pos)
{
	if (!m_readout) return;
	QString text;
	if (m_item && isDrawn()) {
		text = m_typed.isEmpty()
				? tr("%1 mm", "length of the part being drawn")
				  .arg(mm(m_item->properties().length))
				: tr("%1 mm ⏎", "length typed for the part being drawn, Enter accepts it")
				  .arg(m_typed);
	} else if (m_item) {
		text = m_item->description();
	}
	m_readout->setText(text);
	m_readout->setPos(scene_pos);
	m_readout->setVisible(!text.isEmpty());
}

void DiagramEventAddCabinetItem::updateStatusHint()
{
	if (!m_diagram || m_diagram->views().isEmpty()) return;
	auto *editor = QETApp::diagramEditorAncestorOf(m_diagram->views().constFirst());
	if (!editor) return;

	QString message;
	switch (m_properties.kind) {
		case CabinetItem::Kind::MountingPlate:
			message = tr("Mounting plate: click where it goes | Esc to cancel");
			break;
		case CabinetItem::Kind::Drawing:
			message = tr("DXF drawing: click where it goes | Esc to cancel");
			break;
		case CabinetItem::Kind::DinRail:
			message = m_drawing
					? tr("DIN rail: click where it ends, or type its length in mm and press Enter | Esc to give it up")
					: tr("DIN rail: click where it starts | Right click or Esc to finish");
			break;
		case CabinetItem::Kind::WireDuct:
			message = m_drawing
					? tr("Wire duct: click where it ends, or type its length in mm and press Enter | Esc to give it up")
					: tr("Wire duct of %1 mm: click where it starts | Right click or Esc to finish")
					  .arg(mm(m_properties.width));
			break;
	}
	editor->statusBar()->showMessage(message);
}
