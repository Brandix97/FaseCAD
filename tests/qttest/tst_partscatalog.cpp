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
#include "partsdb/partsarchive.h"
#include "partsdb/partscatalogdialog.h"
#include "utils/fasecadaccount.h"
#include "partsdb/partsdatabase.h"

#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUrlQuery>

/**
	The official parts catalog (Brandon, 2026-11-25): the list the server
	gives, the photo of a part, the part put into the parts database, and
	the fasecad.com account once the server asks for it. The server here answers
	as catalogo.php of the site does.
*/
class tst_partscatalog : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;
	QTcpServer m_server;
	QByteArray m_faspart;
	QByteArray m_photo;
	bool m_wants_key = false;
	QStringList m_asked;

	void answer(QTcpSocket *socket)
	{
		const QByteArray request = socket->readAll();
		const QList<QByteArray> lines = request.split('\n');
		if (lines.isEmpty()) return;
		const QList<QByteArray> first = lines.first().split(' ');
		if (first.size() < 2) return;
		const QUrl url(QString::fromLatin1(first.at(1)));
		const QUrlQuery query(url);
		m_asked << url.query();
		const bool keyed = request.toLower().contains("x-fasecad-acceso: "
				+ QByteArray("taller:clave-del-taller").toBase64().toLower());
		QByteArray body;
		QByteArray type = "application/json";
		int code = 200;
		if (query.hasQueryItem(QStringLiteral("lista"))) {
			QJsonObject part{{QStringLiteral("id"), QStringLiteral("acme-ps-30")},
							 {QStringLiteral("manufacturer"), QStringLiteral("Acme")},
							 {QStringLiteral("order_number"), QStringLiteral("PS-30")},
							 {QStringLiteral("description"), QStringLiteral("Power supply 24 V")},
							 {QStringLiteral("category"), QStringLiteral("Power supplies")},
							 {QStringLiteral("photo"), true}};
			const bool found = !query.hasQueryItem(QStringLiteral("q"))
					|| QStringLiteral("acme ps-30 power supply").contains(query.queryItemValue(QStringLiteral("q")).toLower());
			body = QJsonDocument(QJsonObject{
				{QStringLiteral("total"), found ? 1 : 0},
				{QStringLiteral("pagina"), 1},
				{QStringLiteral("por_pagina"), 60},
				{QStringLiteral("categorias"), QJsonArray{QStringLiteral("Power supplies")}},
				{QStringLiteral("piezas"), found ? QJsonArray{part} : QJsonArray{}}}).toJson();
		} else if (query.queryItemValue(QStringLiteral("foto")) == QLatin1String("acme-ps-30")) {
			body = m_photo;
			type = "image/png";
		} else if (query.queryItemValue(QStringLiteral("pieza")) == QLatin1String("acme-ps-30")) {
			if (m_wants_key && !keyed) {
				code = 401;
				body = "{\"error\":\"licencia\"}";
			} else {
				body = m_faspart;
				type = "application/octet-stream";
			}
		} else {
			code = 404;
			body = "{}";
		}
		QByteArray reply = "HTTP/1.1 " + QByteArray::number(code) + (code == 200 ? " OK" : " Error")
				+ "\r\nContent-Type: " + type + "\r\nContent-Length: " + QByteArray::number(body.size())
				+ "\r\nConnection: close\r\n\r\n" + body;
		socket->write(reply);
		socket->disconnectFromHost();
	}

private slots:
	void initTestCase()
	{
		QVERIFY(m_dir.isValid());
		QVERIFY(m_server.listen(QHostAddress::LocalHost));
		connect(&m_server, &QTcpServer::newConnection, this, [this]() {
			while (QTcpSocket *socket = m_server.nextPendingConnection()) {
				connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {answer(socket);});
				connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
			}
		});
		PartsCatalogDialog::setServerUrl(QUrl(QStringLiteral("http://127.0.0.1:%1/catalogo.php")
											  .arg(m_server.serverPort())));

			//The part the catalog offers, as a .faspart with its photo
		QImage image(16, 16, QImage::Format_ARGB32);
		image.fill(Qt::red);
		QBuffer buffer(&m_photo);
		buffer.open(QIODevice::WriteOnly);
		image.save(&buffer, "PNG");
		PartsArchive::Content content;
		content.part.manufacturer = QStringLiteral("Acme");
		content.part.order_number = QStringLiteral("PS-30");
		content.part.description = QStringLiteral("Power supply 24 V");
		content.part.power_loss = 7.5;
		content.files.append({PartFileKind::Image, QStringLiteral("ps-30.png"), m_photo});
		const QString path = m_dir.filePath(QStringLiteral("acme-ps-30.faspart"));
		QString error;
		QVERIFY2(PartsArchive::write(path, content, &error), qPrintable(error));
		QFile file(path);
		QVERIFY(file.open(QIODevice::ReadOnly));
		m_faspart = file.readAll();

		PartsDatabase::setConfiguredPath(m_dir.filePath(QStringLiteral("parts.sqlite")));
		PartsDatabase::setEnabled(true);
	}

	void aPartOfTheCatalogGoesIntoTheDatabase()
	{
		PartsCatalogDialog dialog;
		QSignalSpy listed(&dialog, &PartsCatalogDialog::listShown);
		QVERIFY(listed.wait(10000));
		QCOMPARE(dialog.listed().size(), 1);
		auto *table = dialog.findChild<QTableWidget *>(QStringLiteral("catalog_table"));
		QVERIFY(table);
		QCOMPARE(table->item(0, 2)->text(), QStringLiteral("PS-30"));
			//Its photo comes after the list
		QTRY_VERIFY_WITH_TIMEOUT(!table->item(0, 0)->icon().isNull(), 10000);

		table->selectRow(0);
		QSignalSpy added(&dialog, &PartsCatalogDialog::partsAdded);
		dialog.addSelected();
		QVERIFY(added.wait(10000));
		QCOMPARE(added.first().first().toInt(), 1);
		PartsDatabase database;
		QVERIFY(database.open(PartsDatabase::path()));
		const qint64 id = database.findPart(QStringLiteral("Acme"), QStringLiteral("PS-30"));
		QVERIFY(id > 0);
		QCOMPARE(database.part(id)->power_loss, std::optional<double>(7.5));
		QCOMPARE(database.files(id).size(), 1);

			//Already there: left as it is
		dialog.addSelected();
		QVERIFY(added.wait(10000));
		QCOMPARE(added.last().first().toInt(), 0);
		QVERIFY(dialog.status().contains(QStringLiteral("already")));
	}

	void theAccountIsSentOnceAsked()
	{
		PartsDatabase database;
		QVERIFY(database.open(PartsDatabase::path()));
		QVERIFY(database.removePart(database.findPart(QStringLiteral("Acme"), QStringLiteral("PS-30"))));

		m_wants_key = true;
		PartsCatalogDialog dialog;
		QSignalSpy listed(&dialog, &PartsCatalogDialog::listShown);
		QVERIFY(listed.wait(10000));
		auto *table = dialog.findChild<QTableWidget *>(QStringLiteral("catalog_table"));
		table->selectRow(0);
		QSignalSpy added(&dialog, &PartsCatalogDialog::partsAdded);
		dialog.addSelected();
		QVERIFY(added.wait(10000));
		QCOMPARE(added.last().first().toInt(), 0);
		QVERIFY2(dialog.status().contains(QStringLiteral("sign in")), qPrintable(dialog.status()));

		FaseCadAccount::set(QStringLiteral("taller"), QStringLiteral("clave-del-taller"));
		dialog.addSelected();
		QVERIFY(added.wait(10000));
		QCOMPARE(added.last().first().toInt(), 1);
		FaseCadAccount::set(QString(), QString());
		m_wants_key = false;
	}

	void aSearchAsksTheServer()
	{
		PartsCatalogDialog dialog;
		QSignalSpy listed(&dialog, &PartsCatalogDialog::listShown);
		QVERIFY(listed.wait(10000));
		auto *search = dialog.findChild<QLineEdit *>(QStringLiteral("catalog_search"));
		QVERIFY(search);
		search->setText(QStringLiteral("nothing like it"));
		dialog.search();
		QVERIFY(listed.wait(10000));
		QCOMPARE(dialog.listed().size(), 0);
		QVERIFY(m_asked.last().contains(QStringLiteral("q=nothing")));
	}
};

QET_APP_TEST_MAIN(tst_partscatalog)
#include "tst_partscatalog.moc"
