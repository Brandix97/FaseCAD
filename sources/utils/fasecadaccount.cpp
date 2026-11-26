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
#include "fasecadaccount.h"

#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSettings>
#include <QSysInfo>
#include <QUuid>

#ifdef Q_OS_WIN
#	include <windows.h>
#	include <dpapi.h>
#endif

namespace {
	const QString user_key = QStringLiteral("fasecad/account-user");
	const QString password_key = QStringLiteral("fasecad/account-password");
	const QString session_key = QStringLiteral("fasecad/account-session");
	const QString computer_key = QStringLiteral("fasecad/computer-id");

	QByteArray seal(const QByteArray &plain)
	{
#ifdef Q_OS_WIN
		DATA_BLOB in{DWORD(plain.size()), reinterpret_cast<BYTE *>(const_cast<char *>(plain.data()))};
		DATA_BLOB out{0, nullptr};
		if (CryptProtectData(&in, L"FaseCAD", nullptr, nullptr, nullptr,
							 CRYPTPROTECT_UI_FORBIDDEN, &out)) {
			const QByteArray sealed(reinterpret_cast<const char *>(out.pbData), int(out.cbData));
			LocalFree(out.pbData);
			return "dpapi:" + sealed.toBase64();
		}
#endif
		return "plain:" + plain.toBase64();
	}

	QByteArray open(const QByteArray &stored)
	{
		if (stored.startsWith("plain:"))
			return QByteArray::fromBase64(stored.mid(6));
#ifdef Q_OS_WIN
		if (stored.startsWith("dpapi:")) {
			QByteArray sealed = QByteArray::fromBase64(stored.mid(6));
			DATA_BLOB in{DWORD(sealed.size()), reinterpret_cast<BYTE *>(sealed.data())};
			DATA_BLOB out{0, nullptr};
			if (CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr,
								   CRYPTPROTECT_UI_FORBIDDEN, &out)) {
				const QByteArray plain(reinterpret_cast<const char *>(out.pbData), int(out.cbData));
				SecureZeroMemory(out.pbData, out.cbData);
				LocalFree(out.pbData);
				return plain;
			}
		}
#endif
		return QByteArray();
	}
}

QString FaseCadAccount::user()
{
	QSettings settings;
	return settings.value(user_key).toString();
}

QString FaseCadAccount::password()
{
	QSettings settings;
	return QString::fromUtf8(open(settings.value(password_key).toByteArray()));
}

void FaseCadAccount::set(const QString &user, const QString &password)
{
	QSettings settings;
		//Another account or none: the session goes
	if (user.trimmed() != settings.value(user_key).toString())
		settings.remove(session_key);
	if (user.trimmed().isEmpty()) {
		settings.remove(user_key);
		settings.remove(password_key);
		return;
	}
	settings.setValue(user_key, user.trimmed());
	settings.setValue(password_key, seal(password.toUtf8()));
}

bool FaseCadAccount::isSet()
{
	return !user().isEmpty();
}

QByteArray FaseCadAccount::header()
{
	if (!isSet()) return QByteArray();
	return (user() + QLatin1Char(':') + password()).toUtf8().toBase64();
}

QString FaseCadAccount::session()
{
	QSettings settings;
	return QString::fromUtf8(open(settings.value(session_key).toByteArray()));
}

void FaseCadAccount::setSession(const QString &session)
{
	QSettings settings;
	if (session.isEmpty())
		settings.remove(session_key);
	else
		settings.setValue(session_key, seal(session.toUtf8()));
}

QString FaseCadAccount::computerId()
{
	QSettings settings;
	QString id = settings.value(computer_key).toString();
	if (id.isEmpty()) {
		id = QUuid::createUuid().toString(QUuid::WithoutBraces);
		settings.setValue(computer_key, id);
	}
	return id;
}

QString FaseCadAccount::computerName()
{
	return QSysInfo::machineHostName();
}

void FaseCadAccount::applyTo(QNetworkRequest &request)
{
	const QByteArray access = header();
	if (access.isEmpty()) return;
	request.setRawHeader("X-FaseCAD-Acceso", access);
	const QString open_session = session();
	if (!open_session.isEmpty())
		request.setRawHeader("X-FaseCAD-Sesion", open_session.toLatin1());
}

FaseCadAccount::Choice FaseCadAccount::askSignIn(QWidget *parent)
{
	QDialog dialog(parent);
	dialog.setObjectName(QStringLiteral("fasecad_sign_in"));
	dialog.setWindowTitle(QCoreApplication::translate("FaseCadAccount", "Sign in to fasecad.com"));
	auto *form = new QFormLayout(&dialog);
	auto *intro = new QLabel(QCoreApplication::translate("FaseCadAccount", "Your FaseCAD account, for the AI and the official parts catalog."), &dialog);
	intro->setWordWrap(true);
	form->addRow(intro);
	auto *user_edit = new QLineEdit(user(), &dialog);
	user_edit->setObjectName(QStringLiteral("account_user"));
	auto *password_edit = new QLineEdit(password(), &dialog);
	password_edit->setObjectName(QStringLiteral("account_password"));
	password_edit->setEchoMode(QLineEdit::Password);
	form->addRow(QCoreApplication::translate("FaseCadAccount", "User or email:"), user_edit);
	form->addRow(QCoreApplication::translate("FaseCadAccount", "Password:"), password_edit);
		//The customer area: an account, a forgotten password, the subscription
	auto *links = new QLabel(QStringLiteral("<a href=\"%1\">%2</a> · <a href=\"%3\">%4</a> · <a href=\"%5\">%6</a>")
							 .arg(QStringLiteral("https://my.fasecad.com/registro.php"),
								  QCoreApplication::translate("FaseCadAccount", "Create an account"),
								  QStringLiteral("https://my.fasecad.com/olvido.php"),
								  QCoreApplication::translate("FaseCadAccount", "Forgot the password?"),
								  QStringLiteral("https://my.fasecad.com/"),
								  QCoreApplication::translate("FaseCadAccount", "My FaseCAD")), &dialog);
	links->setObjectName(QStringLiteral("account_links"));
	links->setOpenExternalLinks(true);
	links->setTextInteractionFlags(Qt::TextBrowserInteraction);
	form->addRow(links);
	auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	box->button(QDialogButtonBox::Ok)->setText(QCoreApplication::translate("FaseCadAccount", "Sign in"));
	bool signed_out = false;
	if (isSet()) {
		auto *out = box->addButton(QCoreApplication::translate("FaseCadAccount", "Sign out"), QDialogButtonBox::ResetRole);
		QObject::connect(out, &QPushButton::clicked, &dialog, [&dialog, &signed_out]() {
			signed_out = true;
			dialog.accept();
		});
	}
	form->addRow(box);
	QObject::connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	QObject::connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	if (user_edit->text().isEmpty()) user_edit->setFocus();
	else password_edit->setFocus();
	if (dialog.exec() != QDialog::Accepted) return Choice::Cancel;
	if (signed_out) return Choice::SignOut;
	set(user_edit->text(), password_edit->text());
	return Choice::SignIn;
}
