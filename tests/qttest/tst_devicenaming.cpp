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
#include "devicenaming.h"
#include "diagram.h"
#include "factory/elementfactory.h"
#include "qetgraphicsitem/element.h"
#include "qetinformation.h"
#include "qetproject.h"
#include "partsdb/partsdatabase.h"
#include "partsdb/partslinks.h"

#include <QDomDocument>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QUuid>

/**
	The naming of a placed device, which replaces QElectroTech's numbering
	formulas (DeviceNaming):

	 - the default name is the label the symbol gives itself, its digits
	   left out, else "-" and the prefix of its folder, else none;
	 - a device takes the first number of that name no other device of
	   the project carries, and its label is its own (no formula);
	 - devices placed together take one number each;
	 - a contact, which takes its master's label, is not named.
*/
class tst_devicenaming : public QObject
{
	Q_OBJECT

	int m_position = 0;

	/// A symbol of link type @p link_type whose own label is @p label
	Element *symbol(Diagram *diagram, const QString &link_type,
					const QString &label = QString())
	{
		const QString name = QStringLiteral("symbol%1").arg(m_position);
		const QString information = label.isEmpty()
				? QString()
				: QStringLiteral("<elementInformations><elementInformation "
								 "name=\"label\" show=\"1\">%1</elementInformation>"
								 "</elementInformations>").arg(label);
		const QString xml = QStringLiteral(
				"<definition type=\"element\" link_type=\"%1\" uuid=\"%2\" "
				"width=\"20\" height=\"40\" hotspot_x=\"10\" hotspot_y=\"20\">"
				"<names><name lang=\"en\">Test</name></names>"
				"<kindInformations><kindInformation name=\"type\">%3"
				"</kindInformation></kindInformations>%4"
				"<description><line x1=\"0\" y1=\"-10\" x2=\"0\" y2=\"10\"/>"
				"<terminal x=\"0\" y=\"-20\" orientation=\"n\" uuid=\"%5\"/>"
				"</description></definition>")
				.arg(link_type, QUuid::createUuid().toString(),
					 link_type == QLatin1String("slave") ? QStringLiteral("simple")
														 : QStringLiteral("coil"),
					 information, QUuid::createUuid().toString());
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
		element->setPos(100 + 60 * (m_position % 10), 100 + 120 * (m_position / 10));
		++m_position;
		return element;
	}

	static QString labelOf(const Element *element)
	{
		return element->elementInformations()
				.value(QETInformation::ELMT_LABEL).toString();
	}

	static void setLabel(Element *element, const QString &label)
	{
		DiagramContext information = element->elementInformations();
		information.addValue(QETInformation::ELMT_LABEL, label);
		element->setElementInformations(information);
	}

	private slots:
		void initTestCase()
		{
			QETProject::setBackupEnabled(false);
		}

		void nextLabelIsTheFirstFree()
		{
			QCOMPARE(DeviceNaming::nextLabel(QStringLiteral("-K"), {}),
					 QStringLiteral("-K1"));
			QCOMPARE(DeviceNaming::nextLabel(QStringLiteral("-K"),
											 {"-K1", "-K2", "-K4", "-Q3"}),
					 QStringLiteral("-K3"));
		}

		void defaultNameOfTheSymbol()
		{
			QETProject project;
			Diagram *diagram = project.addNewDiagram();

			Element *own = symbol(diagram, QStringLiteral("simple"),
								  QStringLiteral("-Q7"));
			QVERIFY(own);
			QCOMPARE(DeviceNaming::baseName(own), QStringLiteral("-Q"));

				//"_" is no label: the folder's prefix, behind a dash
			Element *prefixed = symbol(diagram, QStringLiteral("simple"),
									   QStringLiteral("_"));
			QVERIFY(prefixed);
			prefixed->setPrefix(QStringLiteral("K"));
			QCOMPARE(DeviceNaming::baseName(prefixed), QStringLiteral("-K"));
			prefixed->setPrefix(QStringLiteral("-F"));
			QCOMPARE(DeviceNaming::baseName(prefixed), QStringLiteral("-F"));

				//Neither (Brandon, 2026-11-21): "?", to be seen
			Element *none = symbol(diagram, QStringLiteral("simple"));
			QVERIFY(none);
			none->setPrefix(QString());
			QCOMPARE(DeviceNaming::baseName(none), QStringLiteral("-?"));
			DeviceNaming::nameNewElement(none);
			QCOMPARE(labelOf(none), QStringLiteral("-?1"));
		}

		void theLetterOfThePartComesFirst()
		{
				//The device letter of the part in the parts database
				//before the symbol's own name (Brandon, 2026-11-21)
			QTemporaryDir dir;
			QVERIFY(dir.isValid());
			const QString before_path = PartsDatabase::configuredPath();
			const bool before_enabled = PartsDatabase::isEnabled();
			PartsDatabase::setConfiguredPath(dir.filePath(QStringLiteral("parts.sqlite")));
			PartsDatabase::setEnabled(true);
			const auto restore = qScopeGuard([&]() {
				PartsDatabase::setConfiguredPath(before_path);
				PartsDatabase::setEnabled(before_enabled);
			});
			{
				PartsDatabase database;
				QVERIFY(database.open(PartsDatabase::path()));
				Part transformer;
				transformer.manufacturer = QStringLiteral("ACME");
				transformer.order_number = QStringLiteral("TR-230");
				transformer.code_letter = QStringLiteral("T");
				QVERIFY(database.addPart(transformer) > 0);
				Part bare;
				bare.manufacturer = QStringLiteral("ACME");
				bare.order_number = QStringLiteral("NO-LETTER");
				QVERIFY(database.addPart(bare) > 0);
			}
			QCOMPARE(DeviceNaming::partLetter(QStringLiteral("ACME"), QStringLiteral("TR-230")),
					 QStringLiteral("T"));

			QETProject project;
			Diagram *diagram = project.addNewDiagram();
			Element *own = symbol(diagram, QStringLiteral("simple"), QStringLiteral("-Q7"));
			QVERIFY(own);
				//Placed with the part
			QCOMPARE(DeviceNaming::baseName(own, QStringLiteral("T")), QStringLiteral("-T"));
				//Made of it
			DiagramContext information = own->elementInformations();
			information.addValue(PartsLinks::infoKey(),
								 PartsLinks::encode({{QStringLiteral("ACME"), QStringLiteral("TR-230"), 1}}));
			own->setElementInformations(information);
			QCOMPARE(DeviceNaming::baseName(own), QStringLiteral("-T"));
			DeviceNaming::nameNewElement(own);
			QCOMPARE(labelOf(own), QStringLiteral("-T1"));
				//A part without letter: the symbol's name
			Element *other = symbol(diagram, QStringLiteral("simple"), QStringLiteral("-Q7"));
			QVERIFY(other);
			information = other->elementInformations();
			information.addValue(PartsLinks::infoKey(),
								 PartsLinks::encode({{QStringLiteral("ACME"), QStringLiteral("NO-LETTER"), 1}}));
			other->setElementInformations(information);
			QCOMPARE(DeviceNaming::baseName(other), QStringLiteral("-Q"));
		}

		void placedDevicesTakeTheFreeNumbers()
		{
			QETProject project;
			Diagram *diagram = project.addNewDiagram();
			Element *first = symbol(diagram, QStringLiteral("simple"));
			QVERIFY(first);
			setLabel(first, QStringLiteral("-K1"));
			Element *third = symbol(diagram, QStringLiteral("master"));
			QVERIFY(third);
			setLabel(third, QStringLiteral("-K3"));

			Element *placed = symbol(diagram, QStringLiteral("simple"));
			QVERIFY(placed);
			placed->setPrefix(QStringLiteral("K"));
			DeviceNaming::nameNewElement(placed);
			QCOMPARE(labelOf(placed), QStringLiteral("-K2"));
			QVERIFY(!placed->elementInformations()
					.contains(QETInformation::ELMT_FORMULA));

				//Several at once: one number each
			Element *a = symbol(diagram, QStringLiteral("simple"));
			Element *b = symbol(diagram, QStringLiteral("master"));
			QVERIFY(a && b);
			a->setPrefix(QStringLiteral("K"));
			b->setPrefix(QStringLiteral("K"));
			DeviceNaming::nameNewElements({a, b});
			QCOMPARE(labelOf(a), QStringLiteral("-K4"));
			QCOMPARE(labelOf(b), QStringLiteral("-K5"));
		}

		void aContactIsNotNamed()
		{
			QETProject project;
			Diagram *diagram = project.addNewDiagram();
			Element *contact = symbol(diagram, QStringLiteral("slave"));
			QVERIFY(contact);
			contact->setPrefix(QStringLiteral("K"));
			DeviceNaming::nameNewElement(contact);
			QVERIFY(labelOf(contact).isEmpty());
		}
};

QET_APP_TEST_MAIN(tst_devicenaming)
#include "tst_devicenaming.moc"
