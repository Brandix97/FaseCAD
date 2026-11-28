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
#include "partcategories.h"

#include "partsdatabase.h"

#include <QApplication>
#include <QBuffer>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QListWidget>
#include <QTreeWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
		///Role of an item holding the name of its category
	const int name_role = Qt::UserRole;
		///An icon of a file larger than this is refused: it is kept in
		///the database, read for every category shown
	const qint64 largest_icon_file = 512 * 1024;
}

QIcon PartCategoryIcons::icon(const QByteArray &data)
{
	if (data.isEmpty()) {
		return QIcon();
	}
	QPixmap pixmap;
	const bool svg = data.contains("<svg");
	if (!pixmap.loadFromData(data, svg ? "SVG" : nullptr)) {
		return QIcon();
	}
	return QIcon(pixmap);
}

QIcon PartCategoryIcons::smallIcon(const QByteArray &data, int side)
{
	QImage image;
	if (data.isEmpty() || !image.loadFromData(data, data.contains("<svg") ? "SVG" : nullptr)) {
		return QIcon();
	}
	return sized(image, side);
}

QIcon PartCategoryIcons::sized(const QImage &image, int side)
{
	if (image.isNull()) {
		return QIcon();
	}
	QIcon icon;
		//The picture at 1 first: its size is the size of the icon
	for (const qreal ratio : {1.0, 1.25, 1.5, 1.75, 2.0, 3.0}) {
		const int pixels = qRound(side * ratio);
		QPixmap pixmap = QPixmap::fromImage(image.scaled(pixels, pixels, Qt::KeepAspectRatio,
														 Qt::SmoothTransformation));
		pixmap.setDevicePixelRatio(ratio);
		icon.addPixmap(pixmap);
	}
	return icon;
}

QList<PartCategoryIcons::Builtin> PartCategoryIcons::builtins()
{
	return {
		{QStringLiteral("terminal_block"), QCoreApplication::translate("PartCategoryIcons", "Terminal block")},
		{QStringLiteral("circuit_breaker"), QCoreApplication::translate("PartCategoryIcons", "Circuit breaker")},
		{QStringLiteral("rcd"), QCoreApplication::translate("PartCategoryIcons", "Residual current device")},
		{QStringLiteral("motor_protector"), QCoreApplication::translate("PartCategoryIcons", "Motor protection")},
		{QStringLiteral("fuse"), QCoreApplication::translate("PartCategoryIcons", "Fuse")},
		{QStringLiteral("contactor"), QCoreApplication::translate("PartCategoryIcons", "Contactor")},
		{QStringLiteral("relay"), QCoreApplication::translate("PartCategoryIcons", "Relay")},
		{QStringLiteral("disconnector"), QCoreApplication::translate("PartCategoryIcons", "Disconnector")},
		{QStringLiteral("pilot_light"), QCoreApplication::translate("PartCategoryIcons", "Pilot light")},
		{QStringLiteral("push_button"), QCoreApplication::translate("PartCategoryIcons", "Push button")},
		{QStringLiteral("signal_tower"), QCoreApplication::translate("PartCategoryIcons", "Signal tower")},
		{QStringLiteral("hmi"), QCoreApplication::translate("PartCategoryIcons", "Operator panel")},
		{QStringLiteral("plc"), QCoreApplication::translate("PartCategoryIcons", "PLC")},
		{QStringLiteral("drive"), QCoreApplication::translate("PartCategoryIcons", "Drive")},
		{QStringLiteral("power_supply"), QCoreApplication::translate("PartCategoryIcons", "Power supply")},
		{QStringLiteral("signal_conditioner"), QCoreApplication::translate("PartCategoryIcons", "Signal conditioner")},
		{QStringLiteral("network"), QCoreApplication::translate("PartCategoryIcons", "Network")},
		{QStringLiteral("sensor"), QCoreApplication::translate("PartCategoryIcons", "Sensor")},
		{QStringLiteral("cable"), QCoreApplication::translate("PartCategoryIcons", "Cable")},
		{QStringLiteral("connector"), QCoreApplication::translate("PartCategoryIcons", "Connector")},
		{QStringLiteral("ferrule"), QCoreApplication::translate("PartCategoryIcons", "Wire end ferrule")},
		{QStringLiteral("busbar"), QCoreApplication::translate("PartCategoryIcons", "Busbar")},
		{QStringLiteral("din_rail"), QCoreApplication::translate("PartCategoryIcons", "DIN rail")},
		{QStringLiteral("enclosure"), QCoreApplication::translate("PartCategoryIcons", "Enclosure")},
		{QStringLiteral("fan"), QCoreApplication::translate("PartCategoryIcons", "Fan")},
		{QStringLiteral("lighting"), QCoreApplication::translate("PartCategoryIcons", "Lighting")},
		{QStringLiteral("tools"), QCoreApplication::translate("PartCategoryIcons", "Tools")},
		{QStringLiteral("pneumatics"), QCoreApplication::translate("PartCategoryIcons", "Pneumatics")},
		{QStringLiteral("hydraulics"), QCoreApplication::translate("PartCategoryIcons", "Hydraulics")},
		{QStringLiteral("transformer"), QCoreApplication::translate("PartCategoryIcons", "Transformer")},
		{QStringLiteral("motor"), QCoreApplication::translate("PartCategoryIcons", "Motor")},
		{QStringLiteral("other"), QCoreApplication::translate("PartCategoryIcons", "Other")}
	};
}

QByteArray PartCategoryIcons::builtinData(const QString &id)
{
	QFile file(QStringLiteral(":/ico/categories/%1.svg").arg(id));
	if (id.isEmpty() || !file.open(QIODevice::ReadOnly)) {
		return QByteArray();
	}
	return file.readAll();
}

PartCategoriesDialog::PartCategoriesDialog(PartsDatabase &database, QWidget *parent) :
	QDialog(parent),
	m_db(database)
{
	setWindowTitle(tr("Categories of the parts"));
	resize(500, 560);

		//A tree: a category, then its subcategories (Brandon, 2026-11-21)
	m_tree = new QTreeWidget(this);
	m_tree->setObjectName(QStringLiteral("categories_tree"));
	m_tree->setHeaderHidden(true);
	m_tree->setIconSize(QSize(28, 28));

	auto *add_pb = new QPushButton(tr("Add..."), this);
	add_pb->setObjectName(QStringLiteral("add_category_pb"));
	auto *sub_pb = new QPushButton(tr("Add a subcategory..."), this);
	sub_pb->setObjectName(QStringLiteral("add_subcategory_pb"));
	auto *rename_pb = new QPushButton(tr("Rename..."), this);
	rename_pb->setObjectName(QStringLiteral("rename_category_pb"));
	auto *icon_pb = new QPushButton(tr("Icon"), this);
	icon_pb->setObjectName(QStringLiteral("icon_category_pb"));
	auto *icon_menu = new QMenu(icon_pb);
	icon_menu->addAction(tr("FaseCAD icons..."), this, &PartCategoriesDialog::chooseBuiltin);
	icon_menu->addAction(tr("Image file..."), this, &PartCategoriesDialog::chooseFile);
	icon_menu->addAction(tr("No icon"), this, [this]() {
		setIcon(current(), QByteArray());
	});
	icon_pb->setMenu(icon_menu);
	auto *remove_pb = new QPushButton(tr("Remove"), this);
	remove_pb->setObjectName(QStringLiteral("remove_category_pb"));
	m_on_selection = {sub_pb, rename_pb, icon_pb, remove_pb};

	connect(add_pb, &QPushButton::clicked, this, &PartCategoriesDialog::add);
	connect(sub_pb, &QPushButton::clicked, this, &PartCategoriesDialog::addSubcategory);
	connect(rename_pb, &QPushButton::clicked, this, &PartCategoriesDialog::rename);
	connect(remove_pb, &QPushButton::clicked, this, &PartCategoriesDialog::remove);
	connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &PartCategoriesDialog::chooseBuiltin);
	connect(m_tree, &QTreeWidget::currentItemChanged, this, [this]() {updateButtons();});

	auto *buttons_layout = new QVBoxLayout();
	buttons_layout->addWidget(add_pb);
	buttons_layout->addWidget(sub_pb);
	buttons_layout->addWidget(rename_pb);
	buttons_layout->addWidget(icon_pb);
	buttons_layout->addWidget(remove_pb);
	buttons_layout->addStretch();

	auto *top = new QHBoxLayout();
	top->addWidget(m_tree, 1);
	top->addLayout(buttons_layout);

	auto *close_box = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(close_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto *layout = new QVBoxLayout(this);
	layout->addLayout(top, 1);
	layout->addWidget(close_box);

	reload();
}

QString PartCategoriesDialog::current() const
{
	const QTreeWidgetItem *item = m_tree->currentItem();
	return item ? item->data(0, name_role).toString() : QString();
}

/**
	@brief PartCategoriesDialog::reload
	List the categories again, in a tree, @p select current
*/
void PartCategoriesDialog::reload(const QString &select)
{
	const QString keep = select.isEmpty() ? current() : select;
	m_tree->clear();
	QHash<QString, QTreeWidgetItem *> items;
	for (const PartCategory &category : m_db.categoryList()) {
		QTreeWidgetItem *parent = items.value(PartCategoryPath::parent(category.name).toLower());
		auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
		item->setText(0, PartCategoryPath::leaf(category.name));
		item->setIcon(0, PartCategoryIcons::icon(category.icon));
		item->setData(0, name_role, category.name);
		const int own = m_db.partsInCategory(category.name);
		const int all = m_db.partsInCategory(category.name, true);
		item->setToolTip(0, all == own ? tr("%n part(s)", "", own)
									   : tr("%n part(s) with its subcategories", "", all));
		items.insert(category.name.toLower(), item);
		if (category.name.compare(keep, Qt::CaseInsensitive) == 0) {
			m_tree->setCurrentItem(item);
		}
	}
	m_tree->expandAll();
	updateButtons();
}

/**
	@brief PartCategoriesDialog::updateButtons
	The buttons of the category selected; Cables, which comes with
	FaseCAD, is neither renamed nor removed (Brandon, 2026-11-27)
*/
void PartCategoriesDialog::updateButtons()
{
	const bool fixed = PartsDatabase::isDefaultCategory(current());
	for (QPushButton *button : std::as_const(m_on_selection)) {
		const QString name = button->objectName();
		const bool kept = name == QLatin1String("rename_category_pb")
				|| name == QLatin1String("remove_category_pb");
		button->setEnabled(m_tree->currentItem() && !(kept && fixed));
		if (kept)
			button->setToolTip(fixed ? tr("This category comes with FaseCAD") : QString());
	}
}

bool PartCategoriesDialog::warn(bool ok, const QString &error)
{
	if (!ok) {
		QMessageBox::warning(this, windowTitle(), error);
	}
	return ok;
}

/**
	@brief PartCategoriesDialog::follow
	The paths of @p moved changed: those the caller was told of before
	follow them
*/
void PartCategoriesDialog::follow(const QHash<QString, QString> &moved)
{
	for (auto it = m_renamed.begin(); it != m_renamed.end(); ++it) {
		for (auto change = moved.cbegin() ; change != moved.cend() ; ++change) {
			if (it.value().compare(change.key(), Qt::CaseInsensitive) == 0) {
				it.value() = change.value();
				break;
			}
		}
	}
	for (auto change = moved.cbegin() ; change != moved.cend() ; ++change) {
		if (!m_renamed.contains(change.key())) {
			m_renamed.insert(change.key(), change.value());
		}
	}
}

void PartCategoriesDialog::addCategory(const QString &name)
{
	QString error;
	if (warn(m_db.addCategory(name, &error), error)) {
		m_changed = true;
		reload(PartCategoryPath::normalized(name));
	}
}

void PartCategoriesDialog::renameCategory(const QString &name, const QString &new_name)
{
	QString error;
	QHash<QString, QString> moved;
	if (warn(m_db.renameCategory(name, new_name, &error, &moved), error)) {
		m_changed = true;
		follow(moved);
		reload(PartCategoryPath::normalized(new_name));
	}
}

void PartCategoriesDialog::removeCategory(const QString &name)
{
	QString error;
	QHash<QString, QString> moved;
	if (warn(m_db.removeCategory(name, &error, &moved), error)) {
		m_changed = true;
		follow(moved);
		reload(PartCategoryPath::parent(name));
	}
}

void PartCategoriesDialog::setIcon(const QString &name, const QByteArray &icon)
{
	if (name.isEmpty()) {
		return;
	}
	QString error;
	if (warn(m_db.setCategoryIcon(name, icon, &error), error)) {
		m_changed = true;
		reload(name);
	}
}

void PartCategoriesDialog::add()
{
	bool ok = false;
	const QString name = QInputDialog::getText(this, tr("New category"),
											   tr("Name of the category:"),
											   QLineEdit::Normal, QString(), &ok);
	if (ok && !name.trimmed().isEmpty()) {
		addCategory(name);
	}
}

void PartCategoriesDialog::addSubcategory()
{
	const QString parent = current();
	if (parent.isEmpty()) {
		return;
	}
	bool ok = false;
	const QString name = QInputDialog::getText(this, tr("New subcategory"),
											   tr("Name of the subcategory of %1:").arg(parent),
											   QLineEdit::Normal, QString(), &ok);
	if (ok && !name.trimmed().isEmpty()) {
		addCategory(parent + PartCategoryPath::separator() + name.trimmed());
	}
}

void PartCategoriesDialog::rename()
{
	const QString name = current();
	if (name.isEmpty()) {
		return;
	}
	bool ok = false;
	const QString leaf = PartCategoryPath::leaf(name);
	const QString new_leaf = QInputDialog::getText(this, tr("Rename the category"),
												   tr("New name of the category:"),
												   QLineEdit::Normal, leaf, &ok);
	if (ok && !new_leaf.trimmed().isEmpty() && new_leaf.trimmed() != leaf) {
		const QString parent = PartCategoryPath::parent(name);
		renameCategory(name, parent.isEmpty()
					   ? new_leaf.trimmed()
					   : parent + PartCategoryPath::separator() + new_leaf.trimmed());
	}
}

void PartCategoriesDialog::remove()
{
	const QString name = current();
	if (name.isEmpty()) {
		return;
	}
	const int count = m_db.partsInCategory(name);
	const QString parent = PartCategoryPath::parent(name);
	QStringList said{tr("Remove the category %1?").arg(name)};
	if (count) {
		said << (parent.isEmpty()
				 ? tr("Its %n part(s) are left without category.", "", count)
				 : tr("Its %n part(s) go to %1.", "", count).arg(parent));
	}
	if (m_db.partsInCategory(name, true) > count
		|| (m_tree->currentItem() && m_tree->currentItem()->childCount())) {
		said << tr("Its subcategories go up one level, with their parts.");
	}
	if (QMessageBox::question(this, tr("Remove the category"), said.join(QLatin1Char(' ')))
			== QMessageBox::Yes) {
		removeCategory(name);
	}
}

void PartCategoriesDialog::chooseBuiltin()
{
	const QString name = current();
	if (name.isEmpty()) {
		return;
	}
	PartCategoryIconPicker picker(this);
	if (picker.exec() == QDialog::Accepted && !picker.picked().isEmpty()) {
		setIcon(name, PartCategoryIcons::builtinData(picker.picked()));
	}
}

/**
	@brief PartCategoriesDialog::chooseFile
	An image of the user as the icon: an SVG as it is, another image made
	a PNG no larger than 64 pixels, to keep the database small
*/
void PartCategoriesDialog::chooseFile()
{
	const QString name = current();
	if (name.isEmpty()) {
		return;
	}
	const QString path = QFileDialog::getOpenFileName(
				this, tr("Icon of the category"), QString(),
				tr("Images (*.svg *.png *.jpg *.jpeg *.bmp *.ico)"));
	if (path.isEmpty()) {
		return;
	}
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly) || file.size() > largest_icon_file) {
		warn(false, tr("The file %1 cannot be read or is larger than 512 KB.")
			 .arg(QFileInfo(path).fileName()));
		return;
	}
	QByteArray data = file.readAll();
	if (!data.contains("<svg")) {
		QImage image;
		if (!image.loadFromData(data)) {
			warn(false, tr("The file %1 is not an image.").arg(QFileInfo(path).fileName()));
			return;
		}
		if (image.width() > 64 || image.height() > 64) {
			image = image.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		}
		data.clear();
		QBuffer buffer(&data);
		buffer.open(QIODevice::WriteOnly);
		image.save(&buffer, "PNG");
	} else if (PartCategoryIcons::icon(data).isNull()) {
		warn(false, tr("The file %1 is not an image.").arg(QFileInfo(path).fileName()));
		return;
	}
	setIcon(name, data);
}

PartCategoryIconPicker::PartCategoryIconPicker(QWidget *parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Icon of the category"));
	resize(560, 420);
	m_list = new QListWidget(this);
	m_list->setObjectName(QStringLiteral("builtin_icons_list"));
	m_list->setViewMode(QListView::IconMode);
	m_list->setIconSize(QSize(48, 48));
	m_list->setGridSize(QSize(104, 84));
	m_list->setResizeMode(QListView::Adjust);
	m_list->setMovement(QListView::Static);
	m_list->setWordWrap(true);
	for (const PartCategoryIcons::Builtin &builtin : PartCategoryIcons::builtins()) {
		auto *item = new QListWidgetItem(
					PartCategoryIcons::icon(PartCategoryIcons::builtinData(builtin.id)),
					builtin.name, m_list);
		item->setData(Qt::UserRole, builtin.id);
	}
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(m_list, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
	auto *layout = new QVBoxLayout(this);
	layout->addWidget(m_list, 1);
	layout->addWidget(buttons);
}

QString PartCategoryIconPicker::picked() const
{
	const QListWidgetItem *item = m_list->currentItem();
	return item ? item->data(Qt::UserRole).toString() : QString();
}
