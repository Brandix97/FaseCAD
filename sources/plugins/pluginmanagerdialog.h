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
#ifndef PLUGINMANAGERDIALOG_H
#define PLUGINMANAGERDIALOG_H

#include <QDialog>

class QLabel;
class QPushButton;
class QTreeWidget;

/**
	@brief The PluginManagerDialog class lists the plugins installed, with
	their version, author and state, and installs, switches on or off and
	removes them. Installing shows what the plugin asks for first.
*/
class PluginManagerDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit PluginManagerDialog(QWidget *parent = nullptr);

			/// Install the .fasplugin @p path, asking first; for the tests
			/// and for a file dropped on FaseCAD
		bool installFile(const QString &path, bool ask = true);

	private:
		void fill();
		void showDetails();
		QString selectedId() const;

		QTreeWidget *m_list = nullptr;
		QLabel *m_details = nullptr;
		QPushButton *m_remove = nullptr;
		bool m_filling = false;
};

#endif // PLUGINMANAGERDIALOG_H
