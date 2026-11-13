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

#include "partsarchive.h"
#include "partsdatabase.h"

#include <QDialog>
#include <QIcon>
#include <QList>
#include <QPair>

#include <functional>

class QComboBox;
class QFormLayout;
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
	@brief The PartsDatabaseChanges class tells that the parts manager
	wrote the parts database (a part saved or deleted, a file added or
	removed): what shows the parts, such as the parts collection of the
	collection panel, reads them again.
*/
class PartsDatabaseChanges : public QObject
{
	Q_OBJECT

	public:
		static PartsDatabaseChanges *instance();

	signals:
		void changed();
};

/**
	@brief The PartsManagerDialog class is the window where the user keeps
	the parts database : a tree of categories or manufacturers, a searchable
	table of parts, and the form of the selected part with its technical
	data, mounting data, functions, connection points, accessories, image,
	DXF drawing and 3D model.

	The window is not modal, so the user can look up parts while drawing.
	Only one is open at a time : use showManager().
*/
class PartsManagerDialog : public QDialog
{
	Q_OBJECT

	public:
		static void showManager(QWidget *parent);
			/// The window asking for the symbol of a part, which gives
			/// it as an ElementsLocation string (empty when cancelled).
			/// Without one, the symbol is typed.
		static void setSymbolChooser(
				std::function<QString(QWidget *parent)> chooser);
			/// The name and icon a symbol is shown with in the Symbols
			/// tab, from its ElementsLocation string. Without one, the
			/// string itself is shown.
		static void setSymbolDescriber(
				std::function<QPair<QIcon, QString>(const QString &location)> describer);
			/// How the symbols of a part are read for its .faspart, and
			/// put in the collection when one is imported. Without them a
			/// .faspart goes without symbols.
		static void setSymbolArchiver(PartsArchive::SymbolReader reader,
									  PartsArchive::SymbolWriter writer);
		~PartsManagerDialog() override;

	public slots:
		void reject() override;

	protected:
		void closeEvent(QCloseEvent *event) override;

	private:
		explicit PartsManagerDialog(QWidget *parent = nullptr);

		void buildUi();
		QWidget *buildGeneralTab();
		QWidget *buildMountingTab();
		QWidget *buildTechnicalTab();
		QWidget *buildFunctionsTab();
		QWidget *buildConnectionsTab();
		QWidget *buildAccessoriesTab();
		QWidget *buildCableTab();
		QWidget *buildFilesTab();
		QWidget *buildSymbolsTab();
		void exportParts(const QList<qint64> &ids);
		void importParts();
		void addSymbolRow(const QString &location);
		void moveSymbol(int step);
		QLineEdit *addNumberEdit(QFormLayout *layout, const QString &label,
								 std::optional<double> Part::*member);

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

		void addFunctionRow();
		void removeFunctionRow();
		void moveFunctionRow(int delta);
		void setFunctionRow(int row, const PartFunction &function);
		PartFunction functionAt(int row) const;

		void addConnectionRow();
		void removeConnectionRow();
		void connectionsFromFunctions();
		void setConnectionRow(int row, const PartConnection &connection);
		bool connectionAt(int row, PartConnection *connection,
						  int *bad_column) const;

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
		void showField(QWidget *field);
		static QString numberText(const std::optional<double> &value);
		static std::optional<double> numberFromText(const QString &text,
													bool *ok);
		static QString nextText(const QString &text);

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
		QTableWidget *m_functions_table = nullptr;
		QTableWidget *m_connections_table = nullptr;
		QTableWidget *m_accessories_table = nullptr;
		QListWidget *m_files_list = nullptr;
		QListWidget *m_symbols_list = nullptr;
		QLabel *m_files_hint = nullptr;
		QLabel *m_preview_label = nullptr;
		QList<QPushButton *> m_file_buttons;
		QPushButton *m_save_file_pb = nullptr;
		QPushButton *m_remove_file_pb = nullptr;
		QPushButton *m_save_pb = nullptr;
		QPushButton *m_revert_pb = nullptr;
};

#endif // PARTSMANAGERDIALOG_H
