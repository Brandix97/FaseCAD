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
#include "container/qetzip.h"
#include "diagramview.h"
#include "plugins/pluginlibrary.h"
#include "plugins/pluginmanifest.h"
#include "qetproject.h"
#include "scripting/scriptdialog.h"
#include "ui/commandtabs.h"
#include "utils/qetsettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>
#include <QUndoStack>

/**
	Plugins: the manifest, installing a .fasplugin, switching it off and
	removing it, its commands as actions and tabs, a command run on the
	open project in one step of undo, and the windows qet.dialog() draws.
*/
class tst_plugins : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;

	static QByteArray manifest(const QString &id, int api = 1, const QString &extra = QString())
	{
		return QStringLiteral(
			"{\"id\": \"%1\", \"name\": \"Test %1\", \"version\": \"2.0\", \"author\": \"Acme\","
			" \"api\": %2, \"tab\": \"Acme\" %3,"
			" \"commands\": [{\"id\": \"title\", \"label\": \"Set\\ntitle\", \"script\": \"main.js\","
			" \"function\": \"setTitle\", \"size\": \"large\"}]}")
				.arg(id).arg(api).arg(extra).toUtf8();
	}

	QString archive(const QString &id, const QByteArray &json, bool in_folder = true)
	{
		const QString root = in_folder ? id + QLatin1Char('/') : QString();
		QList<QetZip::Entry> entries{
			{root + QStringLiteral("plugin.json"), json},
			{root + QStringLiteral("main.js"),
			 QByteArray("function setTitle() { qet.setFolioTitle(qet.currentFolio(), 'From the plugin'); }\n")}};
		const QString path = m_dir.filePath(id + QStringLiteral(".fasplugin"));
		QString error;
		if (!QetZip::write(path, entries, &error)) qWarning() << error;
		return path;
	}

private slots:
	void initTestCase()
	{
		QVERIFY(m_dir.isValid());
		QDir().mkpath(m_dir.filePath(QStringLiteral("plugins")));
		PluginLibrary::setFolder(m_dir.filePath(QStringLiteral("plugins")));
			//Plugins run even with the scripts switched off
		QetSettings::setScriptingEnabled(false);
	}

	void aManifestSaysWhatThePluginIs()
	{
		QString error;
		const PluginManifest m = PluginManifest::parse(manifest(QStringLiteral("acme.one")), &error);
		QVERIFY2(error.isEmpty(), qPrintable(error));
		QCOMPARE(m.id, QStringLiteral("acme.one"));
		QCOMPARE(m.tab, QStringLiteral("Acme"));
		QCOMPARE(m.commands.size(), 1);
		QVERIFY(m.commands.first().large);
		QCOMPARE(m.commands.first().function, QStringLiteral("setTitle"));
	}

	void aManifestIsRefused_data()
	{
		QTest::addColumn<QByteArray>("json");
		QTest::newRow("not json") << QByteArray("{");
		QTest::newRow("bad id") << manifest(QStringLiteral("../evil"));
		QTest::newRow("newer api") << manifest(QStringLiteral("acme.two"), 99);
		QTest::newRow("unknown permission")
				<< manifest(QStringLiteral("acme.three"), 1, QStringLiteral(", \"permissions\": [\"network\"]"));
		QTest::newRow("no command") << QByteArray("{\"id\": \"acme.four\", \"commands\": []}");
		QTest::newRow("outside the plugin") << QByteArray(
			"{\"id\": \"acme.five\", \"commands\": [{\"id\": \"a\", \"script\": \"../x.js\", \"function\": \"f\"}]}");
		QTest::newRow("no function") << QByteArray(
			"{\"id\": \"acme.six\", \"commands\": [{\"id\": \"a\", \"function\": \"alert(1)\"}]}");
	}
	void aManifestIsRefused()
	{
		QFETCH(QByteArray, json);
		QString error;
		PluginManifest::parse(json, &error);
		QVERIFY(!error.isEmpty());
	}

	void installSwitchOffAndRemove()
	{
		PluginLibrary &library = PluginLibrary::instance();
		const QString path = archive(QStringLiteral("acme.title"), manifest(QStringLiteral("acme.title")));
		QString error, id;
		QCOMPARE(PluginLibrary::readArchive(path, &error).name, QStringLiteral("Test acme.title"));
		QVERIFY2(library.install(path, false, &id, &error), qPrintable(error));
		QCOMPARE(id, QStringLiteral("acme.title"));
		QVERIFY(library.find(id));
		QVERIFY(library.find(id)->enabled);
			//Not twice unless asked
		QVERIFY(!library.install(path, false, &id, &error));
		QVERIFY(library.install(path, true, &id, &error));

			//Its command is an action, in a tab of its own
		QETDiagramEditor *editor = QetAppTest::editor();
		QVERIFY(editor);
		QetSettings::setCommandTabs(true);
		QMetaObject::invokeMethod(editor, "updateCommandTabs");
		library.rescan();
		auto *tabs = editor->findChild<CommandTabs *>();
		QVERIFY(tabs);
		bool found = false;
		for (int i = 0 ; i < tabs->count() ; ++i) found = found || tabs->tabText(i) == QStringLiteral("Acme");
		QVERIFY(found);

			//Switched off: no tab, no action
		library.setEnabled(id, false);
		QVERIFY(!library.find(id)->enabled);
		found = false;
		for (int i = 0 ; i < tabs->count() ; ++i) found = found || tabs->tabText(i) == QStringLiteral("Acme");
		QVERIFY(!found);
		library.setEnabled(id, true);

		QVERIFY(library.uninstall(id, &error));
		QVERIFY(!library.find(id));
		QVERIFY(!QFileInfo::exists(QDir(PluginLibrary::folder()).filePath(id)));
	}

	void anArchiveWithThePluginAtItsRoot()
	{
		const QString path = archive(QStringLiteral("acme.root"), manifest(QStringLiteral("acme.root")), false);
		QString error, id;
		QVERIFY2(PluginLibrary::instance().install(path, false, &id, &error), qPrintable(error));
		QVERIFY(PluginLibrary::instance().uninstall(id, &error));
	}

	void aCommandRunsInOneStepOfUndo()
	{
		const QString path = archive(QStringLiteral("acme.run"), manifest(QStringLiteral("acme.run")));
		QString error, id;
		QVERIFY(PluginLibrary::instance().install(path, true, &id, &error));
		const PluginLibrary::Plugin *plugin = PluginLibrary::instance().find(id);
		QVERIFY(plugin);

		DiagramView *view = QetAppTest::view();
		QVERIFY(view);
		QETProject *project = view->diagram()->project();
		const QString before = view->diagram()->title();
		QVERIFY(PluginLibrary::run(*plugin, plugin->manifest.commands.first(), project, view, &error));
		QCOMPARE(view->diagram()->title(), QStringLiteral("From the plugin"));
		QVERIFY(project->undoStack()->undoText().startsWith(QStringLiteral("Plugin:")));
		project->undoStack()->undo();
		QCOMPARE(view->diagram()->title(), before);
		PluginLibrary::instance().uninstall(id, &error);
	}

	void aDialogDrawsItsFields()
	{
		const QVariantMap spec{
			{QStringLiteral("title"), QStringLiteral("Export")},
			{QStringLiteral("fields"), QVariantList{
				QVariantMap{{"id", "name"}, {"label", "Order"}, {"type", "text"}, {"value", "PO-1"}},
				QVariantMap{{"id", "copies"}, {"type", "number"}, {"value", 2}, {"min", 1}, {"max", 10}},
				QVariantMap{{"id", "format"}, {"type", "choice"}, {"items", QStringList{"CSV", "XML"}}, {"value", "XML"}},
				QVariantMap{{"id", "spares"}, {"type", "check"}, {"value", true}},
				QVariantMap{{"id", "rows"}, {"type", "table"}, {"columns", QStringList{"A", "B"}},
							{"value", QVariantList{QVariantList{"1", "2"}}}},
				QVariantMap{{"id", "bad"}, {"type", "slider"}}}}};
		ScriptDialog dialog(spec);
		QCOMPARE(dialog.errors().size(), 1);
		QCOMPARE(dialog.windowTitle(), QStringLiteral("Export"));
		qobject_cast<QLineEdit *>(dialog.field(QStringLiteral("name")))->setText(QStringLiteral("PO-7"));
		qobject_cast<QSpinBox *>(dialog.field(QStringLiteral("copies")))->setValue(50);
		const QVariantMap values = dialog.values();
		QCOMPARE(values.value(QStringLiteral("name")).toString(), QStringLiteral("PO-7"));
		QCOMPARE(values.value(QStringLiteral("copies")).toInt(), 10);
		QCOMPARE(values.value(QStringLiteral("format")).toString(), QStringLiteral("XML"));
		QCOMPARE(values.value(QStringLiteral("spares")).toBool(), true);
		QCOMPARE(values.value(QStringLiteral("rows")).toList().first().toList().at(1).toString(),
				 QStringLiteral("2"));
		QVERIFY(!values.contains(QStringLiteral("bad")));
	}

	void theExamplePluginIsValid()
	{
		QFile file(QStringLiteral(QET_SOURCE_DIR) + QStringLiteral("/examples/plugins/fasecad.ejemplo-aparatos/plugin.json"));
		QVERIFY(file.open(QIODevice::ReadOnly));
		QString error;
		const PluginManifest m = PluginManifest::parse(file.readAll(), &error);
		QVERIFY2(error.isEmpty(), qPrintable(error));
		QCOMPARE(m.commands.size(), 2);
	}
};

QET_APP_TEST_MAIN(tst_plugins)
#include "tst_plugins.moc"
