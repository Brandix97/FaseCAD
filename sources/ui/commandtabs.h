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
#ifndef COMMANDTABS_H
#define COMMANDTABS_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QToolButton>

#include <functional>

class QAction;
class QGridLayout;

/**
	@brief The CommandTabs class shows the commands of the diagram editor
	in tabs (Home, Insert, Edit...), each tab holding groups of buttons
	with the name of the group underneath.

	A group lays its buttons out the way ribbon toolbars do : a large
	button (big icon, text under it) takes a column of its own, small
	buttons (small icon, text beside it) are stacked three to a column.

	It is another way to reach the actions the editor already has : every
	button triggers the editor's own QAction, found by its id (the
	ShortcutManager id, or one of the ids the editor adds for its panels
	and menus), so shortcuts, enabled state and check marks stay the same
	whichever way the user works. A command this window has no action for
	is left out, and a group left empty is not shown. A placeholder is a
	greyed out button standing for a feature still to come.

	At the right of the tabs : a button opening the command search, and
	one folding the tabs down to their titles.
*/
class CommandTabs : public QTabWidget
{
	Q_OBJECT

	public:
		enum class Size {
			Large,
			Small,
				/// A small button without text, for rows of symbols
			IconOnly
		};

		/// One button : the id of the action and its label.
		struct Command {
			QString id;
			QString label;
			Size size = Size::Small;
				/// Not empty : a button opening a menu of these ids
			QStringList menu;
				/// A feature still to come : shown, never enabled
			bool placeholder = false;
				/// The icon is a symbol drawn in black : it takes the
				/// colour of the text on a dark theme
			bool symbol = false;
		};
		struct Group {
			QString title;
			QList<Command> commands;
		};
		struct Tab {
			QString title;
			QList<Group> groups;
		};

		using Resolver = std::function<QAction *(const QString &id)>;

		static QList<Tab> layout();
			/// The id of the action opening the command search
		static QString searchCommandId();

		explicit CommandTabs(const Resolver &resolver,
							 QWidget *parent = nullptr);

		bool isFolded() const {return m_folded;}
		void setFolded(bool folded);

	private:
		QWidget *buildTab(const Tab &tab, const Resolver &resolver);
		void buildCorner(const Resolver &resolver);
		int m_unfolded_height = 0;
		bool m_folded = false;
		QToolButton *m_fold_button = nullptr;
};

/**
	@brief The CommandButton class is a button of CommandTabs : it shows
	its own short label with the icon of an action, and follows the
	action's enabled, visible and checked state.

	QToolButton::setDefaultAction() would replace the short label by the
	action's long text at every change of the action, hence this class.
*/
class CommandButton : public QToolButton
{
	Q_OBJECT

	public:
		CommandButton(QAction *action, const QString &label,
					  CommandTabs::Size size, QWidget *parent = nullptr,
					  bool menu_only = false, bool symbol = false);
		QAction *action() const {return m_action;}

	protected:
		void changeEvent(QEvent *event) override;

	private:
		void sync();

		QAction *m_action;
		bool m_symbol = false;
};

#endif // COMMANDTABS_H
