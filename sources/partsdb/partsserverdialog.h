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
#ifndef PARTSSERVERDIALOG_H
#define PARTSSERVERDIALOG_H

#include "partsdatabase.h"

#include <QDialog>

class QLabel;
class QLineEdit;
class QSpinBox;

/**
	The PostgreSQL server of the parts database (Brandon, 2026-11-25):
	where it is and who connects, a test of the connection, and the parts
	of a database file copied to it.
*/
class PartsServerDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit PartsServerDialog(QWidget *parent = nullptr);

		PartsDatabase::Server server() const;
		void setServer(const PartsDatabase::Server &server);
			/// The file the copy reads, the one of this computer when empty
		void setSourceFile(const QString &path) {m_source_file = path;}

			/// Connect to the server written, and say how it went
		bool testConnection();
			/// Copy the parts of the file to the server
		int copyParts();

	private:
		QLineEdit *m_host = nullptr;
		QSpinBox *m_port = nullptr;
		QLineEdit *m_database = nullptr;
		QLineEdit *m_user = nullptr;
		QLineEdit *m_password = nullptr;
		QLabel *m_status = nullptr;
		QString m_source_file;
};

#endif // PARTSSERVERDIALOG_H
