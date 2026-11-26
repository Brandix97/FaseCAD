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
#include "factory/elementfactory.h"
#include "properties/xrefproperties.h"
#include "qetgraphicsitem/crossrefitem.h"
#include "qetgraphicsitem/element.h"
#include "qetproject.h"
#include "ui/elementxrefdisplaywidget.h"
#include "ui/xrefdisplayform.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDomDocument>
#include <QDoubleSpinBox>

/**
	The contact mirror of a relay (Brandon, 2026-11-16):

	 - a manual position is relative to the symbol, which it follows, or
	   absolute on the folio, where it stays put; switching from one to
	   the other in the form keeps the display where it is;
	 - the contact symbols turn by 0, 90, 180 or 270 degrees: turned
	   upright, a contact takes three rows;
	 - both are saved with the master only when used;
	 - the properties of the master have a "Contact mirror" tab, applied
	   as it is changed.
*/
class tst_contactmirror : public QObject
{
	Q_OBJECT

	int m_made = 0;

	Element *place(const QString &link_type, const QString &kinds)
	{
		Diagram *diagram = QetAppTest::diagram();
		const QString name = QStringLiteral("mirror%1").arg(++m_made);
		const QString xml = QStringLiteral(
				"<definition type=\"element\" link_type=\"%1\" "
				"width=\"20\" height=\"40\" hotspot_x=\"10\" hotspot_y=\"20\">"
				"<uuid uuid=\"%2\"/>"
				"<names><name lang=\"en\">Test</name></names>"
				"<kindInformations>%3</kindInformations>"
				"<description><rect x=\"-5\" y=\"-10\" width=\"10\" height=\"20\" "
				"style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
				"<terminal x=\"0\" y=\"-20\" orientation=\"n\" name=\"13\" uuid=\"%4\"/>"
				"<terminal x=\"0\" y=\"20\" orientation=\"s\" name=\"14\" uuid=\"%5\"/>"
				"</description></definition>")
				.arg(link_type, QUuid::createUuid().toString(), kinds,
					 QUuid::createUuid().toString(), QUuid::createUuid().toString());
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

		/// A relay with one normally open contact linked to it
	Element *relay()
	{
		Element *coil = place(QStringLiteral("master"),
							  QStringLiteral("<kindInformation name=\"type\">coil</kindInformation>"));
		Element *contact = place(QStringLiteral("slave"),
								 QStringLiteral("<kindInformation name=\"type\">simple</kindInformation>"
												"<kindInformation name=\"state\">NO</kindInformation>"
												"<kindInformation name=\"number\">1</kindInformation>"));
		if (!coil || !contact) return nullptr;
		coil->linkToElement(contact);
		return coil;
	}

	static CrossRefItem *mirrorOf(Element *master)
	{
		QList<QGraphicsItem *> items = master->childItems();
		while (!items.isEmpty()) {
			QGraphicsItem *item = items.takeFirst();
			if (auto *xref = dynamic_cast<CrossRefItem *>(item)) {
				return xref;
			}
			items << item->childItems();
		}
		return nullptr;
	}

	static void show(Element *master, const ElementData::XRefDisplay &display)
	{
		ElementData data = master->elementData();
		data.m_xref_display = display;
		master->setElementData(data);
		QCoreApplication::processEvents();
	}

	static QPointF topLeftOf(CrossRefItem *xref)
	{
		return xref->mapToScene(xref->boundingRect().topLeft());
	}

	static ElementData::XRefDisplay manualDisplay(const QPointF &at, bool absolute)
	{
		ElementData::XRefDisplay display;
		display.enabled = true;
		display.contacts = true;
		display.position = ElementData::XRefDisplay::Manual;
		display.manual = at;
		display.absolute = absolute;
		return display;
	}

private slots:
	void initTestCase()
	{
		QETProject::setBackupEnabled(false);
		QVERIFY(QetAppTest::diagram());
	}

	void aNewProjectShowsContacts()
	{
		for (const QString &type : {QStringLiteral("coil"), QStringLiteral("protection"),
									QStringLiteral("commutator")}) {
			QCOMPARE(XRefProperties::defaultProperties().value(type).displayHas(),
					 XRefProperties::Contacts);
				//...at the foot of the folio
			QCOMPARE(XRefProperties::defaultProperties().value(type).snapTo(),
					 XRefProperties::Bottom);
		}
			//A file which does not say is read as it was drawn
		QDomDocument document;
		XRefProperties read;
		read.fromXml(document.createElement(QStringLiteral("xref")));
		QCOMPARE(read.displayHas(), XRefProperties::Cross);
		QCOMPARE(read.snapTo(), XRefProperties::Label);
	}

	void theNewOptionsAreSavedOnlyWhenUsed()
	{
		ElementData data;
		data.m_xref_display = manualDisplay(QPointF(30, 40), false);
		QDomDocument document;
		QDomElement plain = data.xrefDisplayToXml(document);
		QVERIFY(!plain.hasAttribute(QStringLiteral("anchor")));
		QVERIFY(!plain.hasAttribute(QStringLiteral("angle")));

		data.m_xref_display.absolute = true;
		data.m_xref_display.angle = 270;
		ElementData read;
		read.xrefDisplayFromXml(data.xrefDisplayToXml(document));
		QVERIFY(read.m_xref_display == data.m_xref_display);

		plain.setAttribute(QStringLiteral("angle"), QStringLiteral("45"));
		read.xrefDisplayFromXml(plain);
		QCOMPARE(read.m_xref_display.angle, 0);
		QVERIFY(!read.m_xref_display.absolute);
	}

	void theFormKeepsThePlaceWhenTheOriginChanges()
	{
		XRefDisplayForm form;
		form.setDisplay(manualDisplay(QPointF(10, 20), false),
						QPointF(100, 200), QPointF());
		auto *anchor = form.findChild<QComboBox *>(QStringLiteral("m_anchor"));
		auto *angle = form.findChild<QComboBox *>(QStringLiteral("m_angle"));
		QVERIFY(anchor && angle);

		anchor->setCurrentIndex(anchor->findData(true));
		ElementData::XRefDisplay display = form.display();
		QVERIFY(display.absolute);
		QCOMPARE(display.manual, QPointF(110, 220));

		angle->setCurrentIndex(angle->findData(90));
		QCOMPARE(form.display().angle, 90);

		anchor->setCurrentIndex(anchor->findData(false));
		QCOMPARE(form.display().manual, QPointF(10, 20));
	}

	void anAbsoluteMirrorStaysAndARelativeOneFollows()
	{
		Element *coil = relay();
		QVERIFY(coil);
		show(coil, manualDisplay(QPointF(300, 400), true));
		CrossRefItem *xref = mirrorOf(coil);
		QVERIFY(xref);
		QCOMPARE(topLeftOf(xref), QPointF(300, 400));
		coil->setPos(coil->pos() + QPointF(50, 0));
		QCoreApplication::processEvents();
		QCOMPARE(topLeftOf(xref), QPointF(300, 400));

		show(coil, manualDisplay(QPointF(10, 30), false));
		xref = mirrorOf(coil);
		QVERIFY(xref);
		QCOMPARE(topLeftOf(xref), coil->scenePos() + QPointF(10, 30));
	}

	void aTurnedContactStandsUpright()
	{
		Element *coil = relay();
		QVERIFY(coil);
		ElementData::XRefDisplay display = manualDisplay(QPointF(10, 30), false);
		show(coil, display);
		CrossRefItem *xref = mirrorOf(coil);
		QVERIFY(xref);
		const QSizeF flat = xref->boundingRect().size();

		display.angle = 90;
		show(coil, display);
		xref = mirrorOf(coil);
		QVERIFY(xref);
		const QSizeF upright = xref->boundingRect().size();
		QVERIFY2(upright.height() > flat.height() + 10,
				 qPrintable(QStringLiteral("%1 / %2").arg(upright.height()).arg(flat.height())));
	}

	void theTabAppliesAsItIsChanged()
	{
		Element *coil = relay();
		QVERIFY(coil);
		QVERIFY(ElementXRefDisplayWidget::isUsefulFor(coil));
		ElementXRefDisplayWidget tab(coil);
		tab.setLiveEdit(true);
		auto *project = tab.findChild<QCheckBox *>(QStringLiteral("m_project_cb"));
		auto *angle = tab.findChild<QComboBox *>(QStringLiteral("m_angle"));
		auto *appearance = tab.findChild<QComboBox *>(QStringLiteral("m_appearance"));
		QVERIFY(project && angle && appearance);
		QVERIFY(project->isChecked());
		QVERIFY(!coil->elementData().m_xref_display.enabled);

		project->setChecked(false);
		QVERIFY(coil->elementData().m_xref_display.enabled);
		appearance->setCurrentIndex(appearance->findData(true));
		angle->setCurrentIndex(angle->findData(180));
		QCOMPARE(coil->elementData().m_xref_display.angle, 180);

		project->setChecked(true);
		QVERIFY(!coil->elementData().m_xref_display.enabled);
	}
};

QET_APP_TEST_MAIN(tst_contactmirror)
#include "tst_contactmirror.moc"
