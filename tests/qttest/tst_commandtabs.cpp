/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/
/*
	CommandTabs -- the commands of the diagram editor in tabs grouped by
	task, another way to reach the same actions:

	* the layout has its nine tabs, no command twice on one tab, no empty
	  group, and only ids the editor resolves;
	* a button triggers the action it stands for, and follows its enabled
	  and checked state, keeping its own short label; a large button shows
	  its text under its icon, a small one beside it;
	* a command the window has no action for is left out, a group left
	  empty is not shown, and a menu holds only the commands that exist;
	* no feature still to come is shown: cabinet construction 2D is a
	  command of the Insert tab now;
	* the strip folds down to its titles and opens again.
*/

#include "ui/commandtabs.h"

#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QSet>
#include <QSettings>
#include <QSignalSpy>
#include <QtTest>

class TstCommandTabs : public QObject
{
		Q_OBJECT

	private slots:
		void initTestCase();
		void layoutHasNineTabs();
		void noCommandTwiceOnATab();
		void buttonFollowsItsAction();
		void sizesLayTheTextOut();
		void missingCommandsAreLeftOut();
		void menusHoldWhatExists();
		void noPlaceholderLeft();
		void stripFolds();
};

void TstCommandTabs::initTestCase()
{
	QCoreApplication::setOrganizationName(
				QStringLiteral("QElectroTech-tst_commandtabs"));
	QCoreApplication::setApplicationName(QStringLiteral("tst_commandtabs"));
	QSettings().clear();
}

void TstCommandTabs::layoutHasNineTabs()
{
	const QList<CommandTabs::Tab> tabs = CommandTabs::layout();
	QCOMPARE(tabs.size(), 9);
	for (const CommandTabs::Tab &tab : tabs) {
		QVERIFY(!tab.title.isEmpty());
		QVERIFY2(!tab.groups.isEmpty(), qPrintable(tab.title));
		for (const CommandTabs::Group &group : tab.groups) {
			QVERIFY2(!group.commands.isEmpty(), qPrintable(group.title));
		}
	}
}

void TstCommandTabs::noCommandTwiceOnATab()
{
	const QStringList prefixes = {
		QStringLiteral("diagrameditor."), QStringLiteral("depth."),
		QStringLiteral("panel."), QStringLiteral("menu."),
		QStringLiteral("placeholder.")};
	for (const CommandTabs::Tab &tab : CommandTabs::layout()) {
		QSet<QString> seen;
		for (const CommandTabs::Group &group : tab.groups) {
			for (const CommandTabs::Command &command : group.commands) {
				bool known = false;
				for (const QString &prefix : prefixes) {
					known = known || command.id.startsWith(prefix);
				}
				QVERIFY2(known, qPrintable(command.id));
				QVERIFY2(!command.label.isEmpty(), qPrintable(command.id));
				QVERIFY2(!seen.contains(command.id),
						 qPrintable(tab.title + QLatin1Char('/') + command.id));
				seen.insert(command.id);
			}
		}
	}
}

void TstCommandTabs::buttonFollowsItsAction()
{
	QWidget parent;
	QAction grid(QStringLiteral("Display the grid of the sheet"));
	grid.setCheckable(true);
	auto *button = new CommandButton(&grid, QStringLiteral("Grid"),
									 CommandTabs::Size::Small, &parent);

	QSignalSpy triggered(&grid, &QAction::triggered);
	button->click();
	QCOMPARE(triggered.count(), 1);
	QVERIFY(grid.isChecked());
	QVERIFY(button->isChecked());

	grid.setChecked(false);
	QVERIFY(!button->isChecked());

	grid.setEnabled(false);
	QVERIFY(!button->isEnabled());

		//The short label stays when the action changes its text
	grid.setText(QStringLiteral("Show or hide the grid"));
	QCOMPARE(button->text(), QStringLiteral("Grid"));
}

void TstCommandTabs::sizesLayTheTextOut()
{
	QWidget parent;
	QAction action(QStringLiteral("Paste"));
	CommandButton large(&action, QStringLiteral("Paste"),
						CommandTabs::Size::Large, &parent);
	CommandButton small(&action, QStringLiteral("Paste"),
						CommandTabs::Size::Small, &parent);
	CommandButton icon(&action, QStringLiteral("Paste"),
					   CommandTabs::Size::IconOnly, &parent);
	QCOMPARE(large.toolButtonStyle(), Qt::ToolButtonTextUnderIcon);
	QCOMPARE(small.toolButtonStyle(), Qt::ToolButtonTextBesideIcon);
	QCOMPARE(icon.toolButtonStyle(), Qt::ToolButtonIconOnly);
	QVERIFY(large.iconSize().width() > small.iconSize().width());
}

void TstCommandTabs::missingCommandsAreLeftOut()
{
	QAction undo(QStringLiteral("Undo"));
	QAction redo(QStringLiteral("Redo"));
	CommandTabs tabs([&](const QString &id) -> QAction * {
		if (id == QLatin1String("diagrameditor.undo")) return &undo;
		if (id == QLatin1String("diagrameditor.redo")) return &redo;
		return nullptr;
	});
	QCOMPARE(tabs.count(), 9);
		//Opens on Home, not on File
	QCOMPARE(tabs.currentIndex(), 1);

		//Undo and redo only
	int real = 0;
	for (CommandButton *button : tabs.findChildren<CommandButton *>()) {
		if (button->action() == &undo || button->action() == &redo) {
			++real;
		}
	}
	QCOMPARE(real, 2);

		//On Home, only the History group has a button: its title is the
		//only group title shown
	QStringList titles;
	for (QLabel *label : tabs.widget(1)->findChildren<QLabel *>()) {
		titles << label->text();
	}
	QCOMPARE(titles, QStringList{CommandTabs::tr("History")});
}

void TstCommandTabs::menusHoldWhatExists()
{
	QAction labels(QStringLiteral("Labels"));
	CommandTabs tabs([&](const QString &id) -> QAction * {
		return id == QLatin1String("diagrameditor.terminal_strip_labels")
				? &labels : nullptr;
	});
	bool found = false;
	for (CommandButton *button : tabs.findChildren<CommandButton *>()) {
		if (button->menu()) {
			QCOMPARE(button->menu()->actions(), QList<QAction *>{&labels});
			found = true;
		}
	}
	QVERIFY(found);
}

void TstCommandTabs::noPlaceholderLeft()
{
	for (const CommandTabs::Tab &tab : CommandTabs::layout())
		for (const CommandTabs::Group &group : tab.groups)
			for (const CommandTabs::Command &command : group.commands)
				QVERIFY2(!command.placeholder, qPrintable(command.id));
		//Without the actions of the editor, nothing is left to show
	CommandTabs tabs([](const QString &) -> QAction * {return nullptr;});
	QVERIFY(tabs.findChildren<CommandButton *>().isEmpty());
}

void TstCommandTabs::stripFolds()
{
	CommandTabs tabs([](const QString &) -> QAction * {return nullptr;});
	tabs.show();
	const int open_height = tabs.sizeHint().height();
	tabs.setFolded(true);
	QVERIFY(tabs.isFolded());
	QVERIFY(tabs.maximumHeight() < open_height);
	tabs.setFolded(false);
	QVERIFY(!tabs.isFolded());
	QCOMPARE(tabs.maximumHeight(), QWIDGETSIZE_MAX);
}

QTEST_MAIN(TstCommandTabs)
#include "tst_commandtabs.moc"
