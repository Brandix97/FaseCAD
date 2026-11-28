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
#include "pluginmanagerdialog.h"

#include "pluginlibrary.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

PluginManagerDialog::PluginManagerDialog(QWidget *parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Plugins"));
	resize(820, 520);
	auto *layout = new QVBoxLayout(this);

	auto *intro = new QLabel(tr("Plugins add their own commands to FaseCAD: buttons, windows, "
								"exports and reports made for a customer. Untick a plugin to switch "
								"it off without removing it."), this);
	intro->setWordWrap(true);
	layout->addWidget(intro);

	m_list = new QTreeWidget(this);
	m_list->setObjectName(QStringLiteral("plugins_list"));
	m_list->setHeaderLabels({tr("Plugin"), tr("Version"), tr("Author"), tr("Commands")});
	m_list->setRootIsDecorated(false);
	m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	layout->addWidget(m_list, 2);

	m_details = new QLabel(this);
	m_details->setWordWrap(true);
	m_details->setTextFormat(Qt::RichText);
	m_details->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	m_details->setMinimumHeight(110);
	layout->addWidget(m_details, 1);

	auto *buttons_row = new QHBoxLayout;
	auto *install = new QPushButton(tr("Install..."), this);
	install->setObjectName(QStringLiteral("plugins_install"));
	m_remove = new QPushButton(tr("Remove"), this);
	m_remove->setObjectName(QStringLiteral("plugins_remove"));
	auto *open_folder = new QPushButton(tr("Open the plugins folder"), this);
	auto *reload = new QPushButton(tr("Reload"), this);
	reload->setToolTip(tr("Read the plugins folder again, after changing a plugin by hand"));
	buttons_row->addWidget(install);
	buttons_row->addWidget(m_remove);
	buttons_row->addStretch();
	buttons_row->addWidget(reload);
	buttons_row->addWidget(open_folder);
	layout->addLayout(buttons_row);

	auto *close = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(close);

	connect(install, &QPushButton::clicked, this, [this]() {
		QSettings settings;
		const QString path = QFileDialog::getOpenFileName(
					this, tr("Install a plugin"),
					settings.value(QStringLiteral("plugins/install_folder"), QDir::homePath()).toString(),
					tr("FaseCAD plugins (*.fasplugin)"));
		if (path.isEmpty()) return;
		settings.setValue(QStringLiteral("plugins/install_folder"), QFileInfo(path).absolutePath());
		installFile(path);
	});
	connect(m_remove, &QPushButton::clicked, this, [this]() {
		const QString id = selectedId();
		const PluginLibrary::Plugin *plugin = PluginLibrary::instance().find(id);
		if (!plugin) return;
		if (QMessageBox::question(this, tr("Remove the plugin"),
								  tr("Remove the plugin %1 and its files?").arg(plugin->manifest.name))
			!= QMessageBox::Yes)
			return;
		QString error;
		if (!PluginLibrary::instance().uninstall(id, &error))
			QMessageBox::warning(this, tr("Remove the plugin"), error);
	});
	connect(open_folder, &QPushButton::clicked, this, []() {
		QDir().mkpath(PluginLibrary::folder());
		QDesktopServices::openUrl(QUrl::fromLocalFile(PluginLibrary::folder()));
	});
	connect(reload, &QPushButton::clicked, &PluginLibrary::instance(), &PluginLibrary::rescan);
	connect(m_list, &QTreeWidget::itemSelectionChanged, this, &PluginManagerDialog::showDetails);
	connect(m_list, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int column) {
		if (m_filling || column != 0) return;
		PluginLibrary::instance().setEnabled(item->data(0, Qt::UserRole).toString(),
											 item->checkState(0) == Qt::Checked);
	});
	connect(&PluginLibrary::instance(), &PluginLibrary::changed, this, &PluginManagerDialog::fill);
	fill();
}

void PluginManagerDialog::fill()
{
	const QString selected = selectedId();
	m_filling = true;
	m_list->clear();
	for (const PluginLibrary::Plugin &p : PluginLibrary::instance().plugins()) {
		auto *item = new QTreeWidgetItem(m_list);
		item->setText(0, p.manifest.name);
		item->setText(1, p.manifest.version);
		item->setText(2, p.manifest.author);
		item->setText(3, QString::number(p.manifest.commands.size()));
		item->setData(0, Qt::UserRole, p.manifest.id);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(0, p.enabled ? Qt::Checked : Qt::Unchecked);
		if (!p.manifest.commands.isEmpty())
			item->setIcon(0, PluginLibrary::icon(p, p.manifest.commands.first()));
		if (p.manifest.id == selected) item->setSelected(true);
	}
	for (const QString &error : PluginLibrary::instance().errors()) {
		auto *item = new QTreeWidgetItem(m_list);
		item->setText(0, tr("Not loaded: %1").arg(error));
		item->setFlags(Qt::ItemIsEnabled);
		item->setForeground(0, QColor(200, 60, 40));
	}
	m_filling = false;
	showDetails();
}

QString PluginManagerDialog::selectedId() const
{
	const QList<QTreeWidgetItem *> items = m_list ? m_list->selectedItems() : QList<QTreeWidgetItem *>();
	return items.isEmpty() ? QString() : items.first()->data(0, Qt::UserRole).toString();
}

void PluginManagerDialog::showDetails()
{
	const PluginLibrary::Plugin *p = PluginLibrary::instance().find(selectedId());
	m_remove->setEnabled(p);
	if (!p) {
		m_details->setText(PluginLibrary::instance().plugins().isEmpty()
						   ? tr("No plugin installed. Install a .fasplugin file to add one.")
						   : tr("Choose a plugin to see what it does."));
		return;
	}
	QStringList permissions;
	for (const QString &perm : p->manifest.permissions)
		permissions << PluginManifest::permissionText(perm).toHtmlEscaped();
	QStringList commands;
	for (const PluginManifest::Command &c : p->manifest.commands)
		commands << c.label.simplified().toHtmlEscaped();
	m_details->setText(
		QStringLiteral("<b>%1</b> %2<br>%3<br><br><b>%4</b> %5<br><b>%6</b> %7<br><b>%8</b> %9")
			.arg(p->manifest.name.toHtmlEscaped(), p->manifest.version.toHtmlEscaped(),
				 p->manifest.description.toHtmlEscaped(),
				 tr("Commands:"), commands.join(QStringLiteral(", ")),
				 tr("It may:"), permissions.isEmpty() ? tr("only work on the open project")
													   : permissions.join(QStringLiteral(", ")),
				 tr("Folder:"), QDir::toNativeSeparators(p->path).toHtmlEscaped()));
}

bool PluginManagerDialog::installFile(const QString &path, bool ask)
{
	QString error;
	const PluginManifest manifest = PluginLibrary::readArchive(path, &error);
	if (!error.isEmpty()) {
		if (ask) QMessageBox::warning(this, tr("Install a plugin"), error);
		return false;
	}
	const bool installed = PluginLibrary::instance().find(manifest.id);
	if (ask) {
		QStringList permissions;
		for (const QString &perm : manifest.permissions)
			permissions << QStringLiteral("• ") + PluginManifest::permissionText(perm);
		QString text = tr("Install %1 %2 by %3?").arg(manifest.name, manifest.version,
													   manifest.author.isEmpty() ? tr("an unknown author")
																				 : manifest.author);
		if (!manifest.description.isEmpty()) text += QStringLiteral("\n\n") + manifest.description;
		text += QStringLiteral("\n\n") + (permissions.isEmpty()
				? tr("It only works on the open project.")
				: tr("Besides the open project, it may:") + QLatin1Char('\n') + permissions.join(QLatin1Char('\n')));
		text += QStringLiteral("\n\n") + tr("Install only plugins from someone you trust.");
		if (installed) text += QStringLiteral("\n\n") + tr("It replaces the plugin installed with this id.");
		if (QMessageBox::question(this, tr("Install a plugin"), text) != QMessageBox::Yes)
			return false;
	}
	QString id;
	if (!PluginLibrary::instance().install(path, true, &id, &error)) {
		if (ask) QMessageBox::warning(this, tr("Install a plugin"), error);
		return false;
	}
	return true;
}
