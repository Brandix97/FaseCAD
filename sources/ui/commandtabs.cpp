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
#include "commandtabs.h"

#include <QAction>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QVBoxLayout>

namespace {

using Size = CommandTabs::Size;
using Command = CommandTabs::Command;

	///An action of the diagram editor : "diagrameditor.<name>"
QString editor(const char *name)
{
	return QStringLiteral("diagrameditor.") + QLatin1String(name);
}

Command large(const QString &id, const QString &label)
{
	return {id, label, Size::Large, {}, false};
}

Command small(const QString &id, const QString &label)
{
	return {id, label, Size::Small, {}, false};
}

Command iconOnly(const QString &id, const QString &label)
{
	return {id, label, Size::IconOnly, {}, false};
}

Command menu(const QString &id, const QString &label, Size size,
			 const QStringList &ids)
{
	return {id, label, size, ids, false};
}

Command placeholder(const QString &id, const QString &label)
{
	return {id, label, Size::Large, {}, true};
}

	///A command whose icon is a symbol drawn in black
Command symbol(Command command)
{
	command.symbol = true;
	return command;
}

	///@p icon with its dark lines in @p color, its light parts and its
	///transparency kept: a symbol drawn in black, readable on a dark theme
QIcon recolored(const QIcon &icon, const QSize &size, const QColor &color)
{
	QImage image = icon.pixmap(size).toImage()
			.convertToFormat(QImage::Format_ARGB32);
	for (int y = 0; y < image.height(); ++y) {
		auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
		for (int x = 0; x < image.width(); ++x) {
			const QRgb pixel = line[x];
			if (qAlpha(pixel) && qGray(pixel) < 128) {
				line[x] = qRgba(color.red(), color.green(), color.blue(),
								qAlpha(pixel));
			}
		}
	}
	return QIcon(QPixmap::fromImage(image));
}

	///The icon of a feature still to come : a cabinet with its rails,
	///drawn in the colour of the text so it reads in both themes
QIcon cabinetIcon(const QColor &color)
{
	QPixmap pixmap(32, 32);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setPen(QPen(color, 2));
	painter.drawRect(QRectF(5, 3, 22, 26));
	painter.setPen(QPen(color, 1.5));
	for (int y : {10, 17, 24}) {
		painter.drawLine(QPointF(8, y), QPointF(24, y));
		painter.drawRect(QRectF(10, y - 4, 4, 4));
		painter.drawRect(QRectF(17, y - 4, 4, 4));
	}
	painter.end();
	return QIcon(pixmap);
}

} // namespace

QString CommandTabs::searchCommandId()
{
	return editor("command_search");
}

/**
	@brief CommandTabs::layout
	@return the tabs, their groups and their commands, in the order they
	are shown. A command may stand on several tabs, as the most used
	ones do, but only once on a tab.
*/
QList<CommandTabs::Tab> CommandTabs::layout()
{
	const QStringList connection_symbols = {
		editor("place_corner"), editor("place_tee"),
		editor("place_tee_mirrored"), editor("place_tee_branch_left"),
		editor("place_tee_branch_right"), editor("place_interruption")};

	return {
		{tr("File"), {
			{tr("Menus"), {
				large(QStringLiteral("menu.all"), tr("Menus"))}},
			{tr("Project"), {
				large(editor("new_file"), tr("New")),
				large(editor("open_file"), tr("Open")),
				small(editor("save_file"), tr("Save")),
				small(editor("save_file_as"), tr("Save as")),
				small(editor("close_file"), tr("Close"))}},
			{tr("Print and export"), {
				large(editor("print"), tr("Print")),
				small(editor("export_to_pdf"), tr("PDF")),
				small(editor("export_to_images"), tr("Images")),
				small(editor("export_delivery"), tr("Delivery"))}},
			{tr("Properties"), {
				small(editor("project_edit_properties"), tr("Project")),
				small(editor("project_languages"), tr("Languages")),
				small(editor("project_history"), tr("History"))}}}},
		{tr("Home"), {
			{tr("Clipboard"), {
				large(editor("paste"), tr("Paste")),
				small(editor("cut"), tr("Cut")),
				small(editor("copy"), tr("Copy")),
				small(editor("delete_selection"), tr("Delete")),
				small(editor("copy_text_format"), tr("Copy format")),
				small(editor("apply_text_format"), tr("Apply format"))}},
			{tr("Sheet"), {
				large(QStringLiteral("panel.projects"), tr("Navigator")),
				small(editor("project_add_diagram"), tr("New")),
				small(editor("remove_diagram_from_project"), tr("Remove")),
				small(editor("edit_diagram_properties"), tr("Properties"))}},
			{tr("History"), {
				small(editor("undo"), tr("Undo")),
				small(editor("redo"), tr("Redo"))}},
			{tr("Text"), {
				menu(QStringLiteral("menu.text"), tr("Insert"), Size::Small,
					 {editor("add_text"), editor("place_symbol_texts"),
					  editor("text_destination")}),
				small(editor("rotate_texts"), tr("Rotate")),
				small(editor("group_selected_texts"), tr("Group"))}},
			{tr("Edit"), {
				small(editor("edit_selection"), tr("Properties")),
				small(editor("wiring_list_view"), tr("Wiring list"))}},
			{tr("Find"), {
				small(editor("find"), tr("Find")),
				small(editor("find_element"), tr("Find symbol")),
				small(editor("jump_to_element"), tr("Jump to"))}}}},
		{tr("Insert"), {
			{tr("Symbols"), {
				large(editor("show_element_picker"), tr("Navigator")),
				small(editor("insert_last_element"), tr("Last symbol"))}},
			{tr("Connection symbols"), {
				symbol(iconOnly(editor("place_corner"), tr("Corner"))),
				symbol(iconOnly(editor("place_tee"), tr("Tee"))),
				symbol(iconOnly(editor("place_tee_mirrored"), tr("Tee mirrored"))),
				symbol(iconOnly(editor("place_tee_branch_left"), tr("Branch left"))),
				symbol(iconOnly(editor("place_tee_branch_right"), tr("Branch right"))),
				symbol(iconOnly(editor("place_interruption"), tr("Interruption"))),
				symbol(iconOnly(editor("place_report_outgoing"), tr("Outgoing report"))),
				symbol(iconOnly(editor("place_report_incoming"), tr("Incoming report")))}},
			{tr("Devices"), {
				small(editor("add_generic_device"), tr("Device")),
				small(editor("add_terminal"), tr("Terminal")),
				small(editor("add_terminal_strip"), tr("Terminal strip")),
				small(editor("parts_manager"), tr("Part definition"))}},
			{tr("Cables / connections"), {
				small(editor("add_cable"), tr("Cable")),
				small(editor("add_bridge"), tr("Bridge")),
				small(editor("auto_conductor"), tr("Connection"))}},
			{tr("Text"), {
				small(editor("add_text"), tr("Text")),
				small(editor("add_image"), tr("Image")),
				small(editor("add_pdf"), tr("PDF"))}},
			{tr("Graphic"), {
				iconOnly(editor("add_line"), tr("Line")),
				iconOnly(editor("add_rectangle"), tr("Rectangle")),
				iconOnly(editor("add_ellipse"), tr("Ellipse")),
				iconOnly(editor("add_arc"), tr("Arc")),
				iconOnly(editor("add_polyline"), tr("Polyline")),
				iconOnly(editor("add_path"), tr("Path"))}},
			{tr("Dimension"), {
				large(editor("add_dimension_linear"), tr("Linear")),
				small(editor("add_dimension_chain"), tr("Chain"))}},
			{tr("Cabinet construction 2D"), {
				placeholder(QStringLiteral("placeholder.cabinet_insert"),
							tr("Cabinet\nconstruction 2D"))}}}},
		{tr("Edit"), {
			{tr("Clipboard"), {
				large(editor("paste"), tr("Paste")),
				small(editor("cut"), tr("Cut")),
				small(editor("copy"), tr("Copy")),
				small(editor("delete_selection"), tr("Delete"))}},
			{tr("Graphic"), {
				small(editor("rotate_selection"), tr("Rotate")),
				small(editor("mirror_horizontal"), tr("Mirror")),
				small(editor("mirror_vertical"), tr("Flip")),
				small(editor("duplicate"), tr("Duplicate")),
				small(editor("add_fillet"), tr("Fillet")),
				small(editor("rotate_group_selection"), tr("Rotate group"))}},
			{tr("Group"), {
				small(editor("group_selection"), tr("Group")),
				small(editor("ungroup_selection"), tr("Ungroup")),
				small(editor("group_selected_texts"), tr("Group texts"))}},
			{tr("Organize"), {
				small(editor("snap_selection_to_grid"), tr("Align to grid")),
				small(editor("connect_aligned"), tr("Connect aligned")),
				small(editor("place_symbol_texts"), tr("Place texts"))}},
			{tr("Depth"), {
				small(QStringLiteral("depth.forward"), tr("Bring to front")),
				small(QStringLiteral("depth.raise"), tr("Raise")),
				small(QStringLiteral("depth.lower"), tr("Lower")),
				small(QStringLiteral("depth.backward"), tr("Send to back"))}},
			{tr("Select"), {
				small(editor("select_all"), tr("All")),
				small(editor("select_nothing"), tr("None")),
				small(editor("select_invert"), tr("Invert")),
				small(editor("select_all_conductors"), tr("Wires")),
				small(editor("select_all_text_fields"), tr("Texts"))}}}},
		{tr("View"), {
			{tr("Mode"), {
				large(editor("mode_selection"), tr("Select")),
				small(editor("mode_visualise"), tr("Move view"))}},
			{tr("Zoom"), {
				small(editor("zoom_in"), tr("Zoom in")),
				small(editor("zoom_out"), tr("Zoom out")),
				small(editor("zoom_fit"), tr("Fit")),
				small(editor("zoom_content"), tr("Content")),
				small(editor("zoom_reset"), tr("100 %"))}},
			{tr("Grid"), {
				small(editor("draw_grid"), tr("Grid")),
				small(editor("grid_snap"), tr("Snap")),
				small(editor("draw_guides"), tr("Guides")),
				small(editor("crosshair"), tr("Crosshair")),
				small(editor("cell_lines"), tr("Cell lines")),
				small(editor("cell_rulers"), tr("Rulers"))}},
			{tr("Sheet"), {
				large(editor("dark_mode"), tr("Dark sheet"))}},
			{tr("Panels"), {
				small(QStringLiteral("panel.projects"), tr("Projects")),
				small(QStringLiteral("panel.collections"), tr("Collections")),
				small(QStringLiteral("panel.devices"), tr("Devices")),
				small(QStringLiteral("panel.undo"), tr("Undo list"))}},
			{tr("Windows"), {
				small(editor("tabbed_view_mode"), tr("Tabs")),
				small(editor("windowed_view_mode"), tr("Windows")),
				small(editor("tile_windows"), tr("Tile")),
				small(editor("cascade_windows"), tr("Cascade"))}}}},
		{tr("Devices"), {
			{tr("Devices"), {
				large(editor("parts_manager"), tr("Navigator")),
				large(editor("add_nomenclature"), tr("Bill of\nmaterials")),
				large(editor("add_parts_bom"), tr("Parts\nlist")),
				small(editor("update_elements_from_collections"),
					  tr("Update symbols")),
				small(editor("reload_element_drawings"), tr("Redraw symbols"))}},
			{tr("Terminals"), {
				large(editor("add_terminal"), tr("Terminal")),
				large(editor("terminal_strip_dialog"), tr("Navigator")),
				small(editor("terminal_numbering"), tr("Number")),
				small(editor("add_terminal_strip"), tr("Terminal strip")),
				menu(QStringLiteral("menu.terminal_extras"), tr("Extras"),
					 Size::Small,
					 {editor("terminal_strip_labels"),
					  editor("terminal_strip_bridges"),
					  editor("terminal_strip_plugin")})}},
			{tr("Cabinet construction 2D"), {
				placeholder(QStringLiteral("placeholder.cabinet_navigator"),
							tr("Navigator"))}}}},
		{tr("Connections"), {
			{tr("Wires"), {
				large(editor("auto_conductor"), tr("Automatic")),
				small(editor("auto_break_conductor"), tr("Break")),
				small(editor("conductor_reset"), tr("Reset")),
				small(editor("connect_aligned"), tr("Connect aligned"))}},
			{tr("Cables"), {
				large(editor("cable_manager"), tr("Navigator")),
				small(editor("add_cable"), tr("Cable")),
				small(editor("cable_numbering"), tr("Number")),
				small(editor("add_cable_plan"), tr("Cable plan"))}},
			{tr("Connection symbols"), {
				symbol(menu(QStringLiteral("menu.connection_symbols"),
					 tr("Symbols"), Size::Large, connection_symbols)),
				symbol(small(editor("place_report_outgoing"), tr("Outgoing report"))),
				symbol(small(editor("place_report_incoming"), tr("Incoming report")))}},
			{tr("Bridges"), {
				small(editor("add_bridge"), tr("Bridge")),
				small(editor("terminal_strip_bridges"), tr("Terminal bridges"))}}}},
		{tr("Tools"), {
			{tr("Reports"), {
				small(editor("add_summary"), tr("Summary")),
				small(editor("add_nomenclature"), tr("Bill of materials")),
				small(editor("add_parts_bom"), tr("Parts list")),
				small(editor("add_cable_list"), tr("Cable list")),
				small(editor("add_cable_plan"), tr("Cable plan"))}},
			{tr("Export"), {
				small(editor("csv_export"), tr("Bill of materials")),
				small(editor("export_parts_bom"), tr("Parts list")),
				small(editor("export_wiring_list"), tr("Wiring list")),
				small(editor("export_conductor_names"), tr("Wire names")),
				small(editor("export_cable_list"), tr("Cable list")),
				small(editor("export_cable_plan"), tr("Cable plan")),
				small(editor("export_project_db"), tr("Database"))}},
			{tr("Checks"), {
				small(editor("check_potential_types"), tr("Potentials")),
				small(editor("find_duplicate_labels"), tr("Duplicate labels")),
				small(editor("clean_project"), tr("Clean project"))}},
			{tr("Scripts"), {
				large(editor("run_script"), tr("Run")),
				small(editor("record_macro"), tr("Record")),
				small(editor("manage_scripts"), tr("Manage")),
				small(editor("open_scripts_folder"), tr("Folder"))}}}},
		{tr("Master data"), {
			{tr("Parts"), {
				large(editor("parts_manager"), tr("Parts\nmanager"))}},
			{tr("Symbols"), {
				small(editor("update_elements_from_collections"),
					  tr("Update symbols")),
				small(editor("reload_element_drawings"), tr("Redraw symbols")),
				small(QStringLiteral("panel.collections"), tr("Collections"))}},
			{tr("Project"), {
				small(editor("project_edit_properties"), tr("Properties")),
				small(editor("project_languages"), tr("Languages"))}}}}
	};
}

CommandTabs::CommandTabs(const Resolver &resolver, QWidget *parent) :
	QTabWidget(parent)
{
	setDocumentMode(true);
	for (const Tab &tab : layout()) {
		addTab(buildTab(tab, resolver), tab.title);
	}
		//Opens on Home, the tab used most, rather than on File
	setCurrentIndex(count() > 1 ? 1 : 0);
	buildCorner(resolver);

		//A folded strip opens again when a tab is clicked
	connect(tabBar(), &QTabBar::tabBarClicked, this, [this]() {
		if (m_folded) {
			setFolded(false);
		}
	});
	setFolded(QSettings().value(
			QStringLiteral("diagrameditor/command_tabs_folded"),
			false).toBool());
}

/**
	@brief CommandTabs::buildCorner
	The command search and the button folding the strip, at the right of
	the tabs.
*/
void CommandTabs::buildCorner(const Resolver &resolver)
{
	auto *corner = new QWidget(this);
	auto *layout = new QHBoxLayout(corner);
	layout->setContentsMargins(0, 0, 4, 0);
	layout->setSpacing(4);

	if (QAction *search = resolver(searchCommandId())) {
		auto *search_button = new QToolButton(corner);
		search_button->setText(tr("Search command"));
		search_button->setIcon(style()->standardIcon(
								   QStyle::SP_FileDialogContentsView));
		search_button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		search_button->setAutoRaise(false);
		search_button->setMinimumWidth(220);
		search_button->setToolTip(search->toolTip());
		connect(search_button, &QToolButton::clicked,
				search, &QAction::trigger);
		layout->addWidget(search_button);
	}

	m_fold_button = new QToolButton(corner);
	m_fold_button->setAutoRaise(true);
	m_fold_button->setToolTip(tr("Fold or unfold the tabs"));
	connect(m_fold_button, &QToolButton::clicked, this, [this]() {
		setFolded(!m_folded);
	});
	layout->addWidget(m_fold_button);

	setCornerWidget(corner, Qt::TopRightCorner);
}

/**
	@brief CommandTabs::setFolded
	Show only the titles of the tabs (true), or the buttons too.
	Remembered from one start to the next.
*/
void CommandTabs::setFolded(bool folded)
{
	m_folded = folded;
		//The pages stand in the tab widget's own stack: hiding it leaves
		//the titles
	if (auto *stack = findChild<QStackedWidget *>()) {
		stack->setVisible(!folded);
	}
	if (folded) {
		setMaximumHeight(tabBar()->sizeHint().height());
	} else {
		setMaximumHeight(QWIDGETSIZE_MAX);
	}
	m_fold_button->setArrowType(folded ? Qt::DownArrow : Qt::UpArrow);
	QSettings().setValue(QStringLiteral("diagrameditor/command_tabs_folded"),
						 folded);
}

/**
	@brief CommandTabs::buildTab
	@return the page of @p tab : its groups side by side, separated by a
	line, in a scroll area so a narrow window still reaches every button.
*/
QWidget *CommandTabs::buildTab(const Tab &tab, const Resolver &resolver)
{
	auto *content = new QWidget();
	auto *row = new QHBoxLayout(content);
	row->setContentsMargins(2, 2, 2, 2);
	row->setSpacing(4);

	bool first = true;
	for (const Group &group : tab.groups) {
		auto *group_widget = new QWidget(content);
		auto *grid = new QGridLayout(group_widget);
		grid->setContentsMargins(2, 0, 2, 0);
		grid->setHorizontalSpacing(2);
		grid->setVerticalSpacing(0);

		int column = 0;
		int small_row = 0;
		int buttons = 0;
		for (const Command &command : group.commands) {
			QAction *action = nullptr;
			if (command.placeholder) {
					//Stands for a feature still to come: a disabled
					//action of the strip's own
				action = new QAction(cabinetIcon(palette().color(
								QPalette::Disabled, QPalette::Text)),
									 command.label, this);
				action->setToolTip(tr("%1: coming soon")
								   .arg(QString(command.label)
										.replace(QLatin1Char('\n'),
												 QLatin1Char(' '))));
				action->setEnabled(false);
			} else if (!command.menu.isEmpty()) {
				auto *menu = new QMenu(this);
				for (const QString &id : command.menu) {
					if (QAction *entry = resolver(id)) {
						menu->addAction(entry);
					}
				}
				if (menu->isEmpty()) {
					delete menu;
					continue;
				}
				action = new QAction(menu->actions().first()->icon(),
									 command.label, this);
				action->setMenu(menu);
			} else {
				action = resolver(command.id);
			}
			if (!action) {
				continue;
			}

				//Made with its parent : a button without one would open as
				//a window of its own when it shows its action's state
			auto *button = new CommandButton(
						action, command.label, command.size, group_widget,
						!command.menu.isEmpty()
							|| command.id.startsWith(QLatin1String("menu.")),
						command.symbol);
			if (command.size == Size::Large) {
				if (small_row > 0) {
					++column;
					small_row = 0;
				}
				grid->addWidget(button, 0, column, 3, 1, Qt::AlignVCenter);
				++column;
			} else {
				grid->addWidget(button, small_row, column,
								Qt::AlignLeft | Qt::AlignVCenter);
				if (++small_row == 3) {
					small_row = 0;
					++column;
				}
			}
			++buttons;
		}
		if (!buttons) {
			delete group_widget;
			continue;
		}
		const int columns = small_row > 0 ? column + 1 : column;

		auto *title = new QLabel(group.title, group_widget);
		title->setAlignment(Qt::AlignCenter);
		title->setEnabled(false);
		QFont font = title->font();
		font.setPointSizeF(font.pointSizeF() * 0.9);
		title->setFont(font);
		grid->addWidget(title, 3, 0, 1, qMax(1, columns));

		if (!first) {
			auto *line = new QFrame(content);
			line->setFrameShape(QFrame::VLine);
			line->setFrameShadow(QFrame::Sunken);
			row->addWidget(line);
		}
		first = false;
		row->addWidget(group_widget);
	}
	row->addStretch();

	auto *scroll = new QScrollArea();
	scroll->setWidget(content);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	scroll->setFixedHeight(content->sizeHint().height()
						   + scroll->horizontalScrollBar()->sizeHint().height());
	return scroll;
}

/**
	@brief CommandButton::CommandButton
	@param menu_only : the action is nothing but a menu, a click opens it;
	otherwise a click triggers the action and the arrow beside the button
	opens its menu, if it has one (the variant of a connection symbol, the
	point symbol texts are placed at)
	@param symbol : see CommandTabs::Command::symbol
*/
CommandButton::CommandButton(QAction *action, const QString &label,
							 CommandTabs::Size size, QWidget *parent,
							 bool menu_only, bool symbol) :
	QToolButton(parent),
	m_action(action),
	m_symbol(symbol)
{
	switch (size) {
		case CommandTabs::Size::Large:
			setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
			setIconSize(QSize(32, 32));
			setMinimumWidth(56);
			break;
		case CommandTabs::Size::Small:
			setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
			setIconSize(QSize(16, 16));
			break;
		case CommandTabs::Size::IconOnly:
			setToolButtonStyle(Qt::ToolButtonIconOnly);
			setIconSize(QSize(16, 16));
			break;
	}
	setAutoRaise(true);
	setText(label);

	if (QMenu *menu = action->menu()) {
		setMenu(menu);
		setPopupMode(menu_only ? QToolButton::InstantPopup
							   : QToolButton::MenuButtonPopup);
	}
	if (!menu_only) {
		connect(this, &QToolButton::clicked, action, &QAction::trigger);
	}
	connect(action, &QAction::changed, this, &CommandButton::sync);
	sync();
}

/**
	@brief CommandButton::changeEvent
	A theme switched while the program runs gives the symbols the colour
	of the new text.
*/
void CommandButton::changeEvent(QEvent *event)
{
	QToolButton::changeEvent(event);
	if (m_symbol && event->type() == QEvent::PaletteChange) {
		sync();
	}
}

/**
	@brief CommandButton::sync
	Show the state of the action : icon, enabled, visible, checked, and
	its long text with its shortcut as the tooltip.
*/
void CommandButton::sync()
{
	QIcon icon = m_action->icon();
	const QPalette palette = this->palette();
	if (m_symbol && !icon.isNull()
			&& palette.color(QPalette::Window).lightness() < 128) {
		icon = recolored(icon, iconSize(), palette.color(QPalette::Text));
	}
	if (icon.isNull() && toolButtonStyle() == Qt::ToolButtonTextBesideIcon) {
			//An empty square keeps the text in line with the buttons
			//above and under it, which have an icon
		QPixmap blank(iconSize());
		blank.fill(Qt::transparent);
		icon = QIcon(blank);
	}
	setIcon(icon);
	setEnabled(m_action->isEnabled());
	setVisible(m_action->isVisible());
	setCheckable(m_action->isCheckable());
	setChecked(m_action->isChecked());

	QString tip = m_action->toolTip();
	const QKeySequence shortcut = m_action->shortcut();
	if (!shortcut.isEmpty()) {
		tip += QStringLiteral(" (%1)")
				.arg(shortcut.toString(QKeySequence::NativeText));
	}
	setToolTip(tip);
	setStatusTip(m_action->statusTip());
}
