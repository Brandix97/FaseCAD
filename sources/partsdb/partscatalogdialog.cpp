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
#include "partscatalogdialog.h"

#include "partsarchive.h"
#include "partsdatabase.h"
#include "partsmanagerdialog.h"
#include "partssymbolfiles.h"
#include "../ui/accountbuttons.h"
#include "../utils/fasecadaccount.h"
#include "../utils/fasecadservices.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrlQuery>
#include <QVBoxLayout>

#include <memory>

namespace {
	const QString url_key = QStringLiteral("parts-catalog/url");
	const QString default_url = QStringLiteral("https://api.fasecad.com/catalogo.php");
	constexpr int photo_side = 40;
	enum Column { Photo, Manufacturer, OrderNumber, Description, Category, Columns };
}

QUrl PartsCatalogDialog::serverUrl()
{
	QSettings settings;
	return QUrl(settings.value(url_key, default_url).toString());
}

void PartsCatalogDialog::setServerUrl(const QUrl &url)
{
	QSettings settings;
	if (url.isEmpty() || url.toString() == default_url)
		settings.remove(url_key);
	else
		settings.setValue(url_key, url.toString());
}

QIcon PartsCatalogDialog::icon()
{
		//A mark till the catalog has an icon of its own
	QPixmap pixmap(32, 32);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(QPen(QColor(250, 19, 12), 2, Qt::DashLine));
	painter.setBrush(Qt::NoBrush);
	painter.drawRoundedRect(QRectF(3, 3, 26, 26), 5, 5);
	QFont font = painter.font();
	font.setPixelSize(12);
	font.setBold(true);
	painter.setFont(font);
	painter.setPen(QColor(250, 19, 12));
	painter.drawText(QRectF(3, 3, 26, 26), Qt::AlignCenter, QStringLiteral("FC"));
	painter.end();
	return QIcon(pixmap);
}

PartsCatalogDialog::PartsCatalogDialog(QWidget *parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Official FaseCAD parts catalog"));
	setWindowIcon(icon());
	m_network = new QNetworkAccessManager(this);

	auto *layout = new QVBoxLayout(this);
	auto *filters = new QHBoxLayout();
	m_search = new QLineEdit(this);
	m_search->setObjectName(QStringLiteral("catalog_search"));
	m_search->setPlaceholderText(tr("Manufacturer, order number, description..."));
	m_search->setClearButtonEnabled(true);
	m_category = new QComboBox(this);
	m_category->setObjectName(QStringLiteral("catalog_category"));
	m_category->addItem(tr("Every category"), QString());
	m_account = new QPushButton(this);
	m_account->setObjectName(QStringLiteral("catalog_account_pb"));
	showAccount();
	filters->addWidget(m_search, 1);
	filters->addWidget(m_category);
	filters->addWidget(m_account);
	layout->addLayout(filters);

	m_table = new QTableWidget(0, Columns, this);
	m_table->setObjectName(QStringLiteral("catalog_table"));
	m_table->setHorizontalHeaderLabels({QString(), tr("Manufacturer"), tr("Order number"),
										tr("Description"), tr("Category")});
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_table->verticalHeader()->hide();
	m_table->verticalHeader()->setDefaultSectionSize(photo_side + 4);
	m_table->setIconSize(QSize(photo_side, photo_side));
	m_table->horizontalHeader()->setSectionResizeMode(Description, QHeaderView::Stretch);
	m_table->setColumnWidth(Photo, photo_side + 8);
	layout->addWidget(m_table, 1);

	auto *pages = new QHBoxLayout();
	m_previous = new QPushButton(tr("< Previous"), this);
	m_next = new QPushButton(tr("Next >"), this);
	m_status = new QLabel(this);
	m_status->setObjectName(QStringLiteral("catalog_status"));
	m_status->setWordWrap(true);
	pages->addWidget(m_previous);
	pages->addWidget(m_next);
	pages->addWidget(m_status, 1);
	layout->addLayout(pages);

	auto *replace = new QCheckBox(tr("Replace the parts my database has already"), this);
	replace->setObjectName(QStringLiteral("catalog_replace"));
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	m_add = buttons->addButton(tr("Add to my parts database"), QDialogButtonBox::ActionRole);
	m_add->setObjectName(QStringLiteral("catalog_add_pb"));
	m_add->setEnabled(false);
	auto *bottom = new QHBoxLayout();
	bottom->addWidget(replace);
	bottom->addStretch();
	bottom->addWidget(buttons);
	layout->addLayout(bottom);

	m_typing = new QTimer(this);
	m_typing->setSingleShot(true);
	m_typing->setInterval(400);
	connect(m_typing, &QTimer::timeout, this, [this]() {search(1);});
	connect(m_search, &QLineEdit::textEdited, m_typing, qOverload<>(&QTimer::start));
	connect(m_category, qOverload<int>(&QComboBox::activated), this, [this]() {search(1);});
	connect(m_previous, &QPushButton::clicked, this, [this]() {search(m_page - 1);});
	connect(m_next, &QPushButton::clicked, this, [this]() {search(m_page + 1);});
	connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
		m_add->setEnabled(!m_table->selectionModel()->selectedRows().isEmpty() && !m_busy);
	});
	connect(m_add, &QPushButton::clicked, this, &PartsCatalogDialog::addSelected);
	connect(m_account, &QPushButton::clicked, this, &PartsCatalogDialog::signIn);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	setProperty("replace", false);
	connect(replace, &QCheckBox::toggled, this, [this](bool on) {setProperty("replace", on);});

	resize(900, 600);
	QTimer::singleShot(0, this, [this]() {search(1);});
}

QString PartsCatalogDialog::status() const
{
	return m_status->text();
}

QUrl PartsCatalogDialog::urlFor(const QString &query) const
{
	QUrl url = serverUrl();
	QUrlQuery items(query);
	url.setQuery(items);
	return url;
}

QNetworkReply *PartsCatalogDialog::get(const QUrl &url)
{
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("FaseCAD"));
	request.setTransferTimeout(30000);
	FaseCadAccount::applyTo(request);
	return m_network->get(request);
}

void PartsCatalogDialog::search(int page)
{
	m_page = qMax(1, page);
	QUrlQuery query;
	query.addQueryItem(QStringLiteral("lista"), QString());
	if (!m_search->text().trimmed().isEmpty())
		query.addQueryItem(QStringLiteral("q"), m_search->text().trimmed());
	const QString category = m_category->currentData().toString();
	if (!category.isEmpty())
		query.addQueryItem(QStringLiteral("categoria"), category);
	query.addQueryItem(QStringLiteral("pagina"), QString::number(m_page));
	QUrl url = serverUrl();
	url.setQuery(query);
	m_status->setText(tr("Asking %1...").arg(url.host()));
	++m_busy;
	QNetworkReply *reply = get(url);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		--m_busy;
		if (reply->error() != QNetworkReply::NoError) {
			m_status->setText(tr("The catalog cannot be reached: %1").arg(reply->errorString()));
			emit listShown();
			return;
		}
		showList(reply->readAll());
	});
}

void PartsCatalogDialog::showList(const QByteArray &data)
{
	const QJsonObject answer = QJsonDocument::fromJson(data).object();
	if (!answer.contains(QStringLiteral("piezas"))) {
		m_status->setText(tr("The catalog gave no list of parts."));
		emit listShown();
		return;
	}
		//The categories, the chosen one kept
	const QString chosen = m_category->currentData().toString();
	{
		const QSignalBlocker blocker(m_category);
		m_category->clear();
		m_category->addItem(tr("Every category"), QString());
		for (const QJsonValue &category : answer.value(QStringLiteral("categorias")).toArray())
			m_category->addItem(category.toString(), category.toString());
		m_category->setCurrentIndex(qMax(0, m_category->findData(chosen)));
	}

	m_listed.clear();
	m_table->setRowCount(0);
	const QJsonArray parts = answer.value(QStringLiteral("piezas")).toArray();
	m_table->setRowCount(int(parts.size()));
	for (int row = 0 ; row < parts.size() ; ++row) {
		const QJsonObject part = parts.at(row).toObject();
		m_listed << part;
		auto cell = [this, row](int column, const QString &text) {
			m_table->setItem(row, column, new QTableWidgetItem(text));
		};
		cell(Photo, QString());
		cell(Manufacturer, part.value(QStringLiteral("manufacturer")).toString());
		cell(OrderNumber, part.value(QStringLiteral("order_number")).toString());
		const QString description = part.value(QStringLiteral("description")).toString();
		cell(Description, description.isEmpty() ? part.value(QStringLiteral("designation")).toString()
												: description);
		cell(Category, part.value(QStringLiteral("category")).toString());
		const QString id = part.value(QStringLiteral("id")).toString();
		if (part.value(QStringLiteral("photo")).toBool()) askPhoto(row, id);
	}
	const int total = answer.value(QStringLiteral("total")).toInt();
	const int per_page = qMax(1, answer.value(QStringLiteral("por_pagina")).toInt(60));
	m_pages = qMax(1, (total + per_page - 1) / per_page);
	m_previous->setEnabled(m_page > 1);
	m_next->setEnabled(m_page < m_pages);
	m_status->setText(tr("%n part(s) found, page %1 of %2.", "", total).arg(m_page).arg(m_pages));
	emit listShown();
}

void PartsCatalogDialog::askPhoto(int row, const QString &id)
{
	if (m_photos.contains(id)) {
		if (QTableWidgetItem *item = m_table->item(row, Photo)) item->setIcon(QIcon(m_photos.value(id)));
		return;
	}
	QNetworkReply *reply = get(urlFor(QStringLiteral("foto=") + QString::fromLatin1(QUrl::toPercentEncoding(id))));
	connect(reply, &QNetworkReply::finished, this, [this, reply, row, id]() {
		reply->deleteLater();
		if (reply->error() != QNetworkReply::NoError) return;
		QPixmap pixmap;
		if (!pixmap.loadFromData(reply->readAll())) return;
		pixmap = pixmap.scaled(photo_side, photo_side, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		m_photos.insert(id, pixmap);
			//Still the same part on that row
		if (row < m_listed.size() && m_listed.at(row).value(QStringLiteral("id")).toString() == id) {
			if (QTableWidgetItem *item = m_table->item(row, Photo)) item->setIcon(QIcon(pixmap));
		}
	});
}

void PartsCatalogDialog::addSelected()
{
	if (!PartsDatabase::isEnabled()) {
		m_status->setText(tr("The parts database is not in use (Settings > General)."));
		return;
	}
	QList<int> rows;
	for (const QModelIndex &index : m_table->selectionModel()->selectedRows())
		rows << index.row();
	std::sort(rows.begin(), rows.end());
	if (rows.isEmpty()) return;
	m_add->setEnabled(false);
	auto state = std::make_shared<QPair<int, QStringList>>();  //added, failed
	auto left = std::make_shared<int>(int(rows.size()));
	for (const int row : std::as_const(rows)) {
		const QJsonObject listed = m_listed.value(row);
		const QString id = listed.value(QStringLiteral("id")).toString();
		++m_busy;
		QNetworkReply *reply = get(urlFor(QStringLiteral("pieza=") + QString::fromLatin1(QUrl::toPercentEncoding(id))));
		connect(reply, &QNetworkReply::finished, this, [this, reply, listed, state, left]() {
			reply->deleteLater();
			--m_busy;
			const QString name = QStringLiteral("%1 %2").arg(listed.value(QStringLiteral("manufacturer")).toString(),
															 listed.value(QStringLiteral("order_number")).toString()).trimmed();
			const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
			if (code == 409) {
				state->second << tr("%1: your session was closed, the account is open on another computer (Sign in...)").arg(name);
				FaseCadServices::instance()->sessionClosed();
			} else if (code == 402) {
				state->second << tr("%1: the parts of the official catalog are part of FaseCAD Pro (my.fasecad.com)").arg(name);
			} else if (code == 401 || code == 403) {
				state->second << (FaseCadAccount::isSet()
								  ? tr("%1: the catalog does not accept this user and password (Sign in...)").arg(name)
								  : tr("%1: sign in to download it (Sign in...)").arg(name));
			} else if (reply->error() != QNetworkReply::NoError) {
				state->second << QStringLiteral("%1: %2").arg(name, reply->errorString());
			} else {
				QString error;
				const int added = addPart(reply->readAll(), listed, &error);
				if (added > 0) ++state->first;
				else if (added < 0) state->second << QStringLiteral("%1: %2").arg(name, error);
				else state->second << tr("%1: your database has it already").arg(name);
			}
			if (--*left > 0) return;
			if (state->first > 0) emit PartsDatabaseChanges::instance()->changed();
			QString text = tr("%n part(s) added to your parts database.", "", state->first);
			if (!state->second.isEmpty())
				text += QLatin1Char(' ') + tr("Not added: %1").arg(state->second.join(QStringLiteral("; ")));
			m_status->setText(text);
			m_add->setEnabled(!m_table->selectionModel()->selectedRows().isEmpty());
			emit partsAdded(state->first);
		});
	}
}

int PartsCatalogDialog::addPart(const QByteArray &faspart, const QJsonObject &listed, QString *error)
{
	Q_UNUSED(listed)
	QTemporaryDir dir;
	if (!dir.isValid()) {
		if (error) *error = tr("No folder for the downloaded file.");
		return -1;
	}
	const QString path = dir.filePath(QStringLiteral("part.") + PartsArchive::suffix());
	{
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly) || file.write(faspart) != faspart.size()) {
			if (error) *error = tr("The downloaded file cannot be written.");
			return -1;
		}
	}
	const std::optional<PartsArchive::Content> content = PartsArchive::read(path, error);
	if (!content) return -1;
	PartsDatabase database;
	if (!database.open(PartsDatabase::path(), error)) return -1;
	const Part &part = content->part;
	const qint64 existing = part.order_number.isEmpty()
			? 0 : database.findPart(part.manufacturer, part.order_number);
	if (existing && !property("replace").toBool()) return 0;
	const qint64 id = PartsArchive::store(database, *content, existing, &PartsSymbolFiles::write, error);
	return id > 0 ? 1 : -1;
}

void PartsCatalogDialog::signIn()
{
	if (!AccountButtons::signIn(this)) return;
	showAccount();
	search(m_page);
}

void PartsCatalogDialog::showAccount()
{
	if (FaseCadAccount::isSet()) {
		m_account->setText(FaseCadAccount::user());
		m_account->setToolTip(tr("Signed in to fasecad.com as %1. Click to change or sign out.")
							  .arg(FaseCadAccount::user()));
	} else {
		m_account->setText(tr("Sign in..."));
		m_account->setToolTip(tr("Sign in to fasecad.com to download the parts"));
	}
}
