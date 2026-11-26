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
#ifndef FASECADACCOUNT_H
#define FASECADACCOUNT_H

#include <QByteArray>
#include <QString>

class QNetworkRequest;
class QWidget;

/**
	The account of the user on fasecad.com (Brandon, 2026-11-26): the user
	name and the password the AI and the official parts catalog ask for. The password is kept
	encrypted for the user of this computer (Windows DPAPI); elsewhere it is
	only encoded.
*/
namespace FaseCadAccount
{
	QString user();
	QString password();
		/// Keeps both; an empty user forgets the account
	void set(const QString &user, const QString &password);
	bool isSet();
		/// The value of the X-FaseCAD-Acceso header: base64 of
		/// "user:password" in UTF-8; empty without an account
	QByteArray header();
		/// The session fasecad.com opened for this computer, one at a time
		/// for each account (Brandon, 2026-11-26); empty when none
	QString session();
	void setSession(const QString &session);
		/// The id of this installation, the same from one start to the
		/// next: the server tells this computer from another by it
	QString computerId();
		/// The name of this computer shown to the one who signs in
		/// elsewhere with the same account
	QString computerName();
		/// Put the account and the session into @p request
	void applyTo(QNetworkRequest &request);

	enum class Choice {Cancel, SignIn, SignOut};
		/// The sign-in window: user and password, and signing out when
		/// signed in. Keeps the user and the password on SignIn; signing
		/// in and out with the server is FaseCadServices'
	Choice askSignIn(QWidget *parent);
}

#endif // FASECADACCOUNT_H
