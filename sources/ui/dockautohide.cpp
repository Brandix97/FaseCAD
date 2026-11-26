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
#include "dockautohide.h"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

QList<DockAutoHide *> DockAutoHide::s_all;

namespace
{
	const QString on_key = QStringLiteral("diagrameditor/autohide_docks");
	const QString pinned_key = QStringLiteral("diagrameditor/autohide_pinned");
	const QString folded_key = QStringLiteral("diagrameditor/autohide_folded");
	const QString title_bar_name = QStringLiteral("fasecad_autohide_titlebar");
	const int min_width = 180;

	bool isSide(int area)
	{
		return area == Qt::LeftDockWidgetArea || area == Qt::RightDockWidgetArea;
	}

		/**
			A pushpin: upright for a panel pinned where it stands, lying
			for a panel which folds by itself.
		*/
	QIcon pinIcon(bool pinned, const QColor &color)
	{
		QIcon icon;
		for (const int size : {16, 32})
		{
			QPixmap pixmap(size, size);
			pixmap.fill(Qt::transparent);
			QPainter painter(&pixmap);
			painter.setRenderHint(QPainter::Antialiasing);
			painter.scale(size / 16.0, size / 16.0);
			if (!pinned) {
				painter.translate(16, 0);
				painter.rotate(90);
			}
			painter.setPen(QPen(color, 1.3));
			painter.setBrush(Qt::NoBrush);
			painter.drawRect(QRectF(5.5, 1.5, 5, 6));
			painter.drawLine(QPointF(3.5, 7.5), QPointF(12.5, 7.5));
			painter.drawLine(QPointF(8, 7.5), QPointF(8, 14.5));
			icon.addPixmap(pixmap);
		}
		return icon;
	}

		/// The tab of a folded panel, its name read along the edge
	class SideTab : public QToolButton
	{
		public:
			SideTab(const QString &text, int area, QWidget *parent) :
				QToolButton(parent),
				m_area(area)
			{
				setText(text);
				setToolButtonStyle(Qt::ToolButtonTextOnly);
				setAutoRaise(true);
				setCheckable(true);
				setFocusPolicy(Qt::NoFocus);
			}

			QSize sizeHint() const override
			{
				const QSize size = QToolButton::sizeHint();
				return QSize(size.height(), size.width() + 16);
			}
			QSize minimumSizeHint() const override {return sizeHint();}

		protected:
			void paintEvent(QPaintEvent *) override
			{
				QStylePainter painter(this);
				QStyleOptionToolButton option;
				initStyleOption(&option);
				if (option.state & (QStyle::State_MouseOver | QStyle::State_Sunken | QStyle::State_On)) {
					painter.drawPrimitive(QStyle::PE_PanelButtonTool, option);
				}
				if (m_area == Qt::LeftDockWidgetArea) {
					painter.translate(0, height());
					painter.rotate(-90);
				} else {
					painter.translate(width(), 0);
					painter.rotate(90);
				}
				option.rect = QRect(0, 0, height(), width());
				painter.drawControl(QStyle::CE_ToolButtonLabel, option);
			}

		private:
			int m_area;
	};

		/// The title of a side panel while the panels fold by themselves:
		/// its name, its pin and its close button
	QWidget *titleBarFor(QDockWidget *dock, DockAutoHide *auto_hide)
	{
		auto *bar = new QWidget(dock);
		bar->setObjectName(title_bar_name);
		auto *layout = new QHBoxLayout(bar);
		layout->setContentsMargins(6, 2, 2, 2);
		layout->setSpacing(2);
		auto *title = new QLabel(dock->windowTitle(), bar);
		layout->addWidget(title, 1);
		QObject::connect(dock, &QDockWidget::windowTitleChanged, title, &QLabel::setText);

		auto *pin = new QToolButton(bar);
		pin->setObjectName(QStringLiteral("autohide_pin"));
		pin->setAutoRaise(true);
		pin->setIcon(pinIcon(true, bar->palette().color(QPalette::WindowText)));
		pin->setToolTip(DockAutoHide::tr("Hide this panel automatically"));
		QObject::connect(pin, &QToolButton::clicked, auto_hide, [auto_hide, dock]() {
			auto_hide->setPinned(dock, false);
		});
		layout->addWidget(pin);

		auto *close = new QToolButton(bar);
		close->setAutoRaise(true);
		close->setIcon(bar->style()->standardIcon(QStyle::SP_TitleBarCloseButton));
		close->setToolTip(DockAutoHide::tr("Close"));
		QObject::connect(close, &QToolButton::clicked, dock, &QDockWidget::close);
		layout->addWidget(close);
		return bar;
	}
}

DockAutoHide::DockAutoHide(QMainWindow *window) :
	QObject(window),
	m_window(window)
{
	s_all << this;
	m_window->installEventFilter(this);

	m_cursor_timer.setInterval(150);
	connect(&m_cursor_timer, &QTimer::timeout, this, &DockAutoHide::checkCursor);
	m_hover_timer.setSingleShot(true);
	m_hover_timer.setInterval(350);
	connect(&m_hover_timer, &QTimer::timeout, this, [this]() {
		if (m_hovered && m_folded.contains(m_hovered)) {
			QToolButton *tab = m_folded.value(m_hovered).tab;
			if (tab && tab->rect().contains(tab->mapFromGlobal(QCursor::pos()))) {
				open(m_hovered);
			}
		}
	});
	connect(qApp, &QApplication::focusChanged, this,
			[this](QWidget *, QWidget *now) {focusChanged(now);});
}

DockAutoHide::~DockAutoHide()
{
	s_all.removeAll(this);
}

bool DockAutoHide::isOn()
{
	QSettings settings;
	return settings.value(on_key, false).toBool();
}

void DockAutoHide::setOn(bool on)
{
	QSettings settings;
	settings.setValue(on_key, on);
}

void DockAutoHide::applyToAll()
{
	for (DockAutoHide *auto_hide : std::as_const(s_all)) {
		auto_hide->apply();
	}
}

/**
	@brief DockAutoHide::apply
	With the setting on, fold every side panel which is open and not
	pinned, and the ones folded when the window was last closed; with it
	off, show the folded panels again where they were.
*/
void DockAutoHide::apply()
{
	m_on = isOn();
	QSettings settings;
	m_pinned = settings.value(pinned_key).toStringList();
	for (QDockWidget *dock : m_window->findChildren<QDockWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
		watch(dock);
	}

	if (m_on)
	{
			//Folded when the window was last closed: hidden in the state
			//of the window
		const QStringList folded = settings.value(folded_key).toStringList();
		for (const QString &line : folded)
		{
			const QStringList parts = line.split(QLatin1Char('|'));
			if (parts.size() != 3) {
				continue;
			}
			auto *dock = m_window->findChild<QDockWidget *>(parts.at(0), Qt::FindDirectChildrenOnly);
			if (dock && !dock->isFloating() && dock->isHidden() && !isPinned(dock)) {
				foldAway(dock, parts.at(1).toInt(), parts.at(2).toInt());
			}
		}
		for (QDockWidget *dock : sideDocks()) {
			if (!m_folded.contains(dock) && !isPinned(dock)
				&& dock->toggleViewAction()->isChecked()) {
				foldAway(dock);
			}
		}
	}
	else
	{
		const QList<QDockWidget *> docks = m_folded.keys();
		for (QDockWidget *dock : docks) {
			bringBack(dock);
		}
	}

	for (QDockWidget *dock : sideDocks()) {
		updateTitleBar(dock);
	}
	updateBars();
	save();
}

bool DockAutoHide::isFolded(const QDockWidget *dock) const
{
	return m_folded.contains(const_cast<QDockWidget *>(dock));
}

bool DockAutoHide::isOpen(const QDockWidget *dock) const
{
	return dock && m_open == dock;
}

bool DockAutoHide::isPinned(const QDockWidget *dock) const
{
	return dock && m_pinned.contains(dock->objectName());
}

QToolButton *DockAutoHide::tabOf(const QDockWidget *dock) const
{
	return m_folded.value(const_cast<QDockWidget *>(dock)).tab;
}

QWidget *DockAutoHide::overlay() const
{
	return m_overlay;
}

/**
	@brief DockAutoHide::open
	Slide the folded @p dock out over the folio, next to its tab
*/
void DockAutoHide::open(QDockWidget *dock, bool focus)
{
	if (!dock || !m_folded.contains(dock)) {
		return;
	}
	if (m_open == dock)
	{
		if (focus && m_open_widget) {
			m_open_widget->setFocus(Qt::OtherFocusReason);
		}
		return;
	}
	fold();

	QWidget *content = dock->widget();
	if (!content) {
		return;
	}
	if (!m_overlay) {
		buildOverlay();
	}
	m_open = dock;
	m_open_widget = content;
	m_overlay_title->setText(dock->windowTitle());
		//Out of the hidden dock, into the panel slid out
	m_overlay_body->addWidget(content);
	content->show();
	placeOverlay();
	m_overlay->show();
	m_overlay->raise();
	if (QToolButton *tab = m_folded.value(dock).tab) {
		tab->setChecked(true);
	}
	m_outside = 0;
	m_cursor_timer.start();
	if (focus) {
		content->setFocus(Qt::OtherFocusReason);
	}
}

/**
	@brief DockAutoHide::fold
	Fold the panel slid out back into its tab: what it shows goes back
	into its dock
*/
void DockAutoHide::fold()
{
	m_cursor_timer.stop();
	m_resize_from_x = -1;
	if (m_overlay) {
		m_overlay->hide();
	}
	if (!m_open) {
		return;
	}
	QDockWidget *dock = m_open;
	QWidget *content = m_open_widget;
	m_open = nullptr;
	m_open_widget = nullptr;
	if (content) {
		m_busy = true;
		dock->setWidget(content);
		m_busy = false;
	}
	if (QToolButton *tab = m_folded.value(dock).tab) {
		tab->setChecked(false);
	}
}

/**
	@brief DockAutoHide::setPinned
	A pinned panel stays open where it stands; one unpinned folds into its
	tab -- while the setting is on.
*/
void DockAutoHide::setPinned(QDockWidget *dock, bool pinned)
{
	if (!dock) {
		return;
	}
	m_pinned.removeAll(dock->objectName());
	if (pinned)
	{
		m_pinned << dock->objectName();
		bringBack(dock);
	}
	else if (m_on && dock->isVisible())
	{
		foldAway(dock);
	}
	updateTitleBar(dock);
	updateBars();
	save();
}

bool DockAutoHide::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == m_window)
	{
		if (event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest) {
			if (m_open) {
				placeOverlay();
			}
		} else if (event->type() == QEvent::Close) {
			fold();
		} else if (event->type() == QEvent::ChildPolished) {
				//A panel added later (the assistant)
			if (auto *dock = qobject_cast<QDockWidget *>(static_cast<QChildEvent *>(event)->child())) {
				watch(dock);
			}
		}
		return false;
	}

	if (watched == m_resize_handle && m_open)
	{
		auto *mouse = static_cast<QMouseEvent *>(event);
		switch (event->type())
		{
			case QEvent::MouseButtonPress:
				m_resize_from_x = qRound(mouse->globalPosition().x());
				m_resize_from_width = m_overlay->width();
				return true;
			case QEvent::MouseMove:
				if (m_resize_from_x >= 0 && m_folded.contains(m_open))
				{
					const int dx = qRound(mouse->globalPosition().x()) - m_resize_from_x;
					Folded &folded = m_folded[m_open];
					folded.width = m_resize_from_width
							+ (folded.area == Qt::LeftDockWidgetArea ? dx : -dx);
					placeOverlay();
				}
				return true;
			case QEvent::MouseButtonRelease:
				m_resize_from_x = -1;
				save();
				return true;
			default:
				break;
		}
		return false;
	}

		//The tab of a folded panel: resting on it slides it out
	if (auto *tab = qobject_cast<QToolButton *>(watched))
	{
		for (auto it = m_folded.constBegin() ; it != m_folded.constEnd() ; ++it)
		{
			if (it.value().tab != tab) {
				continue;
			}
			if (event->type() == QEvent::Enter) {
				m_hovered = it.key();
				m_hover_timer.start();
			} else if (event->type() == QEvent::Leave) {
				m_hover_timer.stop();
			}
			break;
		}
	}
	return false;
}

/**
	@brief DockAutoHide::sideDocks
	@return the panels of the window docked on its left or right side,
	shown or not
*/
QList<QDockWidget *> DockAutoHide::sideDocks() const
{
	QList<QDockWidget *> docks;
	for (QDockWidget *dock : m_window->findChildren<QDockWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
		if (!dock->isFloating() && isSide(m_window->dockWidgetArea(dock))) {
			docks << dock;
		}
	}
	return docks;
}

void DockAutoHide::watch(QDockWidget *dock)
{
	for (const QPointer<QDockWidget> &watched : std::as_const(m_watched)) {
		if (watched == dock) {
			return;
		}
	}
	m_watched << dock;
	connect(dock, &QDockWidget::visibilityChanged, this, [this, dock](bool visible) {
		dockVisibilityChanged(dock, visible);
	});
	connect(dock, &QDockWidget::topLevelChanged, this, [this, dock]() {updateTitleBar(dock);});
	connect(dock, &QDockWidget::dockLocationChanged, this, [this, dock]() {updateTitleBar(dock);});
}

/**
	@brief DockAutoHide::foldAway
	Fold @p dock into a tab of its side
	@param area : the side, 0 for the one it is docked on
	@param width : the width it slides out with, 0 for its own
*/
void DockAutoHide::foldAway(QDockWidget *dock, int area, int width)
{
	if (!dock || m_folded.contains(dock)) {
		return;
	}
	Folded folded;
	folded.area = area ? area : int(m_window->dockWidgetArea(dock));
	if (!isSide(folded.area)) {
		return;
	}
	folded.width = width > 0 ? width : (dock->isVisible() ? dock->width() : 300);
	folded.width = qMax(min_width, folded.width);

	QToolBar *bar = barFor(folded.area);
	auto *tab = new SideTab(dock->windowTitle(), folded.area, bar);
	tab->setObjectName(QStringLiteral("autohide_tab_") + dock->objectName());
	tab->setToolTip(dock->windowTitle());
	tab->installEventFilter(this);
	connect(dock, &QDockWidget::windowTitleChanged, tab, &QToolButton::setText);
	connect(tab, &QToolButton::clicked, this, [this, dock]() {
		QWidget *focus = QApplication::focusWidget();
		if (m_open == dock && m_overlay && focus && m_overlay->isAncestorOf(focus)) {
			fold();
		} else {
			open(dock, true);
		}
	});
	folded.tab = tab;
	folded.action = bar->addWidget(tab);
	m_folded.insert(dock, folded);

	m_busy = true;
	dock->hide();
	m_busy = false;
	updateBars();
}

/**
	@brief DockAutoHide::bringBack
	Show the folded @p dock again where it was docked
*/
void DockAutoHide::bringBack(QDockWidget *dock)
{
	if (!m_folded.contains(dock)) {
		return;
	}
	if (m_open == dock) {
		fold();
	}
	removeTab(m_folded.take(dock));

	m_busy = true;
	dock->show();
	m_busy = false;
	updateBars();
}

/**
	@brief DockAutoHide::removeTab
	Take the tab of a panel no longer folded off its bar
*/
void DockAutoHide::removeTab(const Folded &folded)
{
		//The action of the bar owns the tab: deleting it deletes both
	QPointer<QToolButton> tab = folded.tab;
	if (QToolBar *bar = barFor(folded.area)) {
		bar->removeAction(folded.action);
	}
	delete folded.action;
	delete tab.data();
}

/**
	@brief DockAutoHide::closeOpen
	Close the panel slid out: it folds, and leaves its tab
*/
void DockAutoHide::closeOpen()
{
	QDockWidget *dock = m_open;
	fold();
	if (!dock || !m_folded.contains(dock)) {
		return;
	}
	removeTab(m_folded.take(dock));
	updateBars();
	save();
}

/**
	@brief DockAutoHide::dockVisibilityChanged
	A folded panel asked to show (the View menu, a jump to it) slides out
	instead; a side panel opened while the setting is on folds into a tab
	and slides out.
*/
void DockAutoHide::dockVisibilityChanged(QDockWidget *dock, bool visible)
{
	if (m_busy || !m_on || !visible) {
		return;
	}
	QPointer<QDockWidget> guard(dock);
	QTimer::singleShot(0, this, [this, guard]()
	{
		if (!guard || !m_on || !guard->isVisible()) {
			return;
		}
		if (m_folded.contains(guard))
		{
			m_busy = true;
			guard->hide();
			m_busy = false;
			open(guard, true);
			return;
		}
		if (guard->isFloating() || isPinned(guard) || !isSide(m_window->dockWidgetArea(guard))) {
			return;
		}
		foldAway(guard);
		updateTitleBar(guard);
		open(guard, true);
		save();
	});
}

/**
	@brief DockAutoHide::focusChanged
	Clicking the folio, or anything else of the window outside the panel
	slid out, folds it
*/
void DockAutoHide::focusChanged(QWidget *now)
{
	if (!m_open || !m_overlay || !now) {
		return;
	}
	if (m_overlay->isAncestorOf(now) || now->window() != m_window) {
		return;
	}
	if (m_overlay->rect().contains(m_overlay->mapFromGlobal(QCursor::pos()))) {
		return;
	}
	fold();
}

void DockAutoHide::placeOverlay()
{
	if (!m_overlay || !m_open || !m_folded.contains(m_open)) {
		return;
	}
	QWidget *central = m_window->centralWidget();
	const QRect area = central ? central->geometry() : m_window->rect();
	Folded &folded = m_folded[m_open];
	const int max_width = qMax(min_width, area.width() - 40);
	folded.width = qBound(min_width, folded.width, max_width);
	QRect rect(area.left(), area.top(), folded.width, area.height());
	if (folded.area == Qt::RightDockWidgetArea) {
		rect.moveRight(area.right());
	}
	m_overlay->setGeometry(rect);
	const int handle = 5;
	m_resize_handle->setGeometry(folded.area == Qt::LeftDockWidgetArea
								 ? QRect(rect.width() - handle, 0, handle, rect.height())
								 : QRect(0, 0, handle, rect.height()));
	m_resize_handle->raise();
}

/**
	@brief DockAutoHide::checkCursor
	The panel slid out by the mouse folds once the mouse has left it for a
	moment -- not while a button is held (a symbol dragged from it onto the
	folio), a menu or a window of its own is open, or the keyboard is in it
*/
void DockAutoHide::checkCursor()
{
	if (!m_open || !m_overlay) {
		m_cursor_timer.stop();
		return;
	}
	if (QApplication::activePopupWidget() || QApplication::activeModalWidget()
		|| QGuiApplication::mouseButtons() != Qt::NoButton) {
		m_outside = 0;
		return;
	}
	const QPoint cursor = QCursor::pos();
	bool inside = m_overlay->rect().contains(m_overlay->mapFromGlobal(cursor));
	if (QToolButton *tab = m_folded.value(m_open).tab) {
		inside = inside || tab->rect().contains(tab->mapFromGlobal(cursor));
	}
	QWidget *focus = QApplication::focusWidget();
	if (inside || (focus && m_overlay->isAncestorOf(focus))) {
		m_outside = 0;
		return;
	}
	if (++m_outside >= 4) {
		fold();
	}
}

void DockAutoHide::updateTitleBar(QDockWidget *dock)
{
	if (!dock) {
		return;
	}
	QWidget *current = dock->titleBarWidget();
	const bool ours = current && current->objectName() == title_bar_name;
	const bool wanted = m_on && !dock->isFloating() && isSide(m_window->dockWidgetArea(dock));
	if (wanted && !current) {
		dock->setTitleBarWidget(titleBarFor(dock, this));
	} else if (!wanted && ours) {
		dock->setTitleBarWidget(nullptr);
		current->deleteLater();
	}
}

void DockAutoHide::updateBars()
{
	for (const int area : {int(Qt::LeftDockWidgetArea), int(Qt::RightDockWidgetArea)})
	{
		bool any = false;
		for (const Folded &folded : std::as_const(m_folded)) {
			if (folded.area == area) {
				any = true;
				break;
			}
		}
		QToolBar *bar = area == Qt::LeftDockWidgetArea ? m_left_bar : m_right_bar;
		if (bar) {
			bar->setVisible(any);
		}
	}
}

void DockAutoHide::save() const
{
	QSettings settings;
	settings.setValue(pinned_key, m_pinned);
	QStringList folded;
	for (auto it = m_folded.constBegin() ; it != m_folded.constEnd() ; ++it) {
		folded << QStringLiteral("%1|%2|%3").arg(it.key()->objectName())
				  .arg(it.value().area).arg(it.value().width);
	}
	settings.setValue(folded_key, folded);
}

void DockAutoHide::buildOverlay()
{
	m_overlay = new QFrame(m_window);
	m_overlay->setObjectName(QStringLiteral("fasecad_autohide_overlay"));
	m_overlay->setFrameShape(QFrame::StyledPanel);
	m_overlay->setFrameShadow(QFrame::Raised);
	m_overlay->setAutoFillBackground(true);
	m_overlay->hide();

	auto *layout = new QVBoxLayout(m_overlay);
	layout->setContentsMargins(3, 3, 3, 3);
	layout->setSpacing(2);

	auto *header = new QHBoxLayout();
	header->setContentsMargins(4, 0, 0, 0);
	m_overlay_title = new QLabel(m_overlay);
	QFont bold = m_overlay_title->font();
	bold.setBold(true);
	m_overlay_title->setFont(bold);
	header->addWidget(m_overlay_title, 1);

	m_overlay_pin = new QToolButton(m_overlay);
	m_overlay_pin->setObjectName(QStringLiteral("autohide_overlay_pin"));
	m_overlay_pin->setAutoRaise(true);
	m_overlay_pin->setIcon(pinIcon(false, m_overlay->palette().color(QPalette::WindowText)));
	m_overlay_pin->setToolTip(tr("Keep this panel open"));
	connect(m_overlay_pin, &QToolButton::clicked, this, [this]() {
		if (m_open) {
			setPinned(m_open, true);
		}
	});
	header->addWidget(m_overlay_pin);

	auto *close = new QToolButton(m_overlay);
	close->setAutoRaise(true);
	close->setIcon(m_overlay->style()->standardIcon(QStyle::SP_TitleBarCloseButton));
	close->setToolTip(tr("Close"));
	connect(close, &QToolButton::clicked, this, &DockAutoHide::closeOpen);
	header->addWidget(close);
	layout->addLayout(header);

	m_overlay_body = new QVBoxLayout();
	m_overlay_body->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(m_overlay_body, 1);

	m_resize_handle = new QWidget(m_overlay);
	m_resize_handle->setCursor(Qt::SizeHorCursor);
	m_resize_handle->setToolTip(tr("Drag to change the width of the panel"));
	m_resize_handle->installEventFilter(this);
}

QToolBar *DockAutoHide::barFor(int area)
{
	if (!isSide(area)) {
		return nullptr;
	}
	QToolBar *&bar = area == Qt::LeftDockWidgetArea ? m_left_bar : m_right_bar;
	if (!bar)
	{
		bar = new QToolBar(m_window);
		bar->setObjectName(area == Qt::LeftDockWidgetArea
						   ? QStringLiteral("fasecad_autohide_left")
						   : QStringLiteral("fasecad_autohide_right"));
		bar->setWindowTitle(tr("Folded panels"));
		bar->setMovable(false);
		bar->setFloatable(false);
		bar->setContextMenuPolicy(Qt::PreventContextMenu);
		bar->toggleViewAction()->setVisible(false);
		m_window->addToolBar(area == Qt::LeftDockWidgetArea ? Qt::LeftToolBarArea
															: Qt::RightToolBarArea, bar);
		bar->hide();
	}
	return bar;
}
