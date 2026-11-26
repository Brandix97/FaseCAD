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
#include "elementtextpattern.h"
#include "factory/elementfactory.h"
#include "partsdb/partfielddialog.h"
#include "partsdb/partsdatabase.h"
#include "partsdb/partslinks.h"
#include "partsdb/parttexts.h"
#include "qetgraphicsitem/dynamicelementtextitem.h"
#include "qetgraphicsitem/element.h"
#include "qetinformation.h"
#include "qetproject.h"
#include "ui/dynamicelementtextmodel.h"
#include "ui/elementinfopartwidget.h"
#include "ui/elementinfowidget.h"

#include <QBuffer>
#include <QComboBox>
#include <QImage>
#include <QDomDocument>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QUndoCommand>

#include <memory>

/**
	The texts of a symbol which show data of one of its parts, and the
	auxiliary blocks (Brandon, 2026-11-16):

	 - "Part data" is a source of text offered only for an element made of
	   parts; the search window lists every piece of data of every part;
	 - the text keeps the value it read: QElectroTech reads it as a user
	   text, FaseCAD reads it again from the parts database;
	 - only the first auxiliary block is offered, the others where an
	   element has something in them.
*/
class tst_parttexts : public QObject
{
	Q_OBJECT

	int m_made = 0;
	QTemporaryDir m_dir;

	Element *place(Diagram *diagram)
	{
		const QString name = QStringLiteral("device%1").arg(++m_made);
		const QString xml = QStringLiteral(
				"<definition type=\"element\" link_type=\"simple\" "
				"width=\"20\" height=\"40\" hotspot_x=\"10\" hotspot_y=\"20\">"
				"<uuid uuid=\"%1\"/>"
				"<names><name lang=\"en\">Test</name></names>"
				"<description><rect x=\"-5\" y=\"-10\" width=\"10\" height=\"20\" "
				"style=\"line-style:normal;line-weight:normal;filling:none;color:black\"/>"
				"<terminal x=\"0\" y=\"-20\" orientation=\"n\" uuid=\"%2\"/>"
				"</description></definition>")
				.arg(QUuid::createUuid().toString(), QUuid::createUuid().toString());
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
		element->setPos(100 + 60 * m_made, 200);
		return element;
	}

	static void madeOf(Element *element, const QString &manufacturer, const QString &order)
	{
		PartsLinks::Link link;
		link.manufacturer = manufacturer;
		link.order_number = order;
		DiagramContext information = element->elementInformations();
		information.addValue(PartsLinks::infoKey(), PartsLinks::encode({link}));
		element->setElementInformations(information);
	}

	static DynamicElementTextItem *textOn(Element *element)
	{
		auto *text = new DynamicElementTextItem(element);
		element->addDynamicTextItem(text);
		return text;
	}

	static Part relay()
	{
		Part part;
		part.manufacturer = QStringLiteral("Acme");
		part.order_number = QStringLiteral("K-24");
		part.description = QStringLiteral("Relay");
		part.voltage = QStringLiteral("24 V");
		part.width = 45;
		part.technical_data << qMakePair(QStringLiteral("Coil voltage"), QStringLiteral("24 V DC"));
		return part;
	}

	QString databasePath() const {return m_dir.filePath(QStringLiteral("parts.sqlite"));}

		/// The value cell of the row of @p type of @p text
	static QStandardItem *rowOf(DynamicElementTextModel &model, DynamicElementTextItem *text, int type)
	{
		QStandardItem *item = model.itemFromIndex(model.indexFromText(text));
		for (int r = 0 ; item && r < item->rowCount() ; ++r) {
			QStandardItem *value = item->child(r, 1);
			if (value && value->data(Qt::UserRole+1).toInt() == type) {
				return value;
			}
		}
		return nullptr;
	}

private slots:
	void initTestCase()
	{
		QETProject::setBackupEnabled(false);
		QVERIFY(m_dir.isValid());
		PartsDatabase database;
		QString error;
		QVERIFY2(database.open(databasePath(), &error), qPrintable(error));
		QVERIFY2(database.addPart(relay(), &error) > 0, qPrintable(error));
		PartsDatabase::setConfiguredPath(databasePath());
		PartsDatabase::setEnabled(true);
	}

	void aReferenceNamesThePartAndTheData()
	{
		const QString reference = PartTexts::reference(QStringLiteral("Acme"), QStringLiteral("K-24"),
													   QStringLiteral("voltage"));
		QString manufacturer, order, id;
		QVERIFY(PartTexts::decode(reference, &manufacturer, &order, &id));
		QCOMPARE(manufacturer, QStringLiteral("Acme"));
		QCOMPARE(order, QStringLiteral("K-24"));
		QCOMPARE(id, QStringLiteral("voltage"));
		QCOMPARE(PartTexts::fieldName(QStringLiteral("voltage")), QStringLiteral("Rated voltage"));
		QCOMPARE(PartTexts::fieldName(QStringLiteral("data:Coil voltage")), QStringLiteral("Coil voltage"));
		QCOMPARE(PartTexts::describe(reference), QStringLiteral("Rated voltage, K-24"));
		QVERIFY(!PartTexts::decode(QStringLiteral("nothing"), nullptr, nullptr, nullptr));
	}

	void thePartGivesTheDataItHas()
	{
		const QList<PartTexts::Field> fields = PartTexts::fieldsOf(relay());
		QStringList ids;
		for (const PartTexts::Field &field : fields) {
			ids << field.id;
			QVERIFY(!field.value.isEmpty());
			QCOMPARE(field.order_number, QStringLiteral("K-24"));
		}
		QVERIFY(ids.contains(QStringLiteral("description")));
		QVERIFY(ids.contains(QStringLiteral("voltage")));
		QVERIFY(ids.contains(QStringLiteral("data:Coil voltage")));
		QVERIFY(!ids.contains(QStringLiteral("current")));
		QCOMPARE(PartTexts::valueOf(relay(), QStringLiteral("width")), QStringLiteral("45"));
		QCOMPARE(PartTexts::valueOf(relay(), QStringLiteral("data:Coil voltage")), QStringLiteral("24 V DC"));
	}

	void aTextKeepsItsPartData()
	{
		QETProject project;
		Diagram *folio = project.addNewDiagram();
		Element *element = place(folio);
		QVERIFY(element);
		madeOf(element, QStringLiteral("Acme"), QStringLiteral("K-24"));
		DynamicElementTextItem *text = textOn(element);
		const QString reference = PartTexts::reference(QStringLiteral("Acme"), QStringLiteral("K-24"),
													   QStringLiteral("voltage"));
		text->setPartField(reference);
		text->setPartValue(QStringLiteral("24 V"));
		text->setTextFrom(DynamicElementTextItem::PartInfo);
		QCOMPARE(text->toPlainText(), QStringLiteral("24 V"));

			//QElectroTech reads a user text showing the value
		QDomDocument document;
		const QDomElement xml = text->toXml(document);
		QCOMPARE(xml.attribute(QStringLiteral("text_from")), QStringLiteral("UserText"));
		QCOMPARE(xml.firstChildElement(QStringLiteral("text")).text(), QStringLiteral("24 V"));
		QCOMPARE(xml.firstChildElement(QStringLiteral("part_info")).attribute(QStringLiteral("reference")),
				 reference);

		DynamicElementTextItem *read = textOn(element);
		read->fromXml(xml);
		QCOMPARE(read->textFrom(), DynamicElementTextItem::PartInfo);
		QCOMPARE(read->partField(), reference);
		QCOMPARE(read->partValue(), QStringLiteral("24 V"));
		QCOMPARE(read->toPlainText(), QStringLiteral("24 V"));

			//A text of another source saves no part
		DynamicElementTextItem *plain = textOn(element);
		plain->setText(QStringLiteral("hello"));
		QVERIFY(plain->toXml(document).firstChildElement(QStringLiteral("part_info")).isNull());
	}

	void theDatabaseRefreshesTheText()
	{
		QETProject project;
		Diagram *folio = project.addNewDiagram();
		Element *element = place(folio);
		QVERIFY(element);
		madeOf(element, QStringLiteral("Acme"), QStringLiteral("K-24"));
		DynamicElementTextItem *text = textOn(element);
		text->setPartField(PartTexts::reference(QStringLiteral("Acme"), QStringLiteral("K-24"),
												QStringLiteral("data:Coil voltage")));
		text->setPartValue(QStringLiteral("12 V DC"));
		text->setTextFrom(DynamicElementTextItem::PartInfo);

		DynamicElementTextItem *gone = textOn(element);
		gone->setPartField(PartTexts::reference(QStringLiteral("Acme"), QStringLiteral("NONE"),
												QStringLiteral("voltage")));
		gone->setPartValue(QStringLiteral("230 V"));
		gone->setTextFrom(DynamicElementTextItem::PartInfo);

		QCOMPARE(PartTexts::refresh({element}), 1);
		QCOMPARE(text->toPlainText(), QStringLiteral("24 V DC"));
			//A part not in the database: what it showed stays
		QCOMPARE(gone->toPlainText(), QStringLiteral("230 V"));
		QCOMPARE(PartTexts::refresh({element}), 0);

		PartsDatabase database;
		QVERIFY(database.open(databasePath()));
		const QList<PartTexts::Field> fields = PartTexts::fieldsOf(element, database);
		QVERIFY(fields.size() >= 4);
	}

	void aPartImageIsShownAsItIs()
	{
			//A photo of the relay, 40 x 20, in the parts database
		auto png = [](int width, int height) {
			QImage image(width, height, QImage::Format_ARGB32);
			image.fill(Qt::red);
			QByteArray bytes;
			QBuffer buffer(&bytes);
			buffer.open(QIODevice::WriteOnly);
			image.save(&buffer, "PNG");
			return bytes;
		};
		qint64 file = 0;
		{
			PartsDatabase database;
			QVERIFY(database.open(databasePath()));
			const qint64 part = database.findPart(QStringLiteral("Acme"), QStringLiteral("K-24"));
			QVERIFY(part > 0);
			file = database.addFile(part, PartFileKind::Image, QStringLiteral("photo.png"), png(40, 20));
			QVERIFY(file > 0);
		}

		QETProject project;
		Diagram *folio = project.addNewDiagram();
		Element *element = place(folio);
		QVERIFY(element);
		madeOf(element, QStringLiteral("Acme"), QStringLiteral("K-24"));

			//Offered with the data of the part
		PartsDatabase database;
		QVERIFY(database.open(databasePath()));
		const QString id = PartTexts::imageId(QStringLiteral("photo.png"));
		bool offered = false;
		for (const PartTexts::Field &field : PartTexts::fieldsOf(element, database))
			offered = offered || field.id == id;
		QVERIFY(offered);
		const QByteArray image = PartTexts::imageOf(database, QStringLiteral("Acme"),
													QStringLiteral("K-24"), id);
		QVERIFY(!image.isEmpty());
		QCOMPARE(QImage::fromData(image).size(), QSize(40, 20));
		QCOMPARE(PartTexts::describe(PartTexts::reference(QStringLiteral("Acme"), QStringLiteral("K-24"), id)),
				 QStringLiteral("Image: photo.png, K-24"));

			//20 wide when its width is "auto", as high as the image is
		DynamicElementTextItem *text = textOn(element);
		text->setPartField(PartTexts::reference(QStringLiteral("Acme"), QStringLiteral("K-24"), id));
		text->setPartImage(image);
		text->setTextFrom(DynamicElementTextItem::PartInfo);
		QVERIFY(text->showsImage());
		QCOMPARE(text->imageSize(), QSizeF(20, 10));
		QCOMPARE(text->boundingRect().size(), QSizeF(20, 10));
		text->setTextWidth(30);
		QCOMPARE(text->imageSize(), QSizeF(30, 15));
			//A height too: the smaller of the two, never stretched
		text->setTextHeight(5);
		QCOMPARE(text->imageSize(), QSizeF(10, 5));
		text->setTextHeight(-1);

			//Kept in the project, QElectroTech reading an empty user text
		QDomDocument document;
		const QDomElement xml = text->toXml(document);
		QVERIFY(!xml.firstChildElement(QStringLiteral("part_info"))
				.firstChildElement(QStringLiteral("image")).isNull());
		QVERIFY(xml.firstChildElement(QStringLiteral("text")).text().isEmpty());
		DynamicElementTextItem *read = textOn(element);
		read->fromXml(xml);
		QVERIFY(read->showsImage());
		QCOMPARE(read->partImage(), image);

			//Another photo in the database: read again
		QVERIFY(database.removeFile(file));
		const qint64 part = database.findPart(QStringLiteral("Acme"), QStringLiteral("K-24"));
		QVERIFY(database.addFile(part, PartFileKind::Image, QStringLiteral("photo.png"), png(80, 20)) > 0);
		QCOMPARE(PartTexts::refresh({element}), 2);
		QCOMPARE(text->imageSize(), QSizeF(30, 7.5));
	}

	void anImageChosenInTheTextsTabIsShown()
	{
			//As the search window gives it back to the row of the tab
		QETProject project;
		Diagram *folio = project.addNewDiagram();
		Element *element = place(folio);
		QVERIFY(element);
		madeOf(element, QStringLiteral("Acme"), QStringLiteral("K-24"));
		DynamicElementTextItem *text = textOn(element);
		DynamicElementTextModel model(element);
		QStandardItem *source = rowOf(model, text, DynamicElementTextModel::textFrom);
		QStandardItem *part = rowOf(model, text, DynamicElementTextModel::partText);
		QVERIFY(source && part);
		source->setData(QStringLiteral("Part data"), Qt::DisplayRole);

		QImage picture(40, 20, QImage::Format_ARGB32);
		picture.fill(Qt::blue);
		QByteArray png;
		QBuffer buffer(&png);
		buffer.open(QIODevice::WriteOnly);
		picture.save(&buffer, "PNG");
		const QString reference = PartTexts::reference(QStringLiteral("Acme"), QStringLiteral("K-24"),
													   PartTexts::imageId(QStringLiteral("photo.png")));
		QWidget editor;
		editor.setObjectName(QStringLiteral("part_data_dialog"));
		editor.setProperty("ok", true);
		editor.setProperty("reference", reference);
		editor.setProperty("value", QString());
		editor.setProperty("image", png);
		DynamicTextItemDelegate delegate;
		delegate.setModelData(&editor, &model, part->index());

		std::unique_ptr<QUndoCommand> undo(model.undoForEditedText(text));
		undo->redo();
		QCOMPARE(text->partField(), reference);
		QCOMPARE(text->partImage(), png);
		QVERIFY(text->showsImage());
			//20 wide, as high as the image is
		QCOMPARE(text->textWidth(), 20.0);
		QCOMPARE(text->imageSize(), QSizeF(20, 10));
		undo->undo();
		QVERIFY(!text->showsImage());
	}

	void copyFormatBringsThePartData()
	{
			//Copy format (Brandon, 2026-11-20): -K1 shows the photo and the
			//description of its relay. The same relay gets them, another
			//part its own, an element of no part nothing
		auto png = [](const QColor &color) {
			QImage image(40, 20, QImage::Format_ARGB32);
			image.fill(color);
			QByteArray bytes;
			QBuffer buffer(&bytes);
			buffer.open(QIODevice::WriteOnly);
			image.save(&buffer, "PNG");
			return bytes;
		};
		const QString acme = QStringLiteral("Acme");
		{
			PartsDatabase database;
			QVERIFY(database.open(databasePath()));
			QString error;
			for (const QString &order : {QStringLiteral("FP-1"), QStringLiteral("FP-2")}) {
				Part part;
				part.manufacturer = acme;
				part.order_number = order;
				part.description = order == QLatin1String("FP-1") ? QStringLiteral("One")
																  : QStringLiteral("Two");
				const qint64 id = database.addPart(part, &error);
				QVERIFY2(id > 0, qPrintable(error));
				QVERIFY(database.addFile(id, PartFileKind::Image,
										 order == QLatin1String("FP-1") ? QStringLiteral("a.png")
																		: QStringLiteral("b.png"),
										 png(order == QLatin1String("FP-1") ? Qt::red : Qt::green)) > 0);
			}
		}
		PartsDatabase database;
		QVERIFY(database.open(databasePath()));
		const QString a_png = PartTexts::imageId(QStringLiteral("a.png"));
		const QByteArray photo = PartTexts::imageOf(database, acme, QStringLiteral("FP-1"), a_png);
		QVERIFY(!photo.isEmpty());

		QETProject project;
		Diagram *folio = project.addNewDiagram();
		Element *k1 = place(folio);
		Element *same = place(folio);
		Element *other = place(folio);
		Element *bare = place(folio);
		QVERIFY(k1 && same && other && bare);
		madeOf(k1, acme, QStringLiteral("FP-1"));
		madeOf(same, acme, QStringLiteral("FP-1"));
		madeOf(other, acme, QStringLiteral("FP-2"));

		DynamicElementTextItem *image = textOn(k1);
		image->setPartField(PartTexts::reference(acme, QStringLiteral("FP-1"), a_png));
		image->setPartImage(photo);
		image->setTextFrom(DynamicElementTextItem::PartInfo);
		image->setTextWidth(20);
		image->setTextHeight(8);
		image->setPos(30, 5);
		DynamicElementTextItem *description = textOn(k1);
		description->setPartField(PartTexts::reference(acme, QStringLiteral("FP-1"),
													   QStringLiteral("description")));
		description->setPartValue(QStringLiteral("One"));
		description->setTextFrom(DynamicElementTextItem::PartInfo);
		description->setPos(30, -20);

		auto partTexts = [](Element *element) {
			QList<DynamicElementTextItem *> texts;
			for (DynamicElementTextItem *text : element->dynamicTextItems())
				if (text->textFrom() == DynamicElementTextItem::PartInfo)
					texts << text;
			return texts;
		};
		auto imageOn = [&partTexts](Element *element) -> DynamicElementTextItem * {
			for (DynamicElementTextItem *text : partTexts(element))
				if (text->showsImage())
					return text;
			return nullptr;
		};

		ApplyElementTextFormatCommand command({same, other, bare},
											  ElementTextFormatPattern::fromElement(k1));
		command.redo();
		QCOMPARE(partTexts(same).size(), 2);
		DynamicElementTextItem *copy = imageOn(same);
		QVERIFY(copy);
		QCOMPARE(copy->partField(), image->partField());
		QCOMPARE(copy->partImage(), photo);
		QCOMPARE(copy->pos(), QPointF(30, 5));
		QCOMPARE(copy->textHeight(), 8.0);
		QCOMPARE(copy->imageSize(), QSizeF(16, 8));

		QCOMPARE(partTexts(other).size(), 2);
		DynamicElementTextItem *own = imageOn(other);
		QVERIFY(own);
		QCOMPARE(own->partField(), PartTexts::reference(acme, QStringLiteral("FP-2"),
														PartTexts::imageId(QStringLiteral("b.png"))));
		QCOMPARE(QImage::fromData(own->partImage()).pixelColor(0, 0), QColor(Qt::green));
		bool two = false;
		for (DynamicElementTextItem *text : partTexts(other))
			two = two || text->toPlainText() == QLatin1String("Two");
		QVERIFY(two);

		QVERIFY(partTexts(bare).isEmpty());

			//Undone: gone; again: the same texts back, nothing doubled
		command.undo();
		QVERIFY(partTexts(same).isEmpty());
		command.redo();
		QCOMPARE(partTexts(same).size(), 2);
			//Applied twice: the texts there take the format, none added
		ApplyElementTextFormatCommand again({same}, ElementTextFormatPattern::fromElement(k1));
		again.redo();
		QCOMPARE(partTexts(same).size(), 2);
	}

	void copyFormatKeepsWhereATextIsAligned()
	{
			//Copy format of a terminal (Brandon, 2026-11-20): its label,
			//aligned right, ends beside the wire. A longer label of the
			//other ends there too, its "Position X/Y" the same numbers
		QETProject project;
		Diagram *folio = project.addNewDiagram();
		Element *source = place(folio);
		Element *target = place(folio);
		QVERIFY(source && target);
		DynamicElementTextItem *label = textOn(source);
		label->setText(QStringLiteral("-XIN:L1"));
		label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		label->setAnchorPos(QPointF(2, -7));
		DynamicElementTextItem *longer = textOn(target);
		longer->setText(QStringLiteral("L233333 much longer"));
		longer->setPos(-40, 10);
		QVERIFY(longer->boundingRect().width() > label->boundingRect().width());

		ApplyElementTextFormatCommand command({target}, ElementTextFormatPattern::fromElement(source));
		command.redo();
		QCOMPARE(longer->alignment(), label->alignment());
		QCOMPARE(longer->anchorPos(), QPointF(2, -7));
		QCOMPARE(longer->toPlainText(), QStringLiteral("L233333 much longer"));
		command.undo();
		QCOMPARE(longer->pos(), QPointF(-40, 10));
	}

	void theSourceIsOfferedForAnElementWithParts()
	{
		QETProject project;
		Diagram *folio = project.addNewDiagram();
		Element *bare = place(folio);
		Element *made = place(folio);
		QVERIFY(bare && made);
		madeOf(made, QStringLiteral("Acme"), QStringLiteral("K-24"));
		QVERIFY(!PartTexts::hasParts(bare));
		QVERIFY(PartTexts::hasParts(made));

		for (Element *element : {bare, made}) {
			DynamicElementTextItem *text = textOn(element);
			DynamicElementTextModel model(element);
			DynamicTextItemDelegate delegate;
			QStandardItem *source = rowOf(model, text, DynamicElementTextModel::textFrom);
			QVERIFY(source);
			QWidget parent;
			std::unique_ptr<QWidget> editor(delegate.createEditor(&parent, QStyleOptionViewItem(),
																  source->index()));
			auto *combo = qobject_cast<QComboBox *>(editor.get());
			QVERIFY(combo);
			QCOMPARE(combo->count(), 4);
			QCOMPARE(combo->itemText(3), QStringLiteral("Part data"));
			auto *items = qobject_cast<QStandardItemModel *>(combo->model());
			QVERIFY(items);
			QCOMPARE(items->item(3)->isEnabled(), element == made);
		}
	}

	void theTextPropertiesApplyThePartData()
	{
		QETProject project;
		Diagram *folio = project.addNewDiagram();
		Element *element = place(folio);
		QVERIFY(element);
		madeOf(element, QStringLiteral("Acme"), QStringLiteral("K-24"));
		DynamicElementTextItem *text = textOn(element);
		DynamicElementTextModel model(element);
		QStandardItem *source = rowOf(model, text, DynamicElementTextModel::textFrom);
		QStandardItem *part = rowOf(model, text, DynamicElementTextModel::partText);
		QVERIFY(source && part);
		QCOMPARE(part->text(), QStringLiteral("Choose..."));

		const QString reference = PartTexts::reference(QStringLiteral("Acme"), QStringLiteral("K-24"),
													   QStringLiteral("description"));
		part->setData(QStringLiteral("Relay"), Qt::UserRole+3);
		part->setData(reference, Qt::UserRole+2);
		source->setData(QStringLiteral("Part data"), Qt::DisplayRole);

		std::unique_ptr<QUndoCommand> undo(model.undoForEditedText(text));
		QVERIFY(undo->childCount() >= 2);
		undo->redo();
		QCOMPARE(text->textFrom(), DynamicElementTextItem::PartInfo);
		QCOMPARE(text->partField(), reference);
		QCOMPARE(text->toPlainText(), QStringLiteral("Relay"));
		QCOMPARE(part->text(), QStringLiteral("Textual description, K-24"));
		undo->undo();
		QCOMPARE(text->textFrom(), DynamicElementTextItem::UserText);
		QVERIFY(text->partField().isEmpty());
	}

	void theSearchWindowFindsEveryWord()
	{
		PartFieldDialog dialog(PartTexts::fieldsOf(relay()), QString());
		auto *search = dialog.findChild<QLineEdit *>(QStringLiteral("m_search"));
		auto *table = dialog.findChild<QTableWidget *>(QStringLiteral("m_table"));
		QVERIFY(search && table);
		QVERIFY(table->rowCount() >= 4);
		QVERIFY(!dialog.hasSelection());

		search->setText(QStringLiteral("coil 24"));
		int shown = 0, row_shown = -1;
		for (int row = 0 ; row < table->rowCount() ; ++row) {
			if (!table->isRowHidden(row)) {
				++shown;
				row_shown = row;
			}
		}
		QCOMPARE(shown, 1);
		table->selectRow(row_shown);
		QCOMPARE(dialog.selectedField().id, QStringLiteral("data:Coil voltage"));

			//The data the text shows is chosen when the window opens
		const QString current = PartTexts::reference(QStringLiteral("Acme"), QStringLiteral("K-24"),
													 QStringLiteral("voltage"));
		PartFieldDialog again(PartTexts::fieldsOf(relay()), current);
		QVERIFY(again.hasSelection());
		QCOMPARE(again.selectedField().id, QStringLiteral("voltage"));
	}

	void onlyTheFirstAuxiliaryBlockIsOffered()
	{
		QVERIFY(!QETInformation::isExtraAuxiliaryInfo(QStringLiteral("auxiliary1")));
		QVERIFY(!QETInformation::isExtraAuxiliaryInfo(QStringLiteral("description_auxiliary1")));
		QVERIFY(QETInformation::isExtraAuxiliaryInfo(QStringLiteral("auxiliary2")));
		QVERIFY(QETInformation::isExtraAuxiliaryInfo(QStringLiteral("manufacturer_reference_auxiliary3")));
		QVERIFY(QETInformation::isExtraAuxiliaryInfo(QStringLiteral("unity_auxiliary4")));
		QVERIFY(!QETInformation::isExtraAuxiliaryInfo(QStringLiteral("description")));

		QETProject project;
		Diagram *folio = project.addNewDiagram();
		Element *element = place(folio);
		QVERIFY(element);
		auto rowFor = [](ElementInfoWidget &widget, const QString &key) -> ElementInfoPartWidget * {
			for (ElementInfoPartWidget *row : widget.findChildren<ElementInfoPartWidget *>()) {
				if (row->key() == key) {
					return row;
				}
			}
			return nullptr;
		};
			//No field from the article number on (Brandon, 2026-11-21):
			//the parts of the element say it. The description stays
		QVERIFY(QETInformation::isEditedInInformationTab(QETInformation::ELMT_DESCRIPTION));
		QVERIFY(!QETInformation::isEditedInInformationTab(QETInformation::ELMT_DESIGNATION));
		QVERIFY(!QETInformation::isEditedInInformationTab(QETInformation::ELMT_MANUFACTURER));
		QVERIFY(!QETInformation::isEditedInInformationTab(QETInformation::ELMT_DESCRIPTION_AUX1));
		{
			ElementInfoWidget widget(element);
			ElementInfoPartWidget *first = rowFor(widget, QETInformation::ELMT_DESCRIPTION_AUX1);
			ElementInfoPartWidget *second = rowFor(widget, QETInformation::ELMT_DESCRIPTION_AUX2);
			QVERIFY(first && second);
			QVERIFY(first->isHidden());
			QVERIFY(second->isHidden());
			QVERIFY(!rowFor(widget, QETInformation::ELMT_DESCRIPTION)->isHidden());
			QVERIFY(rowFor(widget, QETInformation::ELMT_DESIGNATION)->isHidden());
		}

			//An old project with something there: not shown, and kept
			//when the tab writes the element back
		DiagramContext information = element->elementInformations();
		information.addValue(QETInformation::ELMT_DESCRIPTION_AUX2, QStringLiteral("Old block"));
		information.addValue(QETInformation::ELMT_MANUFACTURER, QStringLiteral("Acme"));
		element->setElementInformations(information);
		ElementInfoWidget widget(element);
		ElementInfoPartWidget *second = rowFor(widget, QETInformation::ELMT_DESCRIPTION_AUX2);
		QVERIFY(second && second->isHidden());
		QCOMPARE(second->text(), QStringLiteral("Old block"));
		QVERIFY(rowFor(widget, QETInformation::ELMT_MANUFACTURER)->isHidden());
		const DiagramContext written = widget.currentInfo();
		QCOMPARE(written.value(QETInformation::ELMT_MANUFACTURER).toString(), QStringLiteral("Acme"));
		QCOMPARE(written.value(QETInformation::ELMT_DESCRIPTION_AUX2).toString(), QStringLiteral("Old block"));
	}
};

QET_APP_TEST_MAIN(tst_parttexts)
#include "tst_parttexts.moc"
