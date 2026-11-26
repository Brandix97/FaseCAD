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
#ifndef ASSISTANTSESSION_H
#define ASSISTANTSESSION_H

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QUrl>

class QETDiagramEditor;
class QNetworkAccessManager;
class QNetworkReply;

/**
	A conversation with the AI of the "IA" panel (Brandon, 2026-11-25): the
	user's instructions, carried out on the project open in FaseCAD.

	Each request goes to the AI service of fasecad.com (ia.php), which adds
	the key and passes it to the Claude API (Messages API, streamed). The
	loop of tool calls runs here: the model writes scripts (run_script) that
	act on the open project through the scripting API, each script one
	undo step, and looks up symbols of the collection (list_symbols). No
	script writes a file: saving and exporting stay the user's.

	The conversation is only ever added to, its answers kept as they came
	(their thinking blocks too), as the API asks.
*/
class AssistantSession : public QObject
{
	Q_OBJECT

	public:
		explicit AssistantSession(QETDiagramEditor *editor, QObject *parent = nullptr);

			/// The AI service, https://api.fasecad.com/ia.php by default
		static QUrl serverUrl();
		static void setServerUrl(const QUrl &url);
		static QString model();
			/// What the model is told once for all: who it is, how to
			/// act, every call a script can make
		static QString systemPrompt();
		static QJsonArray tools();

			/// Carry out @p instruction; answers come through the signals
		void ask(const QString &instruction);
			/// Stop now: the step under way is not sent back
		void stop();
			/// Start again with an empty conversation
		void clear();
		bool isRunning() const {return m_running;}
		QJsonArray messages() const {return m_messages;}
			/// The last request sent, as it went
		QJsonObject lastRequest() const {return m_last_request;}
			/// At most this many rounds of tool calls for one instruction
		void setMaxSteps(int steps) {m_max_steps = steps;}

	signals:
			/// A text of the AI, once whole
		void said(const QString &text);
			/// A script it ran: its title, whether it worked, the undo
			/// step it left, what it logged or the error
		void scriptRan(const QString &title, bool ok, const QString &undo, const QString &details);
		void statusChanged(const QString &status);
			/// The instruction is done: "end_turn", "stopped", "refusal",
			/// "max_tokens", "steps", or "error" with @p message
		void finished(const QString &reason, const QString &message);

	private:
		void post();
		void readEvents();
		void handleEvent(const QString &event, const QJsonObject &data);
		void replyDone();
		void messageDone();
		void finish(const QString &reason, const QString &message = QString());
		QJsonObject runTool(const QJsonObject &use);
		QJsonObject runScript(const QJsonObject &input);
		QJsonObject listSymbols(const QJsonObject &input) const;
		QString context() const;

		QETDiagramEditor *m_editor = nullptr;
		QNetworkAccessManager *m_network = nullptr;
		QPointer<QNetworkReply> m_reply;
		QJsonArray m_messages;
		QJsonObject m_last_request;
		QByteArray m_buffer;
			/// The answer being streamed: its blocks, the JSON of the
			/// tool inputs as they come, how it stopped
		QJsonObject m_message;
		QList<QJsonObject> m_blocks;
		QHash<int, QString> m_partial_json;
		QString m_stop_reason;
		QString m_stream_error;
		bool m_message_done = false;
		bool m_running = false;
		int m_steps = 0;
		int m_max_steps = 30;
};

#endif // ASSISTANTSESSION_H
