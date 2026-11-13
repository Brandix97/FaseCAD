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
#include "dataBase/projectdatabase.h"
#include "diagram.h"
#include "factory/folioindex.h"
#include "qetgraphicsitem/ViewItem/qetgraphicstableitem.h"
#include "qetmessagebox.h"
#include "qetproject.h"
#include "titleblockproperties.h"

/**
	The automatic folio index (Lists > Automatic folio index):

	 - one click puts an "Index" folio at the start of the project with a
	   table of the number and title of every folio;
	 - a second click finds it and makes no other;
	 - when the folios do not fit on one index folio, more index folios
	   follow it, their tables continuing the first;
	 - folios added later make the index grow the same way, without the
	   warning other tables give.
*/
class tst_folioindex : public QObject
{
	Q_OBJECT

	static QString titleOf(Diagram *folio)
	{
		return folio->border_and_titleblock.exportTitleBlock().title;
	}

		/// Rows the tables of the index show, from its first table
	static int shownRows(QetGraphicsTableItem *head)
	{
		int shown = 0;
		for (QetGraphicsTableItem *table = head ; table ; table = table->nextTable()) {
			shown += table->displayNRow();
		}
		return shown;
	}

	static int tableCount(QetGraphicsTableItem *head)
	{
		int count = 0;
		for (QetGraphicsTableItem *table = head ; table ; table = table->nextTable()) {
			++count;
		}
		return count;
	}

	static void addFolios(QETProject &project, int count)
	{
		for (int i = 0 ; i < count ; ++i) {
			Diagram *folio = project.addNewDiagram();
			TitleBlockProperties properties = folio->border_and_titleblock.exportTitleBlock();
			properties.title = QStringLiteral("Folio %1").arg(project.diagrams().size());
			folio->border_and_titleblock.importTitleBlock(properties);
		}
	}

	private slots:
		void initTestCase()
		{
			QETProject::setBackupEnabled(false);
				//The warning of a table too short must never show
			QET::QetMessageBox::setNonInteractive(true);
		}

		void oneClickMakesTheIndex()
		{
			QETProject project;
			addFolios(project, 3);
			const int folios = project.diagrams().size();
			Diagram *first = project.diagrams().first();

			Diagram *index = FolioIndex::create(&project);
			QVERIFY(index);
			QCOMPARE(project.diagrams().first(), index);
			QCOMPARE(project.diagrams().at(1), first);
			QCOMPARE(titleOf(index), QStringLiteral("Index"));

			QetGraphicsTableItem *head = FolioIndex::headTable(&project);
			QVERIFY(head);
			QCOMPARE(head->diagram(), index);
			QVERIFY(FolioIndex::isIndex(head));
			project.dataBase()->updateDB();
			QTRY_COMPARE(head->model()->rowCount(), project.diagrams().size());
			QCOMPARE(head->model()->columnCount(), 2);

				//A second click finds it
			QCOMPARE(FolioIndex::create(&project), index);
			QCOMPARE(project.diagrams().size(), folios + 1);
		}

		void manyFoliosTakeMoreIndexFolios()
		{
			QETProject project;
			addFolios(project, 120);
			FolioIndex::create(&project);
			project.dataBase()->updateDB();
			QetGraphicsTableItem *head = FolioIndex::headTable(&project);
			QVERIFY(head);
			QTRY_VERIFY(shownRows(head) >= head->model()->rowCount());
			QVERIFY(tableCount(head) >= 2);
				//The index folios follow each other at the start
			QetGraphicsTableItem *table = head;
			for (int i = 0 ; table ; table = table->nextTable(), ++i) {
				QCOMPARE(project.folioIndex(table->diagram()), i);
				QCOMPARE(titleOf(table->diagram()), QStringLiteral("Index"));
			}
		}

		void foliosAddedLaterMakeItGrow()
		{
			QETProject project;
			addFolios(project, 5);
			FolioIndex::create(&project);
			project.dataBase()->updateDB();
			QetGraphicsTableItem *head = FolioIndex::headTable(&project);
			QVERIFY(head);
			QTRY_COMPARE(head->model()->rowCount(), project.diagrams().size());
			QCOMPARE(tableCount(head), 1);

			addFolios(project, 150);
			project.dataBase()->updateDB();
			QTRY_COMPARE(head->model()->rowCount(), project.diagrams().size());
			QTRY_VERIFY(shownRows(head) >= head->model()->rowCount());
			QVERIFY(tableCount(head) >= 2);
		}
};

QET_APP_TEST_MAIN(tst_folioindex)
#include "tst_folioindex.moc"
