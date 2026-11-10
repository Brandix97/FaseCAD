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

#include <QList>
#include <QPointer>
#include <QString>
#include <QWidget>

class QETProject;
class QLineEdit;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;

/**
	@brief The DevicesPanel class lists the devices of the current project
	by their label : one line per label, the symbols carrying it under it
	when there are several (the terminals of a strip, the parts of a
	device drawn on several sheets).

	The list is read from the project database, so the sheets are not
	built to fill it. Folio reports and slave contacts are left out: a
	contact shows under the label of its coil. A double-click shows the
	symbol on its sheet.
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
		};

		explicit DevicesPanel(QWidget *parent = nullptr);

		void setProject(QETProject *project);
		QETProject *project() const {return m_project;}

			/// The symbols listed, sorted by label then by sheet
		static QList<Device> devices(QETProject *project);
			/// a1 < a2 < a10 : the numbers in a label compared as numbers
		static bool labelLessThan(const QString &a, const QString &b);

	protected:
		void showEvent(QShowEvent *event) override;

	private:
		void scheduleRefresh();
		void refresh();
		void showDevice(QTreeWidgetItem *item);
		void deleteSelected();
		void showContextMenu(const QPoint &pos);

		QPointer<QETProject> m_project;
		QLineEdit *m_search = nullptr;
		QTreeWidget *m_tree = nullptr;
		QTimer *m_refresh_timer = nullptr;
		QMetaObject::Connection m_database_connection;
			/// The project changed while the panel was hidden
		bool m_stale = true;
};

#endif // DEVICESPANEL_H
