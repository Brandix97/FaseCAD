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
#include "ElementsCollection/elementslocation.h"
#include "ElementsCollection/xmlelementcollection.h"
#include "diagram.h"
#include "exportproperties.h"
#include "factory/elementfactory.h"
#include "qetgraphicsitem/element.h"
#include "qetproject.h"

#include <QDomDocument>
#include <QImage>
#include <QPainter>
#include <QUuid>

/**
	The names a symbol shows beside its connection points (the 1 and 2 of
	a fuse) are printed and exported as they are seen (Brandon,
	2026-11-20): the print and export windows had an option, off by
	default, which left them out of the sheet.
*/
class tst_terminalnamesprinted : public QObject
{
	Q_OBJECT

	int m_made = 0;

		/// A symbol with one connection point named "1", its name shown
		/// or not, placed on @p diagram
	Element *place(Diagram *diagram, bool show_name)
	{
		const QString name = QStringLiteral("fuse%1").arg(++m_made);
		const QString xml = QStringLiteral(
				"<definition type=\"element\" link_type=\"simple\" "
				"width=\"20\" height=\"40\" hotspot_x=\"10\" hotspot_y=\"20\">"
				"<uuid uuid=\"%1\"/>"
				"<names><name lang=\"en\">Fuse</name></names>"
				"<description>"
				"<terminal x=\"0\" y=\"-20\" orientation=\"n\" name=\"1\" show_name=\"%2\" "
				"label_x=\"1\" label_y=\"2\" label_halign=\"1\" label_valign=\"128\" "
				"label_rotation=\"0\" label_font=\"Liberation Sans,5,-1,0,50,0,0,0,0,0,Regular\" "
				"type=\"Generic\" uuid=\"%3\"/>"
				"</description></definition>")
				.arg(QUuid::createUuid().toString(),
					 show_name ? QStringLiteral("true") : QStringLiteral("false"),
					 QUuid::createUuid().toString());
		QDomDocument definition;
		if (!definition.setContent(xml)
			|| !diagram->project()->embeddedElementCollection()->addElementDefinition(
					QStringLiteral("import"), name, definition.documentElement())) {
			return nullptr;
		}
		int state = 0;
		Element *element = ElementFactory::Instance()->createElement(
					ElementsLocation(QStringLiteral("embed://import/%1.elmt").arg(name),
									 diagram->project()),
					nullptr, &state);
		if (state || !element) {
			delete element;
			return nullptr;
		}
		diagram->addItem(element);
		element->setPos(200 + 100 * m_made, 200);
		return element;
	}

		/// What the sheet shows around @p element, drawn as printed
	static QImage printed(Diagram *diagram, Element *element)
	{
		QImage image(300, 300, QImage::Format_ARGB32);
		image.fill(Qt::white);
		QPainter painter(&image);
		diagram->render(&painter, QRectF(image.rect()),
						element->sceneBoundingRect().adjusted(-10, -10, 10, 10),
						Qt::KeepAspectRatio);
		return image;
	}

private slots:
	void theNamesOfTheTerminalsArePrinted()
	{
		QETProject project;
		Diagram *folio = project.addNewDiagram();
		Element *named = place(folio, true);
		Element *unnamed = place(folio, false);
		QVERIFY(named && unnamed);

			//As the print and export windows draw a folio: no terminals
		ExportProperties print;
		print.draw_terminals = false;
		print.draw_grid = false;
		print.draw_guides = false;
		const ExportProperties before = folio->applyProperties(print);
		const QImage with_name = printed(folio, named);
		const QImage without = printed(folio, unnamed);
		folio->applyProperties(before);

			//The two symbols differ only by the name shown: it is there
		QVERIFY(with_name != without);
	}
};

QET_APP_TEST_MAIN(tst_terminalnamesprinted)
#include "tst_terminalnamesprinted.moc"
