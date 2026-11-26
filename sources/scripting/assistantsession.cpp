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
#include "assistantsession.h"

#include "qetscriptapi.h"
#include "qetscripting.h"
#include "../diagram.h"
#include "../diagramview.h"
#include "../qetapp.h"
#include "../qetdiagrameditor.h"
#include "../qetgraphicsitem/element.h"
#include "../qetproject.h"
#include "../utils/fasecadaccount.h"
#include "../utils/fasecadservices.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSettings>

namespace {
	const QString url_key = QStringLiteral("assistant/url");
	const QString default_url = QStringLiteral("https://api.fasecad.com/ia.php");
		/// The fallback of the API on a declined request (beta)
	const QByteArray fallback_beta = "server-side-fallback-2026-07-01";
	const QString panel_symbols = QStringLiteral("common://10_electric/05_iec_60617_cuadros");

	QString compact(const QJsonValue &value)
	{
		if (value.isObject())
			return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
		if (value.isArray())
			return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
		return value.toString();
	}
}

AssistantSession::AssistantSession(QETDiagramEditor *editor, QObject *parent) :
	QObject(parent),
	m_editor(editor),
	m_network(new QNetworkAccessManager(this))
{}

QUrl AssistantSession::serverUrl()
{
	QSettings settings;
	return QUrl(settings.value(url_key, default_url).toString());
}

void AssistantSession::setServerUrl(const QUrl &url)
{
	QSettings settings;
	if (url.isEmpty() || url.toString() == default_url)
		settings.remove(url_key);
	else
		settings.setValue(url_key, url.toString());
}

QString AssistantSession::model()
{
	return QStringLiteral("claude-opus-5-5");
}

QString AssistantSession::systemPrompt()
{
	QString text = QStringLiteral(
		"You are the AI assistant built into FaseCAD, a CAD program for electrical schematics and "
		"control panels (based on QElectroTech). The user writes to you in the \"IA\" panel of the "
		"electrical design window, and you carry out what they ask on the project open in FaseCAD, "
		"working with them: they see each change as you make it and can undo it.\n"
		"\n"
		"How you act:\n"
		"- You change and read the project only through the run_script tool: a JavaScript program "
		"that sees one global, `qet`, the scripting API of FaseCAD listed below. A script logs "
		"what it finds with qet.log(); the log comes back to you. Read before you write when you "
		"are not sure of what is there: uuids, folio indices, terminal indices.\n"
		"- Each run_script call is one undo step, named by its title: give it a short title in the "
		"user's language saying what it does (\"Añadir el contactor -KM1\"). Prefer a few scripts "
		"that each do one meaningful change over one huge script or many tiny ones.\n"
		"- Symbols are placed by their location in the collection, found with list_symbols. The "
		"symbols for control panels are in common://10_electric/05_iec_60617_cuadros.\n"
		"- Coordinates are those of the folio, in its own units; elements snap to a grid of 10. Look "
		"at what is already on a folio (qet.elementGeometry, qet.folioBorder) to place things in "
		"line with it, and leave room for the wires and the texts.\n"
		"- No script saves or exports a file: those calls are refused. Saving stays the user's.\n"
		"- When an instruction is unclear or could harm the user's work (deleting much, changing "
		"many folios), ask before doing it. Otherwise do it and say briefly what you did.\n"
		"- Answer in the language the user writes in, short and plainly, the way an electrician "
		"talks to another.\n"
		"\n"
		"Each message of the user starts with the state of FaseCAD (the project, its folios, the "
		"folio shown, the elements selected) as JSON.\n"
		"\n"
		"The calls of `qet` (return type, name, parameters):\n");
	text += QetScriptApi::signatures().join(QLatin1Char('\n'));
	return text;
}

QJsonArray AssistantSession::tools()
{
	const QJsonObject run_script{
		{QStringLiteral("name"), QStringLiteral("run_script")},
		{QStringLiteral("description"), QStringLiteral(
			"Run a JavaScript program on the project open in FaseCAD. It sees the global `qet` "
			"(the scripting API in the system prompt). Everything it changes is one undo step "
			"named by the title. Returns ok, what it logged with qet.log(), the error if it threw, "
			"and the undo step it left (empty when it changed nothing).")},
		{QStringLiteral("strict"), true},
		{QStringLiteral("input_schema"), QJsonObject{
			{QStringLiteral("type"), QStringLiteral("object")},
			{QStringLiteral("properties"), QJsonObject{
				{QStringLiteral("title"), QJsonObject{
					{QStringLiteral("type"), QStringLiteral("string")},
					{QStringLiteral("description"), QStringLiteral("What the script does, in the user's language: the name of its undo step")}}},
				{QStringLiteral("code"), QJsonObject{
					{QStringLiteral("type"), QStringLiteral("string")},
					{QStringLiteral("description"), QStringLiteral("The JavaScript program")}}}}},
			{QStringLiteral("required"), QJsonArray{QStringLiteral("title"), QStringLiteral("code")}},
			{QStringLiteral("additionalProperties"), false}}}};
	const QJsonObject list_symbols{
		{QStringLiteral("name"), QStringLiteral("list_symbols")},
		{QStringLiteral("description"), QStringLiteral(
			"List a folder of the symbol collection: its subfolders and its symbols, each with its "
			"location (for qet.addElement) and its name. Folders: common:// (the collection of "
			"FaseCAD), custom:// (the user's own).")},
		{QStringLiteral("strict"), true},
		{QStringLiteral("input_schema"), QJsonObject{
			{QStringLiteral("type"), QStringLiteral("object")},
			{QStringLiteral("properties"), QJsonObject{
				{QStringLiteral("folder"), QJsonObject{
					{QStringLiteral("type"), QStringLiteral("string")},
					{QStringLiteral("description"), QStringLiteral("e.g. common://10_electric/05_iec_60617_cuadros")}}}}},
			{QStringLiteral("required"), QJsonArray{QStringLiteral("folder")}},
			{QStringLiteral("additionalProperties"), false}}}};
	return {run_script, list_symbols};
}

void AssistantSession::ask(const QString &instruction)
{
	if (m_running || instruction.trimmed().isEmpty()) return;
	m_running = true;
	m_steps = 0;
	m_messages.append(QJsonObject{
		{QStringLiteral("role"), QStringLiteral("user")},
		{QStringLiteral("content"), QJsonArray{
			QJsonObject{{QStringLiteral("type"), QStringLiteral("text")},
						{QStringLiteral("text"), QStringLiteral("FaseCAD: ") + context()}},
			QJsonObject{{QStringLiteral("type"), QStringLiteral("text")},
						{QStringLiteral("text"), instruction.trimmed()}}}}});
	post();
}

void AssistantSession::stop()
{
	if (!m_running) return;
	if (m_reply) {
		QNetworkReply *reply = m_reply;
		m_reply = nullptr;
		reply->abort();
		reply->deleteLater();
	}
	finish(QStringLiteral("stopped"));
}

void AssistantSession::clear()
{
	stop();
	m_messages = QJsonArray();
}

void AssistantSession::post()
{
	m_buffer.clear();
	m_message = QJsonObject();
	m_blocks.clear();
	m_partial_json.clear();
	m_stop_reason.clear();
	m_stream_error.clear();
	m_message_done = false;

	const QJsonObject ephemeral{{QStringLiteral("type"), QStringLiteral("ephemeral")}};
	QJsonObject body{
		{QStringLiteral("model"), model()},
		{QStringLiteral("max_tokens"), 64000},
		{QStringLiteral("stream"), true},
		{QStringLiteral("thinking"), QJsonObject{{QStringLiteral("type"), QStringLiteral("adaptive")}}},
		{QStringLiteral("output_config"), QJsonObject{{QStringLiteral("effort"), QStringLiteral("high")}}},
			//Declined by a safety classifier: the API carries on with the
			//model it routes the category to, in the same call
		{QStringLiteral("fallbacks"), QStringLiteral("default")},
			//The conversation so far is cached as it grows; the system
			//prompt and the tools, the same every time, before it
		{QStringLiteral("cache_control"), ephemeral},
		{QStringLiteral("system"), QJsonArray{QJsonObject{
			 {QStringLiteral("type"), QStringLiteral("text")},
			 {QStringLiteral("text"), systemPrompt()},
			 {QStringLiteral("cache_control"), ephemeral}}}},
		{QStringLiteral("tools"), tools()},
		{QStringLiteral("messages"), m_messages}};
	m_last_request = body;

	QNetworkRequest request(serverUrl());
	request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
	request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("FaseCAD"));
	request.setRawHeader("anthropic-beta", fallback_beta);
	request.setRawHeader("Accept", "text/event-stream");
		//The fasecad.com account of the user
	FaseCadAccount::applyTo(request);
		//A long silence while the model thinks is not an end: only five
		//minutes without a byte is
	request.setTransferTimeout(300000);
	emit statusChanged(tr("Thinking..."));
	m_reply = m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
	connect(m_reply, &QNetworkReply::readyRead, this, &AssistantSession::readEvents);
	connect(m_reply, &QNetworkReply::finished, this, &AssistantSession::replyDone);
}

void AssistantSession::readEvents()
{
	if (!m_reply) return;
	const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	if (status && status != 200) return;   //An error: read whole at the end
	m_buffer += m_reply->readAll();
	m_buffer.replace("\r\n", "\n");
	int end;
	while ((end = m_buffer.indexOf("\n\n")) >= 0) {
		const QByteArray chunk = m_buffer.left(end);
		m_buffer.remove(0, end + 2);
		QString event;
		QByteArray data;
		for (const QByteArray &line : chunk.split('\n')) {
			if (line.startsWith("event:")) event = QString::fromUtf8(line.mid(6).trimmed());
			else if (line.startsWith("data:")) data += line.mid(5).trimmed();
		}
		if (data.isEmpty()) continue;
		const QJsonObject object = QJsonDocument::fromJson(data).object();
		handleEvent(event.isEmpty() ? object.value(QStringLiteral("type")).toString() : event, object);
	}
}

void AssistantSession::handleEvent(const QString &event, const QJsonObject &data)
{
	if (event == QLatin1String("message_start")) {
		m_message = data.value(QStringLiteral("message")).toObject();
	} else if (event == QLatin1String("content_block_start")) {
		const int index = data.value(QStringLiteral("index")).toInt();
		while (m_blocks.size() <= index) m_blocks.append(QJsonObject());
		m_blocks[index] = data.value(QStringLiteral("content_block")).toObject();
		if (index == 0 || m_blocks[index].value(QStringLiteral("type")).toString() == QLatin1String("thinking"))
			emit statusChanged(tr("Thinking..."));
	} else if (event == QLatin1String("content_block_delta")) {
		const int index = data.value(QStringLiteral("index")).toInt();
		if (index < 0 || index >= m_blocks.size()) return;
		QJsonObject &block = m_blocks[index];
		const QJsonObject delta = data.value(QStringLiteral("delta")).toObject();
		const QString type = delta.value(QStringLiteral("type")).toString();
		if (type == QLatin1String("text_delta")) {
			block.insert(QStringLiteral("text"), block.value(QStringLiteral("text")).toString()
						 + delta.value(QStringLiteral("text")).toString());
		} else if (type == QLatin1String("thinking_delta")) {
			block.insert(QStringLiteral("thinking"), block.value(QStringLiteral("thinking")).toString()
						 + delta.value(QStringLiteral("thinking")).toString());
		} else if (type == QLatin1String("signature_delta")) {
			block.insert(QStringLiteral("signature"), block.value(QStringLiteral("signature")).toString()
						 + delta.value(QStringLiteral("signature")).toString());
		} else if (type == QLatin1String("input_json_delta")) {
			m_partial_json[index] += delta.value(QStringLiteral("partial_json")).toString();
		} else if (type == QLatin1String("citations_delta")) {
			QJsonArray citations = block.value(QStringLiteral("citations")).toArray();
			citations.append(delta.value(QStringLiteral("citation")));
			block.insert(QStringLiteral("citations"), citations);
		}
	} else if (event == QLatin1String("content_block_stop")) {
		const int index = data.value(QStringLiteral("index")).toInt();
		if (index < 0 || index >= m_blocks.size()) return;
		QJsonObject &block = m_blocks[index];
		const QString type = block.value(QStringLiteral("type")).toString();
		if (m_partial_json.contains(index)) {
			const QString json = m_partial_json.take(index);
			block.insert(QStringLiteral("input"), json.trimmed().isEmpty()
						 ? QJsonObject() : QJsonDocument::fromJson(json.toUtf8()).object());
		}
		if (type == QLatin1String("text")) {
			const QString text = block.value(QStringLiteral("text")).toString().trimmed();
			if (!text.isEmpty()) emit said(text);
		} else if (type == QLatin1String("tool_use")) {
			emit statusChanged(tr("Working on the project..."));
		}
	} else if (event == QLatin1String("message_delta")) {
		const QJsonObject delta = data.value(QStringLiteral("delta")).toObject();
		if (delta.contains(QStringLiteral("stop_reason")))
			m_stop_reason = delta.value(QStringLiteral("stop_reason")).toString();
	} else if (event == QLatin1String("message_stop")) {
		m_message_done = true;
	} else if (event == QLatin1String("error")) {
		m_stream_error = data.value(QStringLiteral("error")).toObject()
				.value(QStringLiteral("message")).toString();
	}
}

void AssistantSession::replyDone()
{
	QNetworkReply *reply = m_reply;
	if (!reply) return;   //Stopped
	m_reply = nullptr;
	reply->deleteLater();
	const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	if (status != 200) {
		const QByteArray body = reply->readAll();
		QString message = QJsonDocument::fromJson(body).object().value(QStringLiteral("error"))
				.toObject().value(QStringLiteral("message")).toString();
		if (message.isEmpty())
			message = QJsonDocument::fromJson(body).object().value(QStringLiteral("error")).toString();
		if (status == 409) {
			message = tr("Your session was closed: the account is open on another computer (Sign in...).");
			FaseCadServices::instance()->sessionClosed();
		} else if (status == 401 || status == 403)
			message = FaseCadAccount::isSet()
					? tr("The AI service does not accept this user and password (Sign in...).")
					: tr("The AI service asks for your user and password (Sign in...).");
		else if (status == 402)
			message = tr("The AI is part of FaseCAD Pro: subscribe at my.fasecad.com, then sign in again.");
		else if (status == 429)
			message = tr("The AI is busy, or the AI included this month is used up: try again in a while.");
		if (message.isEmpty())
			message = status ? tr("The AI service answered %1.").arg(status) : reply->errorString();
		finish(QStringLiteral("error"), message);
		return;
	}
	readEvents();
	if (!m_stream_error.isEmpty()) {
		finish(QStringLiteral("error"), m_stream_error);
		return;
	}
	if (!m_message_done) {
		finish(QStringLiteral("error"), tr("The answer of the AI was cut off: %1").arg(reply->errorString()));
		return;
	}
	messageDone();
}

void AssistantSession::messageDone()
{
		//The answer as it came, its thinking blocks included
	QJsonArray content;
	for (const QJsonObject &block : std::as_const(m_blocks))
		if (!block.isEmpty()) content.append(block);
	m_messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("assistant")},
								  {QStringLiteral("content"), content}});

	if (m_stop_reason == QLatin1String("tool_use")) {
		QJsonArray results;
		const bool too_many = ++m_steps > m_max_steps;
		for (const QJsonValue &value : std::as_const(content)) {
			const QJsonObject block = value.toObject();
			if (block.value(QStringLiteral("type")).toString() != QLatin1String("tool_use")) continue;
			QJsonObject result = too_many
					? QJsonObject{{QStringLiteral("content"), QStringLiteral("Not run: the limit of steps for one instruction is reached.")},
								  {QStringLiteral("is_error"), true}}
					: runTool(block);
			result.insert(QStringLiteral("type"), QStringLiteral("tool_result"));
			result.insert(QStringLiteral("tool_use_id"), block.value(QStringLiteral("id")));
			results.append(result);
		}
			//Every result of the turn in one message, so the history
			//stays whole whatever comes next
		m_messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
									  {QStringLiteral("content"), results}});
		if (too_many) {
			finish(QStringLiteral("steps"), tr("Stopped after %1 steps: say how to go on.").arg(m_max_steps));
			return;
		}
		if (!m_running) return;   //Stopped while a script ran
		post();
		return;
	}
	if (m_stop_reason == QLatin1String("pause_turn")) {
		post();
		return;
	}
	if (m_stop_reason == QLatin1String("refusal")) {
		finish(QStringLiteral("refusal"), tr("The AI declined to do this."));
		return;
	}
	if (m_stop_reason == QLatin1String("max_tokens")) {
		finish(QStringLiteral("max_tokens"), tr("The answer was too long and was cut off."));
		return;
	}
	finish(QStringLiteral("end_turn"));
}

void AssistantSession::finish(const QString &reason, const QString &message)
{
	if (!m_running) return;
	m_running = false;
	emit statusChanged(QString());
	emit finished(reason, message);
}

QJsonObject AssistantSession::runTool(const QJsonObject &use)
{
	const QString name = use.value(QStringLiteral("name")).toString();
	const QJsonObject input = use.value(QStringLiteral("input")).toObject();
	QJsonObject answer;
	if (name == QLatin1String("run_script")) answer = runScript(input);
	else if (name == QLatin1String("list_symbols")) answer = listSymbols(input);
	else answer = QJsonObject{{QStringLiteral("error"), QStringLiteral("no such tool")}};
	QJsonObject result{{QStringLiteral("content"), compact(answer)}};
	if (answer.contains(QStringLiteral("error")) && !answer.value(QStringLiteral("ok")).toBool())
		result.insert(QStringLiteral("is_error"), true);
	return result;
}

QJsonObject AssistantSession::runScript(const QJsonObject &input)
{
	const QString title = input.value(QStringLiteral("title")).toString().trimmed();
	const QString code = input.value(QStringLiteral("code")).toString();
	QETProject *project = m_editor ? m_editor->currentProject() : nullptr;
	if (!project) {
		emit scriptRan(title, false, QString(), tr("No project is open."));
		return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("no project is open in FaseCAD")}};
	}
	if (code.trimmed().isEmpty())
		return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("no code given")}};
#ifdef QET_HAS_SCRIPTING
	QetScripting::LiveRun run;
	run.no_files = true;
	const QString step = title.isEmpty() ? tr("AI") : tr("AI: %1").arg(title);
	const bool ok = QetScripting::runSource(code, QStringLiteral("ia:") + title, step,
											project, m_editor->currentDiagramView(), &run);
	QJsonObject answer{{QStringLiteral("ok"), ok},
					   {QStringLiteral("log"), QJsonArray::fromStringList(run.log)},
					   {QStringLiteral("undo"), run.undoText}};
	if (!run.error.isEmpty()) answer.insert(QStringLiteral("error"), run.error);
	emit scriptRan(title, ok, run.undoText,
				   run.error.isEmpty() ? run.log.join(QLatin1Char('\n')) : run.error);
	return answer;
#else
	return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("this FaseCAD has no scripting")}};
#endif
}

QJsonObject AssistantSession::listSymbols(const QJsonObject &input) const
{
	QString folder = input.value(QStringLiteral("folder")).toString().trimmed();
	if (folder.isEmpty()) folder = panel_symbols;
	QString root;
	QString scheme;
	if (folder.startsWith(QLatin1String("common://"))) {
		root = QETApp::commonElementsDirN();
		scheme = QStringLiteral("common://");
	} else if (folder.startsWith(QLatin1String("custom://"))) {
		root = QETApp::customElementsDirN();
		scheme = QStringLiteral("custom://");
	} else {
		return {{QStringLiteral("error"), QStringLiteral("a folder starts with common:// or custom://")}};
	}
	QString relative = folder.mid(scheme.size());
	while (relative.endsWith(QLatin1Char('/'))) relative.chop(1);
	if (relative.contains(QLatin1String(".."))) return {{QStringLiteral("error"), QStringLiteral("no such folder")}};
	const QDir dir(relative.isEmpty() ? root : root + QLatin1Char('/') + relative);
	if (!dir.exists()) return {{QStringLiteral("error"), QStringLiteral("no such folder")}};
	const QString prefix = scheme + (relative.isEmpty() ? QString() : relative + QLatin1Char('/'));

	QJsonArray folders;
	for (const QString &name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
		folders.append(prefix + name);
	static const QRegularExpression names(QStringLiteral("<name lang=\"(es|en)\">([^<]*)</name>"));
	QJsonArray symbols;
	const QStringList files = dir.entryList({QStringLiteral("*.elmt")}, QDir::Files, QDir::Name);
	for (const QString &file : files) {
		if (symbols.size() >= 400) break;
		QFile handle(dir.filePath(file));
		QString shown;
		if (handle.open(QIODevice::ReadOnly)) {
			const QString head = QString::fromUtf8(handle.read(4096));
			QString es, en;
			for (auto it = names.globalMatch(head) ; it.hasNext() ; ) {
				const auto match = it.next();
				(match.captured(1) == QLatin1String("es") ? es : en) = match.captured(2);
			}
			shown = es.isEmpty() ? en : es;
		}
		symbols.append(QJsonObject{{QStringLiteral("location"), prefix + file},
								   {QStringLiteral("name"), shown}});
	}
	return {{QStringLiteral("folder"), folder}, {QStringLiteral("folders"), folders},
			{QStringLiteral("symbols"), symbols}};
}

QString AssistantSession::context() const
{
	QJsonObject state;
	QETProject *project = m_editor ? m_editor->currentProject() : nullptr;
	if (!project) {
		state.insert(QStringLiteral("project"), QJsonValue::Null);
		return compact(state);
	}
	DiagramView *view = m_editor->currentDiagramView();
	Diagram *shown = view ? view->diagram() : nullptr;
	const QList<Diagram *> folios = project->diagrams();
	QJsonArray titles;
	for (int i = 0 ; i < folios.size() && i < 80 ; ++i)
		titles.append(QJsonObject{{QStringLiteral("index"), i}, {QStringLiteral("title"), folios.at(i)->title()}});
	QJsonArray selected;
	if (shown) {
		for (QGraphicsItem *item : shown->selectedItems()) {
			if (auto *element = qgraphicsitem_cast<Element *>(item)) {
				selected.append(QJsonObject{{QStringLiteral("uuid"), element->uuid().toString()},
											{QStringLiteral("label"), element->actualLabel()},
											{QStringLiteral("symbol"), element->name()}});
				if (selected.size() >= 50) break;
			}
		}
	}
	state.insert(QStringLiteral("project"), project->title());
	state.insert(QStringLiteral("folios"), titles);
	state.insert(QStringLiteral("folio_shown"), shown ? int(folios.indexOf(shown)) : -1);
	state.insert(QStringLiteral("selected_elements"), selected);
	state.insert(QStringLiteral("read_only"), project->isReadOnly());
	return compact(state);
}
