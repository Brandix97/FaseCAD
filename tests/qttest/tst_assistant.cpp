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
#include "qetproject.h"
#include "diagram.h"
#include "diagramview.h"
#include "qetdiagrameditor.h"
#include "qetgraphicsitem/independenttextitem.h"
#include "scripting/assistantpanel.h"
#include "scripting/assistantsession.h"
#include "utils/fasecadlicence.h"
#include "utils/fasecadaccount.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QToolButton>

/**
	The "IA" panel (Brandon, 2026-11-25): an instruction sent to the AI
	service, the script it answers with run on the open project as one
	undo step, the result sent back with the conversation as it came, its
	text shown. The service here answers as ia.php does, streaming the
	events of the Claude API.
*/
class tst_assistant : public QObject
{
	Q_OBJECT

	QTcpServer m_server;
	QList<QJsonObject> m_bodies;
	QList<QByteArray> m_headers;
	QList<QByteArray> m_answers;	///< one per request, as the service streams it
	int m_status = 200;

	static QByteArray event(const QString &name, const QJsonObject &data)
	{
		return "event: " + name.toUtf8() + "\ndata: "
				+ QJsonDocument(data).toJson(QJsonDocument::Compact) + "\n\n";
	}

	static QByteArray toolCall(const QString &id, const QString &title, const QString &code)
	{
		const QByteArray input = QJsonDocument(QJsonObject{{QStringLiteral("title"), title},
														   {QStringLiteral("code"), code}}).toJson(QJsonDocument::Compact);
		QByteArray stream = event(QStringLiteral("message_start"), QJsonObject{
			{QStringLiteral("type"), QStringLiteral("message_start")},
			{QStringLiteral("message"), QJsonObject{{QStringLiteral("id"), QStringLiteral("msg_1")},
													{QStringLiteral("role"), QStringLiteral("assistant")},
													{QStringLiteral("content"), QJsonArray()}}}});
			//Its thinking, the text not shown, its signature
		stream += event(QStringLiteral("content_block_start"), QJsonObject{
			{QStringLiteral("type"), QStringLiteral("content_block_start")}, {QStringLiteral("index"), 0},
			{QStringLiteral("content_block"), QJsonObject{{QStringLiteral("type"), QStringLiteral("thinking")},
														  {QStringLiteral("thinking"), QString()},
														  {QStringLiteral("signature"), QString()}}}});
		stream += event(QStringLiteral("content_block_delta"), QJsonObject{
			{QStringLiteral("type"), QStringLiteral("content_block_delta")}, {QStringLiteral("index"), 0},
			{QStringLiteral("delta"), QJsonObject{{QStringLiteral("type"), QStringLiteral("signature_delta")},
												  {QStringLiteral("signature"), QStringLiteral("SIG")}}}});
		stream += event(QStringLiteral("content_block_stop"), QJsonObject{
			{QStringLiteral("type"), QStringLiteral("content_block_stop")}, {QStringLiteral("index"), 0}});
		stream += event(QStringLiteral("content_block_start"), QJsonObject{
			{QStringLiteral("type"), QStringLiteral("content_block_start")}, {QStringLiteral("index"), 1},
			{QStringLiteral("content_block"), QJsonObject{{QStringLiteral("type"), QStringLiteral("tool_use")},
														  {QStringLiteral("id"), id},
														  {QStringLiteral("name"), QStringLiteral("run_script")},
														  {QStringLiteral("input"), QJsonObject()}}}});
			//The input in two pieces, as it streams
		const int half = int(input.size() / 2);
		for (const QByteArray &piece : {input.left(half), input.mid(half)}) {
			stream += event(QStringLiteral("content_block_delta"), QJsonObject{
				{QStringLiteral("type"), QStringLiteral("content_block_delta")}, {QStringLiteral("index"), 1},
				{QStringLiteral("delta"), QJsonObject{{QStringLiteral("type"), QStringLiteral("input_json_delta")},
													  {QStringLiteral("partial_json"), QString::fromUtf8(piece)}}}});
		}
		stream += event(QStringLiteral("content_block_stop"), QJsonObject{
			{QStringLiteral("type"), QStringLiteral("content_block_stop")}, {QStringLiteral("index"), 1}});
		stream += event(QStringLiteral("message_delta"), QJsonObject{
			{QStringLiteral("type"), QStringLiteral("message_delta")},
			{QStringLiteral("delta"), QJsonObject{{QStringLiteral("stop_reason"), QStringLiteral("tool_use")}}}});
		stream += event(QStringLiteral("message_stop"), QJsonObject{{QStringLiteral("type"), QStringLiteral("message_stop")}});
		return stream;
	}

	static QByteArray text(const QString &said)
	{
		QByteArray stream = event(QStringLiteral("message_start"), QJsonObject{
			{QStringLiteral("type"), QStringLiteral("message_start")},
			{QStringLiteral("message"), QJsonObject{{QStringLiteral("id"), QStringLiteral("msg_2")},
													{QStringLiteral("content"), QJsonArray()}}}});
		stream += event(QStringLiteral("content_block_start"), QJsonObject{
			{QStringLiteral("type"), QStringLiteral("content_block_start")}, {QStringLiteral("index"), 0},
			{QStringLiteral("content_block"), QJsonObject{{QStringLiteral("type"), QStringLiteral("text")},
														  {QStringLiteral("text"), QString()}}}});
		stream += event(QStringLiteral("content_block_delta"), QJsonObject{
			{QStringLiteral("type"), QStringLiteral("content_block_delta")}, {QStringLiteral("index"), 0},
			{QStringLiteral("delta"), QJsonObject{{QStringLiteral("type"), QStringLiteral("text_delta")},
												  {QStringLiteral("text"), said}}}});
		stream += event(QStringLiteral("content_block_stop"), QJsonObject{
			{QStringLiteral("type"), QStringLiteral("content_block_stop")}, {QStringLiteral("index"), 0}});
		stream += event(QStringLiteral("message_delta"), QJsonObject{
			{QStringLiteral("type"), QStringLiteral("message_delta")},
			{QStringLiteral("delta"), QJsonObject{{QStringLiteral("stop_reason"), QStringLiteral("end_turn")}}}});
		stream += event(QStringLiteral("message_stop"), QJsonObject{{QStringLiteral("type"), QStringLiteral("message_stop")}});
		return stream;
	}

	void answer(QTcpSocket *socket)
	{
		QByteArray &request = *static_cast<QByteArray *>(socket->property("buffer").value<void *>());
		request += socket->readAll();
		const int head_end = int(request.indexOf("\r\n\r\n"));
		if (head_end < 0) return;
		const QByteArray head = request.left(head_end);
		int length = 0;
		for (const QByteArray &line : head.split('\n'))
			if (line.toLower().startsWith("content-length:")) length = line.mid(15).trimmed().toInt();
		if (request.size() < head_end + 4 + length) return;
		m_headers << head;
		m_bodies << QJsonDocument::fromJson(request.mid(head_end + 4, length)).object();
		const QByteArray body = m_status == 200
				? (m_answers.isEmpty() ? QByteArray() : m_answers.takeFirst())
				: QByteArray("{\"type\":\"error\",\"error\":{\"type\":\"fasecad\",\"message\":\"licence\"}}");
		const QByteArray reply = "HTTP/1.1 " + QByteArray::number(m_status) + " X\r\nContent-Type: "
				+ (m_status == 200 ? "text/event-stream" : "application/json")
				+ "\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
		socket->write(reply);
		socket->disconnectFromHost();
	}

private slots:
	void initTestCase()
	{
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
		AssistantSession::setServerUrl(QUrl(QStringLiteral("http://127.0.0.1:%1/ia.php").arg(m_server.serverPort())));
		QVERIFY(QetAppTest::diagram());
	}

	void anInstructionIsCarriedOut()
	{
		QETDiagramEditor *editor = QetAppTest::editor();
		QETProject *project = editor->currentProject();
		QVERIFY(project);
		Diagram *folio = QetAppTest::view()->diagram();
		auto texts = [folio]() {
			int count = 0;
			for (QGraphicsItem *item : folio->items())
				count += qgraphicsitem_cast<IndependentTextItem *>(item) != nullptr;
			return count;
		};
		const int before = texts();

		AssistantSession session(editor);
		m_answers = {toolCall(QStringLiteral("toolu_1"), QStringLiteral("Añadir un texto"),
							  QStringLiteral("var i = qet.addText(qet.currentFolio(), 'Hola IA', 100, 100); qet.log('hecho ' + i);")),
					 text(QStringLiteral("He añadido un texto."))};
		m_bodies.clear();
		m_headers.clear();
		QSignalSpy said(&session, &AssistantSession::said);
		QSignalSpy ran(&session, &AssistantSession::scriptRan);
		QSignalSpy finished(&session, &AssistantSession::finished);
		session.ask(QStringLiteral("Añade un texto"));
		QVERIFY(finished.wait(15000));
		QCOMPARE(finished.first().first().toString(), QStringLiteral("end_turn"));

			//The script ran on the project, one undo step
		QCOMPARE(texts(), before + 1);
		QCOMPARE(ran.size(), 1);
		QVERIFY(ran.first().at(1).toBool());
		QVERIFY(!ran.first().at(2).toString().isEmpty());
		QCOMPARE(said.size(), 1);
		QCOMPARE(said.first().first().toString(), QStringLiteral("He añadido un texto."));

			//What went: the model, thinking, the fallback, the tools
		QCOMPARE(m_bodies.size(), 2);
		const QJsonObject first = m_bodies.first();
		QCOMPARE(first.value(QStringLiteral("model")).toString(), QStringLiteral("claude-opus-5-5"));
		QVERIFY(first.value(QStringLiteral("stream")).toBool());
		QCOMPARE(first.value(QStringLiteral("thinking")).toObject().value(QStringLiteral("type")).toString(),
				 QStringLiteral("adaptive"));
		QCOMPARE(first.value(QStringLiteral("fallbacks")).toString(), QStringLiteral("default"));
		QCOMPARE(first.value(QStringLiteral("tools")).toArray().size(), 2);
		QVERIFY(first.value(QStringLiteral("system")).toArray().first().toObject()
				.value(QStringLiteral("text")).toString().contains(QStringLiteral("addElement")));
		QVERIFY(m_headers.first().toLower().contains("anthropic-beta: server-side-fallback-2026-07-01"));

			//The second: the answer as it came, thinking signature and
			//all, then the result of the script, in one message
		const QJsonArray messages = m_bodies.at(1).value(QStringLiteral("messages")).toArray();
		QCOMPARE(messages.size(), 3);
		const QJsonArray assistant = messages.at(1).toObject().value(QStringLiteral("content")).toArray();
		QCOMPARE(assistant.at(0).toObject().value(QStringLiteral("signature")).toString(), QStringLiteral("SIG"));
		QCOMPARE(assistant.at(1).toObject().value(QStringLiteral("input")).toObject()
				 .value(QStringLiteral("title")).toString(), QStringLiteral("Añadir un texto"));
		const QJsonObject result = messages.at(2).toObject().value(QStringLiteral("content")).toArray()
				.first().toObject();
		QCOMPARE(result.value(QStringLiteral("type")).toString(), QStringLiteral("tool_result"));
		QCOMPARE(result.value(QStringLiteral("tool_use_id")).toString(), QStringLiteral("toolu_1"));
		QVERIFY(result.value(QStringLiteral("content")).toString().contains(QStringLiteral("hecho")));
		QVERIFY(!result.value(QStringLiteral("is_error")).toBool());
		QCOMPARE(session.messages().size(), 4);

			//One undo takes the script back
		project->undoStack()->undo();
		QCOMPARE(texts(), before);
	}

	void noFileIsWritten()
	{
		QETDiagramEditor *editor = QetAppTest::editor();
		AssistantSession session(editor);
		m_answers = {toolCall(QStringLiteral("toolu_2"), QStringLiteral("Guardar"),
							  QStringLiteral("qet.log(qet.save('C:/fasecad-test.qet') ? 'saved' : 'refused');")),
					 text(QStringLiteral("No puedo guardar."))};
		m_bodies.clear();
		QSignalSpy finished(&session, &AssistantSession::finished);
		session.ask(QStringLiteral("Guarda el proyecto"));
		QVERIFY(finished.wait(15000));
		const QJsonArray messages = m_bodies.at(1).value(QStringLiteral("messages")).toArray();
		const QString result = messages.at(2).toObject().value(QStringLiteral("content")).toArray()
				.first().toObject().value(QStringLiteral("content")).toString();
		QVERIFY2(result.contains(QStringLiteral("refused")), qPrintable(result));
		QVERIFY(!QFile::exists(QStringLiteral("C:/fasecad-test.qet")));
	}

	void theServiceSaysWhyItRefuses()
	{
		AssistantSession session(QetAppTest::editor());
		m_status = 403;
		QSignalSpy finished(&session, &AssistantSession::finished);
		session.ask(QStringLiteral("Hola"));
		QVERIFY(finished.wait(15000));
		QCOMPARE(finished.first().first().toString(), QStringLiteral("error"));
		QVERIFY2(finished.first().at(1).toString().contains(QStringLiteral("user and password")),
				 qPrintable(finished.first().at(1).toString()));
		m_status = 200;
	}

	void theAccountGoesWithEachRequest()
	{
		FaseCadAccount::set(QStringLiteral("ingeniería"), QStringLiteral("s3cr3t:ñ"));
		QCOMPARE(FaseCadAccount::password(), QStringLiteral("s3cr3t:ñ"));
		{
			QSettings settings;
			const QByteArray stored = settings.value(QStringLiteral("fasecad/account-password")).toByteArray();
			QVERIFY2(stored.startsWith("dpapi:"), stored.constData());
			QVERIFY(!QByteArray::fromBase64(stored.mid(6)).contains("s3cr3t"));
		}
		AssistantSession session(QetAppTest::editor());
		m_answers = {text(QStringLiteral("Hola."))};
		m_headers.clear();
		QSignalSpy finished(&session, &AssistantSession::finished);
		session.ask(QStringLiteral("Hola"));
		QVERIFY(finished.wait(15000));
		const QByteArray expected = "x-fasecad-acceso: "
				+ QStringLiteral("ingeniería:s3cr3t:ñ").toUtf8().toBase64().toLower();
		QVERIFY2(m_headers.first().toLower().contains(expected), m_headers.first().constData());

		m_status = 401;
		AssistantSession refused(QetAppTest::editor());
		QSignalSpy refusal(&refused, &AssistantSession::finished);
		refused.ask(QStringLiteral("Hola"));
		QVERIFY(refusal.wait(15000));
		QVERIFY2(refusal.first().at(1).toString().contains(QStringLiteral("does not accept")),
				 qPrintable(refusal.first().at(1).toString()));
		m_status = 200;
		FaseCadAccount::set(QString(), QString());
		QVERIFY(!FaseCadAccount::isSet());
		QVERIFY(FaseCadAccount::header().isEmpty());
	}

	void theButtonShowsThePanel()
	{
		QETDiagramEditor *editor = QetAppTest::editor();
		auto *button = editor->findChild<QToolButton *>(QStringLiteral("assistant_button"));
		QVERIFY(button);
		auto *panel = editor->findChild<AssistantPanel *>();
		QVERIFY(panel);
		editor->show();
			//The rings go round the button, under it, letting the mouse
			//through, and rest while the panel is open
		AssistantRipple *ripple = panel->ripple();
		QVERIFY(ripple->isRunning());
		QVERIFY(ripple->testAttribute(Qt::WA_TransparentForMouseEvents));
		QCOMPARE(ripple->geometry().center(), button->geometry().center());
		QVERIFY(ripple->parentWidget()->rect().contains(ripple->geometry()));
		const QWidgetList siblings = [&]() {
			QWidgetList list;
			for (QObject *child : button->parentWidget()->children())
				if (auto *w = qobject_cast<QWidget *>(child)) list << w;
			return list;
		}();
		QVERIFY(siblings.indexOf(ripple) < siblings.indexOf(button));
		button->click();
		QTRY_VERIFY(panel->isVisible());
		QVERIFY(!ripple->isRunning());
		button->click();
		QTRY_VERIFY(!panel->isVisible());
		QVERIFY(ripple->isRunning());
	}
};

QET_APP_TEST_MAIN(tst_assistant)
#include "tst_assistant.moc"
