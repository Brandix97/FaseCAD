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
#include "fasecadservices.h"

#include "fasecadaccount.h"
#include "../qetversion.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QLocale>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSysInfo>
#include <QTimer>
#include <QUrlQuery>

namespace {
	QUrl server_url(QStringLiteral("https://api.fasecad.com/cuenta.php"));

	QString tr(const char *text)
	{
		return QCoreApplication::translate("FaseCadServices", text);
	}
}

FaseCadServices::FaseCadServices(QObject *parent) :
	QObject(parent),
	m_network(new QNetworkAccessManager(this)),
	m_keep_alive(new QTimer(this))
{
	m_keep_alive->setInterval(keepAliveInterval());
	connect(m_keep_alive, &QTimer::timeout, this, [this]() {
		if (m_signed_in) checkAccount();
	});
}

QNetworkReply *FaseCadServices::post(const QString &query, const QByteArray &body)
{
	QNetworkRequest request(urlFor(query));
	request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("FaseCAD"));
	request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
	FaseCadAccount::applyTo(request);
	request.setTransferTimeout(30000);
	return m_network->post(request, body);
}

void FaseCadServices::setSignedOut()
{
	m_signed_in = m_subscribed = false;
	m_until = QDate();
	m_keep_alive->stop();
}

FaseCadServices *FaseCadServices::instance()
{
	static FaseCadServices *services = new FaseCadServices(qApp);
	return services;
}

QUrl FaseCadServices::serverUrl()
{
	return server_url;
}

void FaseCadServices::setServerUrl(const QUrl &url)
{
	server_url = url;
}

QUrl FaseCadServices::urlFor(const QString &query) const
{
	QUrl url = server_url;
	url.setQuery(QUrlQuery(query));
	return url;
}

void FaseCadServices::refresh()
{
	QNetworkRequest request(urlFor(QStringLiteral("estado")));
	request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("FaseCAD"));
	request.setTransferTimeout(15000);
	QNetworkReply *reply = m_network->get(request);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		const QJsonObject answer = QJsonDocument::fromJson(reply->readAll()).object();
			//Off when the server cannot be reached or does not say on
		m_service_on = reply->error() == QNetworkReply::NoError
				&& answer.value(QStringLiteral("servicio")).toBool();
		if (m_service_on && FaseCadAccount::isSet()) {
				//The session kept from the last time, or a new one when the
				//account is open nowhere else
			if (FaseCadAccount::session().isEmpty()) signIn(false);
			else checkAccount();
			return;
		}
		setSignedOut();
		emit changed();
	});
}

void FaseCadServices::checkAccount()
{
	if (!m_service_on || !FaseCadAccount::isSet() || FaseCadAccount::session().isEmpty()) {
		setSignedOut();
		m_account_error.clear();
		emit changed();
		return;
	}
	QNetworkReply *reply = post(QStringLiteral("yo"), QByteArray("{}"));
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		const QJsonObject answer = QJsonDocument::fromJson(reply->readAll()).object();
		if (status == 409) {
			sessionClosed();
			return;
		}
		if (status == 200) {
			m_signed_in = true;
			m_subscribed = answer.value(QStringLiteral("suscripcion")).toBool();
			m_until = QDate::fromString(answer.value(QStringLiteral("hasta")).toString(), Qt::ISODate);
			m_account_error.clear();
			m_keep_alive->start();
		} else if (status == 401) {
			setSignedOut();
			m_account_error = tr("The user or the password is wrong.");
		} else if (!m_signed_in) {
			m_account_error = tr("fasecad.com cannot be reached: %1").arg(reply->errorString());
		}
			//Signed in and the server out of reach for a while: still
			//signed in, tried again at the next keep-alive
		emit changed();
	});
}

void FaseCadServices::signIn(bool take)
{
	const QJsonObject body{
		{QStringLiteral("equipo"), FaseCadAccount::computerId()},
		{QStringLiteral("nombre"), FaseCadAccount::computerName()},
		{QStringLiteral("forzar"), take}};
	FaseCadAccount::setSession(QString());
	QNetworkReply *reply = post(QStringLiteral("entrar"), QJsonDocument(body).toJson(QJsonDocument::Compact));
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		const QJsonObject answer = QJsonDocument::fromJson(reply->readAll()).object();
		SignIn result = SignIn::Unreachable;
		QString computer;
		QDateTime since;
		setSignedOut();
		if (status == 200 && !answer.value(QStringLiteral("sesion")).toString().isEmpty()) {
			FaseCadAccount::setSession(answer.value(QStringLiteral("sesion")).toString());
			m_service_on = true;
			m_signed_in = true;
			m_subscribed = answer.value(QStringLiteral("suscripcion")).toBool();
			m_until = QDate::fromString(answer.value(QStringLiteral("hasta")).toString(), Qt::ISODate);
			m_account_error.clear();
			m_keep_alive->start();
			result = SignIn::Done;
		} else if (status == 409) {
			result = SignIn::Busy;
			computer = answer.value(QStringLiteral("equipo")).toString();
			since = QDateTime::fromString(answer.value(QStringLiteral("desde")).toString(), Qt::ISODate);
			m_account_error = tr("Your account is open on another computer.");
		} else if (status == 401) {
			result = SignIn::Wrong;
			m_account_error = tr("The user or the password is wrong.");
		} else if (status == 503) {
			result = SignIn::Closed;
			m_account_error = tr("The services of fasecad.com are not open yet.");
		} else {
			m_account_error = tr("fasecad.com cannot be reached: %1").arg(reply->errorString());
		}
		emit changed();
		emit signInDone(result, computer, since);
	});
}

void FaseCadServices::signOut()
{
		//Told to the server when it can be; this computer forgets anyway
	if (!FaseCadAccount::session().isEmpty()) {
		QNetworkReply *reply = post(QStringLiteral("salir"), QByteArray("{}"));
		connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
	}
	FaseCadAccount::setSession(QString());
	FaseCadAccount::set(QString(), QString());
	setSignedOut();
	m_account_error.clear();
	emit changed();
}

void FaseCadServices::sessionClosed()
{
	const bool was = m_signed_in || !FaseCadAccount::session().isEmpty();
	FaseCadAccount::setSession(QString());
	setSignedOut();
	m_account_error = tr("Your account was opened on another computer: this session was closed.");
	emit changed();
	if (was) emit sessionLost();
}

void FaseCadServices::send(Kind kind, const QString &subject, const QString &text)
{
	const QJsonObject body{
		{QStringLiteral("tipo"), kind == Kind::Bug ? QStringLiteral("error") : QStringLiteral("soporte")},
		{QStringLiteral("asunto"), subject.trimmed()},
		{QStringLiteral("texto"), text.trimmed()},
		{QStringLiteral("version"), QetVersion::productVersion()},
		{QStringLiteral("sistema"), QSysInfo::prettyProductName()},
		{QStringLiteral("idioma"), QLocale().name()}};
	QNetworkReply *reply = post(QStringLiteral("soporte"), QJsonDocument(body).toJson(QJsonDocument::Compact));
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (status == 200) {
			emit sent(true, QString());
			return;
		}
		QString message;
		if (status == 409) {
			sessionClosed();
			message = tr("Your account was opened on another computer: this session was closed.");
		} else if (status == 401)
			message = tr("The user or the password is wrong.");
		else if (status == 402)
			message = tr("Your subscription is not running.");
		else if (status == 429)
			message = tr("Too many messages today: try again tomorrow.");
		else
			message = tr("The message could not be sent: %1").arg(reply->errorString());
		emit sent(false, message);
	});
}
