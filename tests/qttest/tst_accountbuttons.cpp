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
#include "qetapptest.h"
#include "qetdiagrameditor.h"
#include "ui/accountbuttons.h"
#include "utils/fasecadaccount.h"
#include "utils/fasecadservices.h"
#include "utils/qetsettings.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>
#include <QToolButton>

/**
	The buttons of the account by the command tabs (Brandon, 2026-11-26):
	"Sign in" grey while the web service is off, "Report a bug" and
	"Request support" only for an account with a subscription, one computer
	at a time for each account, and the message sent. The server here
	answers as cuenta.php of the site does.
*/
class tst_accountbuttons : public QObject
{
	Q_OBJECT

	QTcpServer m_server;
	bool m_service = false;
	bool m_subscription = false;
		/// The session open on the server, and the computer that has it
	QByteArray m_session;
	QString m_computer;
	QString m_computer_name;
	QList<QByteArray> m_heads;
	QList<QJsonObject> m_bodies;

	static QByteArray header(const QByteArray &head, const QByteArray &name)
	{
		for (const QByteArray &line : head.split('\n'))
			if (line.toLower().startsWith(name.toLower() + ':'))
				return line.mid(name.size() + 1).trimmed();
		return QByteArray();
	}

	void answer(QTcpSocket *socket)
	{
		QByteArray &request = *static_cast<QByteArray *>(socket->property("buffer").value<void *>());
		request += socket->readAll();
		const int head_end = int(request.indexOf("\r\n\r\n"));
		if (head_end < 0) return;
		const QByteArray head = request.left(head_end);
		const int length = header(head, "Content-Length").toInt();
		if (request.size() < head_end + 4 + length) return;
		const QJsonObject asked = QJsonDocument::fromJson(request.mid(head_end + 4, length)).object();
		m_heads << head;
		m_bodies << asked;
		const QByteArray first = head.left(head.indexOf('\r'));
		const bool good = header(head, "X-FaseCAD-Acceso")
				== QByteArray("taller:clave-del-taller").toBase64();
		const bool session = !m_session.isEmpty() && header(head, "X-FaseCAD-Sesion") == m_session;
		int code = 200;
		QJsonObject body;
		if (first.contains("?estado")) {
			body = {{QStringLiteral("servicio"), m_service}};
		} else if (!m_service) {
			code = 503;
		} else if (!good) {
			code = 401;
		} else if (first.contains("?entrar")) {
			const QString computer = asked.value(QStringLiteral("equipo")).toString();
			if (!m_session.isEmpty() && m_computer != computer && !asked.value(QStringLiteral("forzar")).toBool()) {
				code = 409;
				body = {{QStringLiteral("ocupada"), true}, {QStringLiteral("equipo"), m_computer_name},
						{QStringLiteral("desde"), QStringLiteral("2026-11-26T08:00:00+00:00")}};
			} else {
				m_session = QByteArray::number(QRandomGenerator::global()->generate64(), 16);
				m_computer = computer;
				m_computer_name = asked.value(QStringLiteral("nombre")).toString();
				body = {{QStringLiteral("sesion"), QString::fromLatin1(m_session)},
						{QStringLiteral("usuario"), QStringLiteral("taller")},
						{QStringLiteral("suscripcion"), m_subscription},
						{QStringLiteral("hasta"), m_subscription ? QStringLiteral("2027-11-26") : QString()}};
			}
		} else if (first.contains("?salir")) {
			if (session) m_session.clear();
			body = {{QStringLiteral("ok"), true}};
		} else if (!session) {
			code = 409;
		} else if (first.contains("?yo")) {
			body = {{QStringLiteral("usuario"), QStringLiteral("taller")},
					{QStringLiteral("suscripcion"), m_subscription},
					{QStringLiteral("hasta"), m_subscription ? QStringLiteral("2027-11-26") : QString()}};
		} else if (first.contains("?soporte")) {
			if (!m_subscription) code = 402;
			else body = {{QStringLiteral("ok"), true}};
		}
		const QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);
		socket->write("HTTP/1.1 " + QByteArray::number(code) + " X\r\nContent-Type: application/json\r\n"
					  "Content-Length: " + QByteArray::number(data.size()) + "\r\nConnection: close\r\n\r\n" + data);
		socket->disconnectFromHost();
	}

	bool waitFor(const std::function<void()> &ask)
	{
		QSignalSpy changed(FaseCadServices::instance(), &FaseCadServices::changed);
		ask();
		return !changed.isEmpty() || changed.wait(10000);
	}

	FaseCadServices::SignIn signIn(bool take = false)
	{
		QSignalSpy done(FaseCadServices::instance(), &FaseCadServices::signInDone);
		FaseCadServices::instance()->signIn(take);
		if (!done.wait(10000)) return FaseCadServices::SignIn::Unreachable;
		return done.first().first().value<FaseCadServices::SignIn>();
	}

	void signedOutEverywhere()
	{
		FaseCadAccount::set(QString(), QString());
		FaseCadAccount::setSession(QString());
		m_session.clear();
		m_computer.clear();
	}

private slots:
	void initTestCase()
	{
		qRegisterMetaType<FaseCadServices::SignIn>();
		QVERIFY(m_server.listen(QHostAddress::LocalHost));
		connect(&m_server, &QTcpServer::newConnection, this, [this]() {
			while (QTcpSocket *socket = m_server.nextPendingConnection()) {
				auto *buffer = new QByteArray;
				socket->setProperty("buffer", QVariant::fromValue(static_cast<void *>(buffer)));
				connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {answer(socket);});
				connect(socket, &QTcpSocket::disconnected, socket, [socket, buffer]() {
					delete buffer;
					socket->deleteLater();
				});
			}
		});
		FaseCadServices::setServerUrl(QUrl(QStringLiteral("http://127.0.0.1:%1/cuenta.php")
										   .arg(m_server.serverPort())));
		signedOutEverywhere();
	}

	void theButtonsStandByTheCommandSearch()
	{
		QETDiagramEditor *editor = QetAppTest::editor();
		QVERIFY(editor);
			//With the command tabs
		QetSettings::setCommandTabs(true);
		QMetaObject::invokeMethod(editor, "updateCommandTabs");
		QVERIFY(editor->findChild<QToolButton *>(QStringLiteral("account_sign_in")));
		QVERIFY(editor->findChild<QToolButton *>(QStringLiteral("account_report_bug")));
		QVERIFY(editor->findChild<QToolButton *>(QStringLiteral("account_request_support")));
	}

	void theOfficialCatalogWaitsForTheService()
	{
		QETDiagramEditor *editor = QetAppTest::editor();
		QVERIFY(editor);
		QAction *catalog = nullptr;
		for (QAction *action : editor->findChildren<QAction *>())
			if (action->text() == QStringLiteral("Official parts catalog..."))
				catalog = action;
		QVERIFY(catalog);
		FaseCadServices *services = FaseCadServices::instance();

		m_service = false;
		QVERIFY(waitFor([services]() {services->refresh();}));
		QVERIFY(!catalog->isEnabled());

		m_service = true;
		QVERIFY(waitFor([services]() {services->refresh();}));
		QVERIFY(catalog->isEnabled());
		m_service = false;
		QVERIFY(waitFor([services]() {services->refresh();}));
	}

	void signingInWaitsForTheService()
	{
		signedOutEverywhere();
		AccountButtons buttons;
		auto *sign_in = buttons.findChild<QToolButton *>(QStringLiteral("account_sign_in"));
		auto *bug = buttons.findChild<QToolButton *>(QStringLiteral("account_report_bug"));
		auto *support = buttons.findChild<QToolButton *>(QStringLiteral("account_request_support"));
		FaseCadServices *services = FaseCadServices::instance();

		m_service = false;
		QVERIFY(waitFor([services]() {services->refresh();}));
		QVERIFY(!sign_in->isEnabled());
		QVERIFY(!bug->isEnabled());
		QVERIFY(!support->isEnabled());

		m_service = true;
		QVERIFY(waitFor([services]() {services->refresh();}));
		QVERIFY(sign_in->isEnabled());
		QVERIFY(!bug->isEnabled());

			//Signed in, no subscription: a session for this computer
		FaseCadAccount::set(QStringLiteral("taller"), QStringLiteral("clave-del-taller"));
		m_subscription = false;
		QCOMPARE(signIn(), FaseCadServices::SignIn::Done);
		QVERIFY(services->signedIn());
		QCOMPARE(FaseCadAccount::session().toLatin1(), m_session);
		QCOMPARE(m_computer, FaseCadAccount::computerId());
		QCOMPARE(sign_in->text(), QStringLiteral("taller"));
		QVERIFY(!bug->isEnabled());
		QVERIFY(!support->isEnabled());

			//With a subscription; the session goes with each request
		m_subscription = true;
		QVERIFY(waitFor([services]() {services->refresh();}));
		QCOMPARE(header(m_heads.last(), "X-FaseCAD-Sesion"), m_session);
		QVERIFY(bug->isEnabled());
		QVERIFY(support->isEnabled());
		QCOMPARE(services->subscribedUntil(), QDate(2027, 11, 26));

			//A wrong password
		FaseCadAccount::set(QStringLiteral("taller"), QStringLiteral("otra"));
		QCOMPARE(signIn(), FaseCadServices::SignIn::Wrong);
		QVERIFY(!services->signedIn());
		QVERIFY(!bug->isEnabled());
		signedOutEverywhere();
	}

	void oneComputerAtATime()
	{
		signedOutEverywhere();
		m_service = m_subscription = true;
		FaseCadServices *services = FaseCadServices::instance();
		QVERIFY(waitFor([services]() {services->refresh();}));
		FaseCadAccount::set(QStringLiteral("taller"), QStringLiteral("clave-del-taller"));
		QCOMPARE(signIn(), FaseCadServices::SignIn::Done);

			//Someone signs in on another computer: this session is closed,
			//noticed at the next check, the account kept to sign in again
		m_session = "de-la-oficina";
		m_computer = QStringLiteral("otro-equipo");
		m_computer_name = QStringLiteral("PC-OFICINA");
		QSignalSpy lost(services, &FaseCadServices::sessionLost);
		QVERIFY(waitFor([services]() {services->checkAccount();}));
		QCOMPARE(lost.size(), 1);
		QVERIFY(!services->signedIn());
		QVERIFY(FaseCadAccount::session().isEmpty());
		QVERIFY(FaseCadAccount::isSet());

			//Signing in here again: the account is in use there
		QSignalSpy done(services, &FaseCadServices::signInDone);
		services->signIn(false);
		QVERIFY(done.wait(10000));
		QCOMPARE(done.first().at(0).value<FaseCadServices::SignIn>(), FaseCadServices::SignIn::Busy);
		QCOMPARE(done.first().at(1).toString(), QStringLiteral("PC-OFICINA"));
		QVERIFY(done.first().at(2).toDateTime().isValid());
		QVERIFY(!services->signedIn());
		QCOMPARE(m_session, QByteArray("de-la-oficina"));

			//... taken over: closed there, open here
		QCOMPARE(signIn(true), FaseCadServices::SignIn::Done);
		QVERIFY(services->signedIn());
		QCOMPARE(m_computer, FaseCadAccount::computerId());
		QCOMPARE(FaseCadAccount::session().toLatin1(), m_session);

			//Signing out frees the account on the server
		QVERIFY(waitFor([services]() {services->signOut();}));
		QTRY_VERIFY(m_session.isEmpty());
		QVERIFY(!FaseCadAccount::isSet());
		QVERIFY(!services->signedIn());
	}

	void theSignInWindowAsksBeforeTakingOver()
	{
		signedOutEverywhere();
		m_service = true;
		FaseCadServices *services = FaseCadServices::instance();
		QVERIFY(waitFor([services]() {services->refresh();}));
		m_session = "de-la-oficina";
		m_computer = QStringLiteral("otro-equipo");
		m_computer_name = QStringLiteral("PC-OFICINA");
		QString asked_about;
		bool answer = false;
		const auto was = AccountButtons::askTakeOver;
		AccountButtons::askTakeOver = [&](QWidget *, const QString &computer, const QDateTime &) {
			asked_about = computer;
			return answer;
		};
			//The sign-in window, filled and accepted
		auto fill = []() {
			QTimer::singleShot(0, []() {
				for (QWidget *top : QApplication::topLevelWidgets()) {
					if (top->objectName() != QLatin1String("fasecad_sign_in") || !top->isVisible()) continue;
					top->findChild<QLineEdit *>(QStringLiteral("account_user"))->setText(QStringLiteral("taller"));
					top->findChild<QLineEdit *>(QStringLiteral("account_password"))
							->setText(QStringLiteral("clave-del-taller"));
					static_cast<QDialog *>(top)->accept();
				}
			});
		};
		fill();
		QVERIFY(AccountButtons::signIn(nullptr));
		QCOMPARE(asked_about, QStringLiteral("PC-OFICINA"));
		QVERIFY(!services->signedIn());
		QCOMPARE(m_session, QByteArray("de-la-oficina"));

		answer = true;
		fill();
		QVERIFY(AccountButtons::signIn(nullptr));
		QVERIFY(services->signedIn());
		QCOMPARE(m_computer, FaseCadAccount::computerId());
		AccountButtons::askTakeOver = was;
		signedOutEverywhere();
	}

	void aBugReportIsSent()
	{
		signedOutEverywhere();
		FaseCadAccount::set(QStringLiteral("taller"), QStringLiteral("clave-del-taller"));
		m_service = m_subscription = true;
		FaseCadServices *services = FaseCadServices::instance();
			//No session kept: one opened quietly
		QSignalSpy done(services, &FaseCadServices::signInDone);
		services->refresh();
		QVERIFY(done.wait(10000));
		QVERIFY(services->signedIn());
			//The window: Send once a subject and a text are written
		{
			SupportDialog dialog(FaseCadServices::Kind::Bug);
			auto *box = dialog.findChild<QDialogButtonBox *>();
			QPushButton *send = nullptr;
			for (QAbstractButton *each : box->buttons())
				if (box->buttonRole(each) == QDialogButtonBox::AcceptRole)
					send = qobject_cast<QPushButton *>(each);
			QVERIFY(send);
			QVERIFY(!send->isEnabled());
			dialog.findChild<QLineEdit *>(QStringLiteral("support_subject"))->setText(QStringLiteral("Se cierra"));
			QVERIFY(!send->isEnabled());
			dialog.findChild<QPlainTextEdit *>(QStringLiteral("support_text"))
					->setPlainText(QStringLiteral("Al abrir el 3D."));
			QVERIFY(send->isEnabled());
		}
		m_bodies.clear();
		QSignalSpy sent(services, &FaseCadServices::sent);
		services->send(FaseCadServices::Kind::Bug, QStringLiteral("Se cierra"), QStringLiteral("Al abrir el 3D."));
		QVERIFY(sent.wait(10000));
		QVERIFY(sent.first().first().toBool());
		const QJsonObject body = m_bodies.last();
		QCOMPARE(body.value(QStringLiteral("tipo")).toString(), QStringLiteral("error"));
		QCOMPARE(body.value(QStringLiteral("asunto")).toString(), QStringLiteral("Se cierra"));
		QVERIFY(!body.value(QStringLiteral("version")).toString().isEmpty());

			//Without a subscription the server refuses
		m_subscription = false;
		sent.clear();
		services->send(FaseCadServices::Kind::Support, QStringLiteral("Ayuda"), QStringLiteral("¿Cómo?"));
		QVERIFY(sent.wait(10000));
		QVERIFY(!sent.first().first().toBool());
		QVERIFY(sent.first().at(1).toString().contains(QStringLiteral("subscription")));
		signedOutEverywhere();
	}
};

QET_APP_TEST_MAIN(tst_accountbuttons)
#include "tst_accountbuttons.moc"
