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
#include "qetapptest.h"
#include "ui/dockautohide.h"

#include <QDockWidget>
#include <QLineEdit>
#include <QMainWindow>
#include <QSettings>
#include <QToolButton>

#include <memory>

/**
	The side panels which hide by themselves (Brandon, 2026-11-16): with
	the setting on, a side panel folds into a tab along the edge of the
	window and slides out over the drawing from it; its pin keeps it open;
	the setting off shows them again where they were.
*/
class tst_dockautohide : public QObject
{
	Q_OBJECT

	struct Window
	{
		std::unique_ptr<QMainWindow> window;
		QDockWidget *projects = nullptr;
		QDockWidget *undo = nullptr;
		QDockWidget *properties = nullptr;
		QLineEdit *drawing = nullptr;
	};

	static QDockWidget *dock(QMainWindow *window, const QString &name, Qt::DockWidgetArea area)
	{
		auto *dock = new QDockWidget(name, window);
		dock->setObjectName(name);
		auto *content = new QLineEdit(dock);
		content->setObjectName(name + QStringLiteral("_content"));
		dock->setWidget(content);
		window->addDockWidget(area, dock);
		return dock;
	}

	static Window makeWindow()
	{
		Window made;
		made.window = std::make_unique<QMainWindow>();
		made.drawing = new QLineEdit(made.window.get());
		made.window->setCentralWidget(made.drawing);
		made.projects = dock(made.window.get(), QStringLiteral("projects"), Qt::LeftDockWidgetArea);
		made.undo = dock(made.window.get(), QStringLiteral("undo"), Qt::LeftDockWidgetArea);
		made.window->tabifyDockWidget(made.undo, made.projects);
		made.properties = dock(made.window.get(), QStringLiteral("properties"), Qt::RightDockWidgetArea);
		made.window->resize(1200, 800);
		made.window->show();
		QCoreApplication::processEvents();
		return made;
	}

private slots:
	void init()
	{
		QSettings settings;
		settings.remove(QStringLiteral("diagrameditor/autohide_docks"));
		settings.remove(QStringLiteral("diagrameditor/autohide_pinned"));
		settings.remove(QStringLiteral("diagrameditor/autohide_folded"));
	}

	void offByDefaultNothingFolds()
	{
		QVERIFY(!DockAutoHide::isOn());
		Window made = makeWindow();
		DockAutoHide auto_hide(made.window.get());
		auto_hide.apply();
		for (QDockWidget *panel : {made.projects, made.undo, made.properties}) {
			QVERIFY(!auto_hide.isFolded(panel));
			QVERIFY(panel->toggleViewAction()->isChecked());
			QVERIFY(!panel->titleBarWidget());
		}
	}

	void theSidePanelsFoldIntoTabs()
	{
		DockAutoHide::setOn(true);
		Window made = makeWindow();
		DockAutoHide auto_hide(made.window.get());
		auto_hide.apply();
		for (QDockWidget *panel : {made.projects, made.undo, made.properties}) {
			QVERIFY(auto_hide.isFolded(panel));
			QVERIFY(panel->isHidden());
			QToolButton *tab = auto_hide.tabOf(panel);
			QVERIFY(tab);
			QCOMPARE(tab->text(), panel->windowTitle());
				//Taller than wide: its name reads along the edge
			QVERIFY(tab->sizeHint().height() > tab->sizeHint().width());
		}
	}

	void aFoldedPanelSlidesOutAndBack()
	{
		DockAutoHide::setOn(true);
		Window made = makeWindow();
		DockAutoHide auto_hide(made.window.get());
		auto_hide.apply();
		QWidget *content = made.properties->widget();
		QVERIFY(content);

		auto_hide.open(made.properties);
		QVERIFY(auto_hide.isOpen(made.properties));
		QWidget *overlay = auto_hide.overlay();
		QVERIFY(overlay && overlay->isVisible());
		QVERIFY(overlay->isAncestorOf(content));
		QVERIFY(made.properties->isHidden());
			//Over the drawing, at its right edge
		const QRect drawing = made.drawing->geometry();
		QCOMPARE(overlay->geometry().right(), drawing.right());
		QVERIFY(overlay->width() >= 180 && overlay->width() < drawing.width());

		auto_hide.fold();
		QVERIFY(!auto_hide.isOpen(made.properties));
		QVERIFY(!overlay->isVisible());
		QCOMPARE(made.properties->widget(), content);
		QVERIFY(auto_hide.isFolded(made.properties));
	}

	void theMouseLeavingFoldsIt()
	{
		DockAutoHide::setOn(true);
		Window made = makeWindow();
		DockAutoHide auto_hide(made.window.get());
		auto_hide.apply();
		auto_hide.open(made.projects);
		QVERIFY(auto_hide.isOpen(made.projects));
		QCursor::setPos(made.window->mapToGlobal(made.drawing->geometry().center()));
		QTRY_VERIFY_WITH_TIMEOUT(!auto_hide.isOpen(made.projects), 3000);
	}

	void aPanelAskedForSlidesOut()
	{
		DockAutoHide::setOn(true);
		Window made = makeWindow();
		DockAutoHide auto_hide(made.window.get());
		auto_hide.apply();
			//The Display menu, or the program showing it
		made.undo->toggleViewAction()->trigger();
		QTRY_VERIFY(auto_hide.isOpen(made.undo));
		QVERIFY(made.undo->isHidden());
		QVERIFY(auto_hide.isFolded(made.undo));
	}

	void thePinKeepsItOpen()
	{
		DockAutoHide::setOn(true);
		{
			Window made = makeWindow();
			DockAutoHide auto_hide(made.window.get());
			auto_hide.apply();
			auto_hide.open(made.properties);
			auto_hide.setPinned(made.properties, true);
			QVERIFY(!auto_hide.isFolded(made.properties));
			QVERIFY(!auto_hide.isOpen(made.properties));
			QVERIFY(made.properties->isVisible());
			QVERIFY(made.properties->widget());
				//Its title has the pin, which folds it again
			QWidget *title = made.properties->titleBarWidget();
			QVERIFY(title);
			auto *pin = title->findChild<QToolButton *>(QStringLiteral("autohide_pin"));
			QVERIFY(pin);
			pin->click();
			QVERIFY(auto_hide.isFolded(made.properties));
			auto_hide.setPinned(made.properties, true);
		}
			//Pinned at the next start too
		Window again = makeWindow();
		DockAutoHide auto_hide(again.window.get());
		auto_hide.apply();
		QVERIFY(auto_hide.isPinned(again.properties));
		QVERIFY(!auto_hide.isFolded(again.properties));
		QVERIFY(auto_hide.isFolded(again.projects));
	}

	void theFoldedPanelsAreKeptForTheNextStart()
	{
		DockAutoHide::setOn(true);
		{
			Window made = makeWindow();
			DockAutoHide auto_hide(made.window.get());
			auto_hide.apply();
		}
			//The state of the window keeps them hidden: they come back
			//folded, not closed
		Window again = makeWindow();
		again.projects->hide();
		again.undo->hide();
		again.properties->hide();
		DockAutoHide auto_hide(again.window.get());
		auto_hide.apply();
		for (QDockWidget *panel : {again.projects, again.undo, again.properties}) {
			QVERIFY(auto_hide.isFolded(panel));
		}
	}

	void turningItOffShowsThemAgain()
	{
		DockAutoHide::setOn(true);
		Window made = makeWindow();
		DockAutoHide auto_hide(made.window.get());
		auto_hide.apply();
		auto_hide.open(made.undo);

		DockAutoHide::setOn(false);
		DockAutoHide::applyToAll();
		for (QDockWidget *panel : {made.projects, made.undo, made.properties}) {
			QVERIFY(!auto_hide.isFolded(panel));
			QVERIFY(!panel->isHidden());
			QVERIFY(panel->widget());
			QVERIFY(!panel->titleBarWidget());
		}
		QVERIFY(!auto_hide.isOpen(made.undo));
			//Tabbed together again
		QCOMPARE(made.window->tabifiedDockWidgets(made.projects).size(), 1);
	}
};

QET_APP_TEST_MAIN(tst_dockautohide)
#include "tst_dockautohide.moc"
