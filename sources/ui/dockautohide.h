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
#ifndef DOCKAUTOHIDE_H
#define DOCKAUTOHIDE_H

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>

class QAction;
class QDockWidget;
class QFrame;
class QLabel;
class QMainWindow;
class QToolBar;
class QToolButton;
class QVBoxLayout;
class QWidget;

/**
	@brief The DockAutoHide class hides the side panels of a main window
	by themselves (Brandon, 2026-11-16): with the setting on (Settings >
	General), a panel of the left or right side folds into a tab along the
	edge of the window, and slides out over the folio when the mouse rests
	on its tab or the tab is clicked. It folds back when the mouse leaves
	it -- or, once something in it was clicked, when the folio is.

	The pin of a panel keeps it open where it stands, as without the
	setting; the pin of an open one folds it again. Which panels are pinned,
	and which are folded, is kept from one start to the next.

	A folded panel is a hidden dock of its area: the state of the window
	saves it as such, and turning the setting off shows it again where it
	was, tabbed with the same panels.
*/
class DockAutoHide : public QObject
{
	Q_OBJECT

	public:
		explicit DockAutoHide(QMainWindow *window);
		~DockAutoHide() override;

		static bool isOn();
		static void setOn(bool on);
			/// Every main window follows the setting
		static void applyToAll();

			/// Follow the setting: fold the panels, or show them again
		void apply();
		bool isFolded(const QDockWidget *dock) const;
		bool isOpen(const QDockWidget *dock) const;
		bool isPinned(const QDockWidget *dock) const;
			/// Slide the folded @p dock out; @p focus gives it the keyboard
		void open(QDockWidget *dock, bool focus = false);
			/// Fold the panel slid out back into its tab
		void fold();
		void setPinned(QDockWidget *dock, bool pinned);
			/// The tab of the folded @p dock
		QToolButton *tabOf(const QDockWidget *dock) const;
		QWidget *overlay() const;

	protected:
		bool eventFilter(QObject *watched, QEvent *event) override;

	private:
		struct Folded
		{
			int area = 0;
			int width = 300;
			QToolButton *tab = nullptr;
			QAction *action = nullptr;
		};

		QList<QDockWidget *> sideDocks() const;
		void watch(QDockWidget *dock);
		void foldAway(QDockWidget *dock, int area = 0, int width = 0);
		void bringBack(QDockWidget *dock);
		void removeTab(const Folded &folded);
		void closeOpen();
		void dockVisibilityChanged(QDockWidget *dock, bool visible);
		void focusChanged(QWidget *now);
		void placeOverlay();
		void checkCursor();
		void updateTitleBar(QDockWidget *dock);
		void updateBars();
		void save() const;
		void buildOverlay();
		QToolBar *barFor(int area);

		QMainWindow *m_window;
		bool m_on = false;
		bool m_busy = false;
		QHash<QDockWidget *, Folded> m_folded;
		QList<QPointer<QDockWidget>> m_watched;
		QStringList m_pinned;
		QToolBar *m_left_bar = nullptr;
		QToolBar *m_right_bar = nullptr;
		QFrame *m_overlay = nullptr;
		QLabel *m_overlay_title = nullptr;
		QToolButton *m_overlay_pin = nullptr;
		QVBoxLayout *m_overlay_body = nullptr;
		QWidget *m_resize_handle = nullptr;
		QPointer<QDockWidget> m_open;
		QPointer<QWidget> m_open_widget;
		QTimer m_cursor_timer;
		QTimer m_hover_timer;
		QPointer<QDockWidget> m_hovered;
		int m_outside = 0;
		int m_resize_from_x = -1;
		int m_resize_from_width = 0;

		static QList<DockAutoHide *> s_all;
};

#endif // DOCKAUTOHIDE_H
