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
#include "partsserverdialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

PartsServerDialog::PartsServerDialog(QWidget *parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Parts database server"));
	auto *layout = new QVBoxLayout(this);
	auto *intro = new QLabel(tr("A PostgreSQL server keeps the parts database for several "
								"computers at once. The tables are made the first time."), this);
	intro->setWordWrap(true);
	layout->addWidget(intro);

	auto *form = new QFormLayout();
	m_host = new QLineEdit(this);
	m_host->setObjectName(QStringLiteral("server_host"));
	m_host->setPlaceholderText(QStringLiteral("192.168.1.10"));
	m_port = new QSpinBox(this);
	m_port->setObjectName(QStringLiteral("server_port"));
	m_port->setRange(1, 65535);
	m_port->setValue(5432);
	m_database = new QLineEdit(this);
	m_database->setObjectName(QStringLiteral("server_database"));
	m_database->setPlaceholderText(QStringLiteral("fasecad"));
	m_user = new QLineEdit(this);
	m_user->setObjectName(QStringLiteral("server_user"));
	m_password = new QLineEdit(this);
	m_password->setObjectName(QStringLiteral("server_password"));
	m_password->setEchoMode(QLineEdit::Password);
	m_password->setToolTip(tr("Kept in the settings of this user on this computer"));
	form->addRow(tr("Server:"), m_host);
	form->addRow(tr("Port:"), m_port);
	form->addRow(tr("Database:"), m_database);
	form->addRow(tr("User:"), m_user);
	form->addRow(tr("Password:"), m_password);
	layout->addLayout(form);

	m_status = new QLabel(this);
	m_status->setWordWrap(true);
	m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
	layout->addWidget(m_status);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	auto *test = buttons->addButton(tr("Test the connection"), QDialogButtonBox::ActionRole);
	test->setObjectName(QStringLiteral("test_connection_pb"));
	auto *copy = buttons->addButton(tr("Copy my parts to the server..."), QDialogButtonBox::ActionRole);
	copy->setObjectName(QStringLiteral("copy_parts_pb"));
	copy->setToolTip(tr("Copies the parts of the database file, with their files, symbols and "
						"categories. A part the server has already is left as it is there."));
	connect(test, &QPushButton::clicked, this, &PartsServerDialog::testConnection);
	connect(copy, &QPushButton::clicked, this, &PartsServerDialog::copyParts);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);
}

PartsDatabase::Server PartsServerDialog::server() const
{
	PartsDatabase::Server server;
	server.host = m_host->text().trimmed();
	server.port = m_port->value();
	server.database = m_database->text().trimmed();
	server.user = m_user->text().trimmed();
	server.password = m_password->text();
	return server;
}

void PartsServerDialog::setServer(const PartsDatabase::Server &server)
{
	m_host->setText(server.host);
	m_port->setValue(server.port);
	m_database->setText(server.database);
	m_user->setText(server.user);
	m_password->setText(server.password);
}

bool PartsServerDialog::testConnection()
{
	QApplication::setOverrideCursor(Qt::WaitCursor);
	PartsDatabase database;
	QString error;
	const bool ok = database.openServer(server(), &error);
	int count = 0;
	if (ok) count = int(database.parts().size());
	QApplication::restoreOverrideCursor();
	m_status->setText(ok ? tr("Connected: the server holds %n parts.", "", count)
						 : error);
	return ok;
}

int PartsServerDialog::copyParts()
{
	const QString file = m_source_file.isEmpty() ? PartsDatabase::defaultPath() : m_source_file;
	PartsDatabase source;
	QString error;
	if (!source.open(file, &error)) {
		m_status->setText(error);
		return -1;
	}
	PartsDatabase target;
	if (!target.openServer(server(), &error)) {
		m_status->setText(error);
		return -1;
	}
	QProgressDialog progress(tr("Copying the parts to the server..."), tr("Stop"), 0, 0, this);
	progress.setWindowModality(Qt::WindowModal);
	progress.setMinimumDuration(300);
	const int copied = target.copyFrom(source, &error, [&progress](int done, int all) {
		progress.setMaximum(all);
		progress.setValue(done);
		return !progress.wasCanceled();
	});
	progress.reset();
	m_status->setText(copied < 0
					  ? error
					  : tr("%n parts copied from %1.", "", copied).arg(QDir::toNativeSeparators(file)));
	return copied;
}
