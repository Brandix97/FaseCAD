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
#ifndef DEVICESPANEL_H
#define DEVICESPANEL_H

#include "../cabinet/cabinetitem.h"

#include <QList>
#include <QPair>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QWidget>

class Diagram;
class QETProject;
class QLineEdit;
class QMenu;
class QTimer;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

/**
	@brief The DevicesPanel class lists the devices of the current project
	by their label : one line per device, however many symbols carry its
	label (a PLC drawn on several sheets), with the articles of its parts
	and its sheets. The symbols without a label are no device: they share
	one line, which opens on them.

	The list is read from the project database, so the sheets are not
	built to fill it. Folio reports and slave contacts are left out: a
	contact shows under the label of its coil. A double-click shows the
	device's symbol made of parts (or its first symbol) on its sheet.

	Devices can be drawn on a cabinet sheet from the context menu: the DXF
	of each one's parts (or a DXF file chosen) follows the mouse there in
	turn, with its label in the middle; a click puts it down and brings
	the next one. A device drawn on a cabinet sheet has a green tick.

	"Number..." renames the selected devices one after the other from a
	first label (-K1, -K2...), in the order they stand on the sheets.
*/
class DevicesPanel : public QWidget
{
	Q_OBJECT

	public:
			/// One symbol of the list, as the database gives it
		struct Device {
			QString uuid;
			QString diagram_uuid;
			QString label;
			QString description;
			int folio = 0;			///< 1 for the first sheet
			QString cell;			///< e.g. C3
				/// The parts it is made of: its information "parts"
				/// (PartsLinks)
			QString parts;
		};

			/// One line of the list: a device and the symbols carrying
			/// its label, or every symbol without a label
		struct Line {
			QString label;
			QString description;
				/// The order numbers of its parts, each once
			QString article;
				/// "1-E11" for one symbol, "1, 2, 3, 4" for several
			QString sheets;
			QList<Device> symbols;
				/// The symbol the line stands for: the first one made of
				/// parts, else the first one
			int main = 0;
		};

		explicit DevicesPanel(QWidget *parent = nullptr);

		void setProject(QETProject *project);
		QETProject *project() const {return m_project;}

			/// The symbols listed, sorted by label then by sheet
		static QList<Device> devices(QETProject *project);
			/// @p devices (sorted as devices() sorts them) as lines
		static QList<Line> lines(const QList<Device> &devices);
			/// a1 < a2 < a10 : the numbers in a label compared as numbers
		static bool labelLessThan(const QString &a, const QString &b);
			/// @p count labels from @p first, its last number going up
			/// by @p step: -K1, -K2...; -K08, -K09, -K10; -X0:1, -X0:2
		static QStringList numberedLabels(const QString &first, int count, int step = 1);
			/// Where @p a stands before @p b on the sheets: sheet, then
			/// column, then row
		static bool positionLessThan(const Device &a, const Device &b);

	protected:
		void showEvent(QShowEvent *event) override;

	private:
			/// A device waiting for its drawing to be placed
		struct Pending {
			QString label;
				/// Its symbols, as (uuid, folio uuid)
			QList<QPair<QString, QString>> symbols;
		};

		void scheduleRefresh();
		void refresh();
		void showDevice(QTreeWidgetItem *item);
		void deleteSelected();
		void showContextMenu(const QPoint &pos);
		void addDrawingActions(QMenu &menu);
		void insertDrawings(Diagram *folio);
		void placeNextDrawing();
		bool drawingOf(const Pending &device, QUuid &symbol,
					   CabinetItem::Properties &properties);
		void numberSelected();
		QList<QTreeWidgetItem *> selectedLines() const;

		QPointer<QETProject> m_project;
		QLineEdit *m_search = nullptr;
		QToolButton *m_number_button = nullptr;
		QTreeWidget *m_tree = nullptr;
		QTimer *m_refresh_timer = nullptr;
			/// The devices whose drawings are placed one after the other,
			/// on that cabinet sheet
		QList<Pending> m_queue;
		QPointer<Diagram> m_queue_folio;
		int m_queue_total = 0;
		int m_queue_position = 0;
		QMetaObject::Connection m_database_connection;
			/// Placing or deleting a drawing on a cabinet sheet, and undo
		QMetaObject::Connection m_undo_connection;
			/// The project changed while the panel was hidden
		bool m_stale = true;
};

#endif // DEVICESPANEL_H
