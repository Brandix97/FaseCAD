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
#ifndef PARTSCATALOGDIALOG_H
#define PARTSCATALOGDIALOG_H

#include <QDialog>
#include <QHash>
#include <QJsonObject>
#include <QPixmap>
#include <QUrl>

class QComboBox;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QNetworkReply;
class QPushButton;
class QTableWidget;
class QTimer;

/**
	The official parts catalog of FaseCAD (Brandon, 2026-11-25): the parts
	fasecad.com offers (catalogo.php), searched by text and category, each
	with its photo, put into the user's parts database as a .faspart is.
	The fasecad.com account of the user (FaseCadAccount) goes with each
	request: the server asks for it to download a part.
*/
class PartsCatalogDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit PartsCatalogDialog(QWidget *parent = nullptr);

			/// The address of the catalog (fixed for the user; the tests
			/// change it)
		static QUrl serverUrl();
		static void setServerUrl(const QUrl &url);
			/// The icon of the catalog. Waiting for its own: a mark
		static QIcon icon();

			/// Ask the server for the parts matching what is written
		void search(int page = 1);
			/// Put the parts of the rows selected into the parts database
		void addSelected();

			/// The parts listed, as the server gave them
		QList<QJsonObject> listed() const {return m_listed;}
		bool isBusy() const {return m_busy > 0;}
		QString status() const;

	signals:
			/// A list came, a part was added: for whoever waits on it
		void listShown();
		void partsAdded(int count);

	private:
		void showList(const QByteArray &data);
		void askPhoto(int row, const QString &id);
		void signIn();
		void showAccount();
		QNetworkReply *get(const QUrl &url);
		QUrl urlFor(const QString &query) const;
		int addPart(const QByteArray &faspart, const QJsonObject &listed, QString *error);

		QNetworkAccessManager *m_network = nullptr;
		QLineEdit *m_search = nullptr;
		QComboBox *m_category = nullptr;
		QTableWidget *m_table = nullptr;
		QPushButton *m_add = nullptr;
		QPushButton *m_previous = nullptr;
		QPushButton *m_next = nullptr;
		QLabel *m_status = nullptr;
		QPushButton *m_account = nullptr;
		QTimer *m_typing = nullptr;
		QList<QJsonObject> m_listed;
		QHash<QString, QPixmap> m_photos;
		int m_page = 1;
		int m_pages = 1;
		int m_busy = 0;
};

#endif // PARTSCATALOGDIALOG_H
