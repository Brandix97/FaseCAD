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
#include "diagram.h"
#include "qetproject.h"
#include "qetresult.h"
#include "titleblock/templatescollection.h"
#include "titleblock/titleblockdesign.h"
#include "titleblock/titleblockdesigner.h"
#include "titleblockcell.h"
#include "titleblocktemplate.h"
#include "ui/titleblockdatadialog.h"

#include <QBuffer>
#include <QImage>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QTemporaryDir>
#include <QFile>
#include <QUndoCommand>
#include <QUndoStack>

#include <memory>

using namespace TitleBlockDesign;

/**
	The title block made of blocks (Brandon, 2026-11-19: "the original one
	I do not like at all, be creative")
*/
class tst_titleblockdesign : public QObject
{
	Q_OBJECT

	static int darkPixels(const QImage &image)
	{
		int dark = 0;
		for (int y = 0 ; y < image.height() ; ++y)
		{
			for (int x = 0 ; x < image.width() ; ++x)
			{
				if (qGray(image.pixel(x, y)) < 128) ++dark;
			}
		}
		return dark;
	}

	static QImage rendered(const TitleBlockTemplate &tbt, const DiagramContext &context, int width)
	{
		QImage image(width + 2, tbt.height() + 2, QImage::Format_RGB32);
		image.fill(Qt::white);
		QPainter painter(&image);
		tbt.render(painter, context, width);
		painter.end();
		return image;
	}

	static void send(QWidget *widget, QEvent::Type type, const QPointF &at, Qt::MouseButtons buttons)
	{
		QMouseEvent event(type, at, widget->mapToGlobal(at.toPoint()),
						  type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
						  buttons, Qt::NoModifier);
		QCoreApplication::sendEvent(widget, &event);
	}

private slots:
	void initTestCase()
	{
		QETProject::setBackupEnabled(false);
	}

	void aDesignBecomesATemplate()
	{
		const Design design = completeDesign();
		std::unique_ptr<TitleBlockTemplate> tbt(build(design));
		QVERIFY(tbt);
		QCOMPARE(tbt->rowsCount(), 12);
			//The bar, then logo, company, project, client, 4 of the
			//revisions, 3 of the signatures, format, 2 of the sheet
		QCOMPARE(tbt->columnsCount(), 15);
		QCOMPARE(tbt->height(), 72);
		const QStringList variables = tbt->listOfVariables();
		for (const char *name : {"projecttitle", "title", "folio", "company", "client",
								 "revision1-index", "revision3-author", "checked-by",
								 "approved-date", "paper", "scale", "standard"}) {
			QVERIFY2(variables.contains(QLatin1String(name)), name);
		}
		QVERIFY(!variables.contains(QStringLiteral("revision4-index")));
			//Five rows of revisions, the table fills the block
		Design more = design;
		more.revision_rows = 5;
		std::unique_ptr<TitleBlockTemplate> five(build(more));
		QVERIFY(five->listOfVariables().contains(QStringLiteral("revision5-description")));
	}

	void theCellsKeepTheirStyle()
	{
		std::unique_ptr<TitleBlockTemplate> tbt(build(completeDesign()));
		QVERIFY(tbt);
		QDomDocument document;
		QDomElement xml = document.createElement(QStringLiteral("root"));
		QVERIFY(tbt->saveToXmlElement(xml));
		TitleBlockTemplate read;
		QVERIFY(read.loadFromXmlElement(xml));

		const TitleBlockCell *bar = read.cell(0, 0);
		QVERIFY(bar);
		QCOMPARE(bar->fill, QColor(0xFA, 0x13, 0x0C));
		QCOMPARE(bar->borders, Qt::Edges());
		const TitleBlockCell *company = read.cell(0, 2);
		QCOMPARE(company->borders, Qt::LeftEdge | Qt::TopEdge | Qt::RightEdge);
		const TitleBlockCell *project = read.cell(0, 3);
		QVERIFY(project->stacked);
		QCOMPARE(project->label.name(), QStringLiteral("Project"));
		QCOMPARE(project->value.name(), QStringLiteral("%{projecttitle}"));
		const TitleBlockCell *header = read.cell(0, 5);
		QVERIFY(header->fill.isValid());
		QVERIFY(header->fill.alpha() < 255);
		QCOMPARE(header->value.name(), QStringLiteral("Rev"));

			//The classic style: one line, no fill, no bar
		Design classic = completeDesign();
		classic.style = Style::Classic;
		std::unique_ptr<TitleBlockTemplate> plain(build(classic));
		QCOMPARE(plain->columnsCount(), 14);
		QVERIFY(!plain->cell(0, 2)->stacked);
		QVERIFY(!plain->cell(0, 4)->fill.isValid());
		QCOMPARE(TitleBlockCell::bordersFromString(QString()), TitleBlockCell::allBorders());
		QCOMPARE(TitleBlockCell::bordersToString(Qt::LeftEdge | Qt::BottomEdge), QStringLiteral("lb"));
	}

	void itIsDrawnWithTheData()
	{
		Design design;
		design.accent_bar = false;
		Block project;
		project.kind = Kind::Project;
		project.width = 100;
		design.blocks << project;
		std::unique_ptr<TitleBlockTemplate> tbt(build(design));
		QVERIFY(tbt);
		DiagramContext empty;
		DiagramContext data;
		data.addValue(QStringLiteral("projecttitle"), QStringLiteral("Bombeo de aguas residuales"));
		data.addValue(QStringLiteral("title"), QStringLiteral("Alimentación"));
		const int without = darkPixels(rendered(*tbt, empty, 600));
		const int with = darkPixels(rendered(*tbt, data, 600));
			//The frame and the labels, then the values too
		QVERIFY(without > 0);
		QVERIFY2(with > without + 200, qPrintable(QStringLiteral("%1 %2").arg(without).arg(with)));
	}

	void theDesignIsReadBack()
	{
		Design design = completeDesign();
		design.name = QStringLiteral("mine");
		design.style = Style::Classic;
		design.height = 90;
		design.accent = QColor(0x12, 0x34, 0x56);
		design.revision_rows = 5;
		design.blocks[1].texts = {QStringLiteral("Cuadros Brandon"), QStringLiteral("Calle 1"),
								  QStringLiteral("600 000 000")};
		QImage logo(8, 8, QImage::Format_ARGB32);
		logo.fill(Qt::red);
		QBuffer buffer(&design.logo);
		buffer.open(QIODevice::WriteOnly);
		logo.save(&buffer, "PNG");
		design.logo_type = QStringLiteral("png");

		QDomDocument document;
		const QDomElement xml = templateXml(design, document);
		Design read;
		QVERIFY(fromTemplateXml(xml, read));
		QCOMPARE(read.name, QStringLiteral("mine"));
		QCOMPARE(read.style, Style::Classic);
		QCOMPARE(read.height, 90);
		QCOMPARE(read.accent, QColor(0x12, 0x34, 0x56));
		QCOMPARE(read.revision_rows, 5);
		QCOMPARE(read.blocks.size(), design.blocks.size());
		for (int i = 0 ; i < read.blocks.size() ; ++i)
		{
			QCOMPARE(read.blocks.at(i).kind, design.blocks.at(i).kind);
			QCOMPARE(read.blocks.at(i).width, design.blocks.at(i).width);
		}
		QCOMPARE(text(read.blocks.at(1), 0), QStringLiteral("Cuadros Brandon"));
		QCOMPARE(text(read.blocks.at(2), 0), QStringLiteral("%{projecttitle}"));
		QCOMPARE(read.logo, design.logo);
			//A template made otherwise carries no design
		QDomElement other = document.createElement(QStringLiteral("titleblocktemplate"));
		QVERIFY(!fromTemplateXml(other, read));
	}

	void widthsAddUpToTheWhole()
	{
		Design design;
		for (int width : {10, 10, 10, 1})
		{
			Block block;
			block.width = width;
			design.blocks << block;
		}
		normalizeWidths(design);
		int total = 0;
		for (const Block &block : design.blocks)
		{
			QVERIFY(block.width >= 3);
			total += block.width;
		}
		QCOMPARE(total, 100);

		const Design complete = completeDesign();
		const QList<QPair<int, int>> spans = blockSpans(complete, 1500);
		QCOMPARE(spans.size(), complete.blocks.size());
		QCOMPARE(spans.first().first, 0);
		QCOMPARE(spans.last().second, 1500);
		for (int i = 1 ; i < spans.size() ; ++i) QCOMPARE(spans.at(i).first, spans.at(i - 1).second);
	}

	void revisionsLatestFirst()
	{
		DiagramContext properties;
		properties.addValue(QStringLiteral("client"), QStringLiteral("Aguas"));
		Revision b {QStringLiteral("B"), QStringLiteral("19/11/26"), QStringLiteral("As built"), QStringLiteral("BR")};
		Revision a {QStringLiteral("A"), QStringLiteral("01/10/26"), QStringLiteral("First issue"), QStringLiteral("BR")};
		setRevisions(properties, {b, a});
		QCOMPARE(properties.value(QStringLiteral("revision1-index")).toString(), QStringLiteral("B"));
		QCOMPARE(properties.value(QStringLiteral("revision2-description")).toString(), QStringLiteral("First issue"));
		QCOMPARE(revisions(properties), (QList<Revision>{b, a}));
		setRevisions(properties, {a});
		QVERIFY(!properties.contains(QStringLiteral("revision2-index")));
		QCOMPARE(properties.value(QStringLiteral("client")).toString(), QStringLiteral("Aguas"));

		QCOMPARE(nextIndex(QString()), QStringLiteral("A"));
		QCOMPARE(nextIndex(QStringLiteral("A")), QStringLiteral("B"));
		QCOMPARE(nextIndex(QStringLiteral("Z")), QStringLiteral("AA"));
		QCOMPARE(nextIndex(QStringLiteral("3")), QStringLiteral("4"));
		QCOMPARE(nextIndex(QStringLiteral("09")), QStringLiteral("10"));
		QCOMPARE(nextIndex(QStringLiteral("R1")), QStringLiteral("R2"));
	}

	void theFolioGivesPaperAndScale()
	{
		QETProject project;
		Diagram *schematic = project.addNewDiagram();
		Diagram *cabinet = project.addNewCabinetDiagram();
		QVERIFY(schematic && cabinet);
		const DiagramContext context = schematic->border_and_titleblock.titleblockInformation();
		QVERIFY2(context.value(QStringLiteral("paper")).toString().startsWith(QLatin1Char('A')),
				 qPrintable(context.value(QStringLiteral("paper")).toString()));
		QCOMPARE(context.value(QStringLiteral("scale")).toString(), QStringLiteral("Not to scale"));
		cabinet->setCabinetScale(10);
		QCOMPARE(cabinet->border_and_titleblock.titleblockInformation()
				 .value(QStringLiteral("scale")).toString(), QStringLiteral("1:10"));
	}

	void theDesignerGivesItToEveryFolio()
	{
		QETProject project;
		Diagram *first = project.addNewDiagram();
		Diagram *second = project.addNewDiagram();
		const QString before = first->border_and_titleblock.titleBlockTemplateName();

		TitleBlockDesigner designer(&project, first);
		designer.setDesign(completeDesign());
		auto *name = designer.findChild<QLineEdit *>(QStringLiteral("m_name"));
		QVERIFY(name);
		name->setText(QStringLiteral("Mis cuadros"));
		designer.setForEveryFolio(true);
		QVERIFY(designer.apply());

		QVERIFY(project.embeddedTitleBlockTemplatesCollection()->templates()
				.contains(QStringLiteral("mis-cuadros")));
		QCOMPARE(first->border_and_titleblock.titleBlockTemplateName(), QStringLiteral("mis-cuadros"));
		QCOMPARE(second->border_and_titleblock.titleBlockTemplateName(), QStringLiteral("mis-cuadros"));
		QVERIFY(second->border_and_titleblock.titleBlockTemplate()->listOfVariables()
				.contains(QStringLiteral("revision1-index")));

			//Opened again, it is the same design
		TitleBlockDesigner again(&project, second);
		QCOMPARE(again.design().name, QStringLiteral("mis-cuadros"));
		QCOMPARE(again.design().blocks.size(), completeDesign().blocks.size());

		project.undoStack()->undo();
		QCOMPARE(first->border_and_titleblock.titleBlockTemplateName(), before);
		QCOMPARE(second->border_and_titleblock.titleBlockTemplateName(), before);
	}

	void theDesignIsSavedWithTheProject()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString path = dir.filePath(QStringLiteral("cajetin.qet"));
		{
			QETProject project;
			project.setTitle(QStringLiteral("Bombeo de aguas residuales"));
			Diagram *folio = project.addNewDiagram();
			project.addNewCabinetDiagram();
			DiagramContext properties = project.projectProperties();
			properties.addValue(QStringLiteral("company"), QStringLiteral("Cuadros Eléctricos del Sur"));
			properties.addValue(QStringLiteral("company-address"), QStringLiteral("Polígono Norte, nave 4"));
			properties.addValue(QStringLiteral("company-contact"), QStringLiteral("955 000 000 · cuadros@example.com"));
			properties.addValue(QStringLiteral("client"), QStringLiteral("Aguas del Sur"));
			properties.addValue(QStringLiteral("site"), QStringLiteral("EDAR Los Pinos"));
			properties.addValue(QStringLiteral("site-address"), QStringLiteral("Camino del Río s/n"));
			properties.addValue(QStringLiteral("checked-by"), QStringLiteral("J. Ruiz"));
			properties.addValue(QStringLiteral("standard"), QStringLiteral("IEC 81346"));
			setRevisions(properties, {
				{QStringLiteral("B"), QStringLiteral("19/11/26"), QStringLiteral("As built"), QStringLiteral("BR")},
				{QStringLiteral("A"), QStringLiteral("01/10/26"), QStringLiteral("First issue"), QStringLiteral("BR")}});
			project.setProjectProperties(properties);

			TitleBlockDesigner designer(&project, folio);
			designer.setDesign(completeDesign());
			designer.findChild<QLineEdit *>(QStringLiteral("m_name"))->setText(QStringLiteral("cuadros"));
			designer.setForEveryFolio(true);
			QVERIFY(designer.apply());
			project.setFilePath(path);
			QVERIFY(project.write().isOk());
				//QET_TITLEBLOCK_PROJECT=<file.qet> keeps a copy to look at
			const QString keep = qEnvironmentVariable("QET_TITLEBLOCK_PROJECT");
			if (!keep.isEmpty()) {
				QFile::remove(keep);
				QFile::copy(path, keep);
			}
		}
		QETProject project(path);
		QCOMPARE(project.diagrams().size(), 2);
		Diagram *folio = project.diagrams().first();
		folio->ensureLoaded();
		QCOMPARE(folio->border_and_titleblock.titleBlockTemplateName(), QStringLiteral("cuadros"));
		Design read;
		QVERIFY(fromTemplateXml(project.embeddedTitleBlockTemplatesCollection()
								->getTemplateXmlDescription(QStringLiteral("cuadros")), read));
		QCOMPARE(read.blocks.size(), completeDesign().blocks.size());
		const DiagramContext context = folio->border_and_titleblock.titleblockInformation();
		QCOMPARE(context.value(QStringLiteral("revision1-index")).toString(), QStringLiteral("B"));
		QCOMPARE(context.value(QStringLiteral("client")).toString(), QStringLiteral("Aguas del Sur"));
	}

	void blocksAreAddedResizedAndMoved()
	{
		QETProject project;
		Diagram *folio = project.addNewDiagram();
		TitleBlockDesigner designer(&project, folio);
		designer.setDesign(compactDesign());
		const int count = designer.design().blocks.size();
			//One block of each kind
		designer.addBlock(Kind::Project);
		QCOMPARE(designer.design().blocks.size(), count);
		designer.addBlock(Kind::Client);
		QCOMPARE(designer.design().blocks.size(), count + 1);
		QCOMPARE(designer.design().blocks.last().kind, Kind::Client);
		QCOMPARE(designer.selectedBlock(), count);
		designer.removeBlock(count);
		QCOMPARE(designer.design().blocks.size(), count);

		designer.resize(1100, 640);
		designer.show();
		QVERIFY(QTest::qWaitForWindowExposed(&designer));
		auto *preview = designer.findChild<TitleBlockDesignPreview *>(QStringLiteral("m_preview"));
		QVERIFY(preview);
		const int widths = designer.design().blocks.at(0).width + designer.design().blocks.at(1).width;
		const int was = designer.design().blocks.at(0).width;

			//The line between the first two blocks, dragged to the right
		const QRectF logo = preview->blockRect(0);
		const QPointF at(logo.right(), logo.center().y());
		send(preview, QEvent::MouseButtonPress, at, Qt::LeftButton);
		send(preview, QEvent::MouseMove, at + QPointF(60, 0), Qt::LeftButton);
		send(preview, QEvent::MouseButtonRelease, at + QPointF(60, 0), Qt::NoButton);
		QVERIFY(designer.design().blocks.at(0).width > was);
		QCOMPARE(designer.design().blocks.at(0).width + designer.design().blocks.at(1).width, widths);

			//The first block dragged past the last one
		const Kind moved = designer.design().blocks.first().kind;
		const QPointF from = preview->blockRect(0).center();
		const QPointF to(preview->blockRect(count - 1).right() - 2, from.y());
		send(preview, QEvent::MouseButtonPress, from, Qt::LeftButton);
		send(preview, QEvent::MouseMove, to, Qt::LeftButton);
		send(preview, QEvent::MouseButtonRelease, to, Qt::NoButton);
		QCOMPARE(designer.design().blocks.last().kind, moved);
		QCOMPARE(designer.selectedBlock(), count - 1);
	}

	void theDataGoToTheProject()
	{
		QETProject project;
		Diagram *folio = project.addNewDiagram();
		TitleBlockDataDialog dialog(&project);
		auto *client = dialog.findChild<QLineEdit *>(QStringLiteral("client"));
		QVERIFY(client);
		QVERIFY(!dialog.command());
		client->setText(QStringLiteral("Aguas del Sur"));
		QUndoCommand *command = dialog.command();
		QVERIFY(command);
		project.undoStack()->push(command);
		QCOMPARE(project.projectProperties().value(QStringLiteral("client")).toString(),
				 QStringLiteral("Aguas del Sur"));
		QCOMPARE(folio->border_and_titleblock.titleblockInformation()
				 .value(QStringLiteral("client")).toString(), QStringLiteral("Aguas del Sur"));
		project.undoStack()->undo();
		QVERIFY(!project.projectProperties().contains(QStringLiteral("client")));
	}
};

QET_APP_TEST_MAIN(tst_titleblockdesign)
#include "tst_titleblockdesign.moc"
