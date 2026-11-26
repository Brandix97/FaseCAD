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
#ifndef FASECADSERVICES_H
#define FASECADSERVICES_H

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

/**
	The services of fasecad.com for the account of the user (Brandon,
	2026-11-26), cuenta.php: whether the web service is on (signing in is
	offered once it is), whether the account signed in has a subscription
	(reporting a bug and asking for support are for those who have one),
	and the messages sent for these.

	One computer at a time for each account: signing in opens the session of
	this computer on the server, sent with every request after
	(FaseCadAccount::applyTo); signing in elsewhere closes it. The session
	is kept alive every few minutes, and noticed closed then.
*/
class FaseCadServices : public QObject
{
	Q_OBJECT

	public:
		static FaseCadServices *instance();
			/// The address of cuenta.php (fixed for the user; the tests
			/// change it)
		static QUrl serverUrl();
		static void setServerUrl(const QUrl &url);

			/// The web service is on: signing in is offered
		bool serviceOn() const {return m_service_on;}
			/// The account signed in is known to the server
		bool signedIn() const {return m_signed_in;}
			/// ... and has a subscription running
		bool subscribed() const {return m_subscribed;}
		QDate subscribedUntil() const {return m_until;}
			/// Why the last check of the account failed, empty when it
			/// did not
		QString accountError() const {return m_account_error;}

			/// Ask the server whether the service is on, then about the
			/// account and its session; changed() when known
		void refresh();
			/// Ask about the account and its session only
		void checkAccount();

		enum class SignIn {Done, Busy, Wrong, Closed, Unreachable};
		Q_ENUM(SignIn)
			/// Open the session of this computer with the user and the
			/// password kept; @p take: even when the account is open on
			/// another computer, closing it there. signedIn() when done
		void signIn(bool take = false);
			/// Close the session and forget the account
		void signOut();
			/// The server said the session is closed: signed out, the
			/// account kept to sign in again; sessionLost()
		void sessionClosed();
			/// Every how long the session is kept alive, ms
		static int keepAliveInterval() {return 5 * 60 * 1000;}

		enum class Kind {Bug, Support};
			/// Send a bug report or a request for support; sent() when done
		void send(Kind kind, const QString &subject, const QString &text);

	signals:
		void changed();
			/// The answer of signIn(); for Busy, @p computer is where the
			/// account is open and @p since since when
		void signInDone(FaseCadServices::SignIn result, const QString &computer,
						const QDateTime &since);
			/// The session of this computer was closed by signing in on
			/// another one
		void sessionLost();
			/// @p ok false: @p message says why
		void sent(bool ok, const QString &message);

	private:
		explicit FaseCadServices(QObject *parent = nullptr);
		QUrl urlFor(const QString &query) const;
		QNetworkReply *post(const QString &query, const QByteArray &body);
		void setSignedOut();

		QNetworkAccessManager *m_network = nullptr;
		QTimer *m_keep_alive = nullptr;
		bool m_service_on = false;
		bool m_signed_in = false;
		bool m_subscribed = false;
		QDate m_until;
		QString m_account_error;
};

#endif // FASECADSERVICES_H
