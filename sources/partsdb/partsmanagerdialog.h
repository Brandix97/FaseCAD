/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef PARTSMANAGERDIALOG_H
#define PARTSMANAGERDIALOG_H

#include "partsdatabase.h"

#include <QDialog>
#include <QList>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QSplitter;
class QTabWidget;
class QTableWidget;
class QTreeWidget;

/**
	@brief The PartsManagerDialog class is the window where the user keeps
	the parts database : a tree of categories or manufacturers, a searchable
	table of parts, and the form of the selected part with its technical
	data, accessories, image and DXF drawing.

	The window is not modal, so the user can look up parts while drawing.
	Only one is open at a time : use showManager().
*/
class PartsManagerDialog : public QDialog
{
	Q_OBJECT

	public:
		static void showManager(QWidget *parent);
		~PartsManagerDialog() override;

	public slots:
		void reject() override;

	protected:
		void closeEvent(QCloseEvent *event) override;

	private:
		explicit PartsManagerDialog(QWidget *parent = nullptr);

		void buildUi();
		QWidget *buildGeneralTab();
		QWidget *buildTechnicalTab();
		QWidget *buildAccessoriesTab();
		QWidget *buildCableTab();
		QWidget *buildFilesTab();

		void reloadTree();
		void reloadParts(qint64 select_id = 0);
		void treeSelectionChanged();
		void partSelectionChanged();
		qint64 selectedPartId() const;
		void selectPart(qint64 id);

		void showPart(const Part &part, bool is_new);
		Part partFromForm() const;
		void setModified(bool modified);
		bool maySwitchPart();
		bool save();
		void revert();
		void newPart();
		void duplicatePart();
		void removePart();

		void addTechnicalRow();
		void removeTechnicalRow();
		void addAccessory();
		void removeAccessory();
		void appendAccessoryRow(const PartAccessory &accessory);

		void reloadFiles();
		void fileSelectionChanged();
		void addFile(PartFileKind kind);
		void linkFile();
		void saveFileAs();
		void removeFile();

		void showError(const QString &error);
		static QString numberText(const std::optional<double> &value);
		static std::optional<double> numberFromText(const QString &text,
													bool *ok);

		PartsDatabase m_db;
		bool m_db_ok = false;
		Part m_current;
		bool m_is_new = false;
		bool m_modified = false;
		bool m_filling = false;

		QLabel *m_path_label = nullptr;
		QSplitter *m_splitter = nullptr;
		QComboBox *m_group_cb = nullptr;
		QTreeWidget *m_tree = nullptr;
		QLineEdit *m_search_le = nullptr;
		QTableWidget *m_parts_table = nullptr;
		QPushButton *m_new_pb = nullptr;
		QPushButton *m_duplicate_pb = nullptr;
		QPushButton *m_remove_pb = nullptr;
		QWidget *m_form = nullptr;
		QTabWidget *m_tabs = nullptr;
		struct TextEdit {
			QString Part::*member;
			QLineEdit *edit;
		};
		struct NumberEdit {
			std::optional<double> Part::*member;
			QLineEdit *edit;
		};
		QList<TextEdit> m_text_edits;
		QList<NumberEdit> m_number_edits;
		QPlainTextEdit *m_notes_te = nullptr;
		QSpinBox *m_cores_sb = nullptr;
		QTableWidget *m_technical_table = nullptr;
		QTableWidget *m_accessories_table = nullptr;
		QListWidget *m_files_list = nullptr;
		QLabel *m_files_hint = nullptr;
		QLabel *m_preview_label = nullptr;
		QList<QPushButton *> m_file_buttons;
		QPushButton *m_save_file_pb = nullptr;
		QPushButton *m_remove_file_pb = nullptr;
		QPushButton *m_save_pb = nullptr;
		QPushButton *m_revert_pb = nullptr;
};

#endif // PARTSMANAGERDIALOG_H
