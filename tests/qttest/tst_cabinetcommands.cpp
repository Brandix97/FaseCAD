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

#include "cabinet/cabinetitem.h"
#include "diagram.h"
#include "elementspanelwidget.h"
#include "projectview.h"
#include "qetproject.h"
#include "undocommand/addgraphicsobjectcommand.h"

#include <QAction>
#include <QImage>
#include <QPainter>

/**
	Cabinet construction 2D in the editor window:

	* the context menu of a folio in the projects panel inserts a
	  schematic sheet or a cabinet construction sheet under it, and no
	  longer offers to insert one above;
	* the commands adding the parts of a cabinet and changing its scale
	  work on a cabinet folio only;
	* the DIN rail command starts its tool at once, and a second click on
	  it ends the tool;
	* the parts of a cabinet are painted.
*/
class tst_cabinetcommands : public QObject
{
	Q_OBJECT

	static QAction *actionWithText(QObject *owner, const QString &text)
	{
		for (QAction *a : owner->findChildren<QAction *>())
			if (a->text() == text) return a;
		return nullptr;
	}

	static Diagram *shownDiagram()
	{
		ProjectView *project_view = QetAppTest::editor()->currentProjectView();
		if (!project_view || !project_view->currentDiagram()) return nullptr;
		return project_view->currentDiagram()->diagram();
	}

	static void show(Diagram *diagram)
	{
		diagram->showMe();
		QCoreApplication::processEvents();
	}

	Diagram *m_schematic = nullptr;
	Diagram *m_cabinet = nullptr;

	private slots:
		void panelInsertsACabinetSheetBelow()
		{
			m_schematic = QetAppTest::diagram();
			QVERIFY(m_schematic);
			QETProject *project = m_schematic->project();
			QCOMPARE(project->diagrams().size(), 1);

			auto *panel = QetAppTest::editor()->findChild<ElementsPanelWidget *>();
			QVERIFY(panel);
			QVERIFY(actionWithText(panel, QStringLiteral("Insert a schematic sheet")));
			QVERIFY(actionWithText(panel, QStringLiteral("Insert a cabinet construction sheet")));
			QVERIFY(!actionWithText(panel, QStringLiteral("Insert a sheet number above")));
			QVERIFY(!actionWithText(panel, QStringLiteral("Insert a sheet number below")));

				//What the menu entry asks of the editor, for the folio
				//under the only one there is
			emit panel->requestForNewCabinetDiagramAt(project, 1);
			QCoreApplication::processEvents();
			QCOMPARE(project->diagrams().size(), 2);
			QCOMPARE(project->diagrams().first(), m_schematic);
			m_cabinet = project->diagrams().at(1);
			QVERIFY(m_cabinet->isCabinetFolio());
			QVERIFY(!m_schematic->isCabinetFolio());

				//And a schematic sheet, as before, between the two
			emit panel->requestForNewDiagramAt(project, 1);
			QCoreApplication::processEvents();
			QCOMPARE(project->diagrams().size(), 3);
			QVERIFY(!project->diagrams().at(1)->isCabinetFolio());
			QCOMPARE(project->diagrams().at(2), m_cabinet);
		}

		void commandsWorkOnACabinetFolioOnly()
		{
			QAction *duct = QetAppTest::action(QStringLiteral("cabinet_duct"));
			QAction *rail = QetAppTest::action(QStringLiteral("cabinet_rail"));
			QAction *plate = QetAppTest::action(QStringLiteral("cabinet_plate"));
			QVERIFY(duct && rail && plate);

			show(m_schematic);
			QCOMPARE(shownDiagram(), m_schematic);
			QVERIFY(!duct->isEnabled());
			QVERIFY(!rail->isEnabled());
			QVERIFY(!plate->isEnabled());
			QAction *scale = actionWithText(QetAppTest::editor(),
											QStringLiteral("Scale of the cabinet…"));
			QVERIFY(scale);
			QVERIFY(!scale->isEnabled());

			show(m_cabinet);
			QCOMPARE(shownDiagram(), m_cabinet);
			QVERIFY(duct->isEnabled());
			QVERIFY(rail->isEnabled());
			QVERIFY(plate->isEnabled());
			QVERIFY(scale->isEnabled());
				//The command says the scale the folio has
			QCOMPARE(scale->text(), QStringLiteral("Scale of the cabinet (1:5)…"));

				//The menu of the "Cabinet construction 2D" button holds them
			QAction *menu_action = nullptr;
			for (QAction *a : QetAppTest::editor()->findChildren<QAction *>()) {
				if (a->menu() && a->menu()->actions().contains(duct)) menu_action = a;
			}
			QVERIFY(menu_action);
			QVERIFY(menu_action->isEnabled());
			QVERIFY(menu_action->menu()->actions().contains(rail));
			QVERIFY(menu_action->menu()->actions().contains(plate));
			QVERIFY(menu_action->menu()->actions().contains(scale));
			show(m_schematic);
			QVERIFY(!menu_action->isEnabled());
		}

		void railCommandStartsItsTool()
		{
			QAction *rail = QetAppTest::action(QStringLiteral("cabinet_rail"));
			show(m_cabinet);
			QVERIFY(!m_cabinet->eventInterfaceIsRunning());

				//No question asked: the rail is drawn at once
			rail->trigger();
			QVERIFY(rail->isChecked());
			QVERIFY(m_cabinet->eventInterfaceIsRunning());

				//A second click on the command ends the tool
			rail->trigger();
			QVERIFY(!rail->isChecked());
			QVERIFY(!m_cabinet->eventInterfaceIsRunning());

				//Escape ends it too, and the command is no longer checked
			rail->trigger();
			QVERIFY(m_cabinet->eventInterfaceIsRunning());
			QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
			QCoreApplication::sendEvent(m_cabinet, &escape);
			QCoreApplication::processEvents();
			QVERIFY(!m_cabinet->eventInterfaceIsRunning());
			QVERIFY(!rail->isChecked());
		}

		void partsArePainted()
		{
			show(m_cabinet);
			auto add = [this](CabinetItem::Kind kind, qreal length, qreal width,
							  Qt::Orientation orientation, const QPointF &pos) {
				CabinetItem::Properties properties;
				properties.kind = kind;
				properties.length = length;
				properties.width = width;
				properties.orientation = orientation;
				if (kind == CabinetItem::Kind::DinRail)
					properties.profile = CabinetItem::defaultRailProfile();
				if (kind == CabinetItem::Kind::WireDuct)
					properties.depth = 60;
				auto *part = new CabinetItem(properties);
				m_cabinet->undoStack().push(
							new AddGraphicsObjectCommand(part, m_cabinet, pos));
				return part;
			};
				//A plate of 600 x 800 at 1:5, ducts around it, two rails
			CabinetItem *plate = add(CabinetItem::Kind::MountingPlate, 600, 800,
									 Qt::Horizontal, QPointF(100, 60));
			add(CabinetItem::Kind::WireDuct, 600, 40, Qt::Horizontal, QPointF(100, 60));
			add(CabinetItem::Kind::WireDuct, 720, 40, Qt::Vertical, QPointF(100, 76));
			add(CabinetItem::Kind::WireDuct, 720, 40, Qt::Vertical, QPointF(324, 76));
			add(CabinetItem::Kind::WireDuct, 520, 60, Qt::Horizontal, QPointF(116, 356));
			add(CabinetItem::Kind::DinRail, 500, 35, Qt::Horizontal, QPointF(120, 120));
			add(CabinetItem::Kind::DinRail, 500, 35, Qt::Horizontal, QPointF(120, 220));

			const QRectF area = plate->sceneRect().adjusted(-20, -20, 20, 20);
			const qreal zoom = 4.0;
			QImage image((area.size() * zoom).toSize(), QImage::Format_ARGB32);
			image.fill(Qt::white);
			{
				QPainter painter(&image);
				painter.setRenderHint(QPainter::Antialiasing, true);
				m_cabinet->render(&painter, QRectF(image.rect()), area);
			}
				//Something of the plate, of a duct and of a rail was drawn
			auto painted = [&](const QPointF &scene_point) {
				const QPointF p = (scene_point - area.topLeft()) * zoom;
				return image.pixelColor(p.toPoint()) != QColor(Qt::white);
			};
			QVERIFY(painted(QPointF(200, 300)));   //plate
			QVERIFY(painted(QPointF(200, 64)));    //duct along the top
			QVERIFY(painted(QPointF(140, 122)));   //rail

				//For whoever wants to look at it
			const QByteArray shot = qgetenv("QET_CABINET_SHOT");
			if (!shot.isEmpty()) QVERIFY(image.save(QString::fromLocal8Bit(shot)));
		}
};

QET_APP_TEST_MAIN(tst_cabinetcommands)

#include "tst_cabinetcommands.moc"
