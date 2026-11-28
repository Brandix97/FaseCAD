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
#include "structureboxes.h"

#include "QPropertyUndoCommand/qpropertyundocommand.h"
#include "diagram.h"
#include "diagramview.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/qetshapeitem.h"
#include "qetinformation.h"
#include "undocommand/addgraphicsobjectcommand.h"
#include "undocommand/changeelementinformationcommand.h"

#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QUndoStack>

namespace {

	/// Ask the plant and location, starting from @p plant and @p location
bool ask(QWidget *parent, QString &plant, QString &location)
{
	QDialog dialog(parent);
	dialog.setWindowTitle(QCoreApplication::translate("StructureBoxes", "Structure box"));
	auto *layout = new QFormLayout(&dialog);
	layout->addRow(new QLabel(QCoreApplication::translate(
		"StructureBoxes", "The symbols in the box take this plant and location."), &dialog));
	auto *plant_edit = new QLineEdit(plant, &dialog);
	plant_edit->setPlaceholderText(QStringLiteral("A1"));
	auto *location_edit = new QLineEdit(location, &dialog);
	location_edit->setPlaceholderText(QStringLiteral("S1"));
	layout->addRow(QCoreApplication::translate("StructureBoxes", "Plant (=):"), plant_edit);
	layout->addRow(QCoreApplication::translate("StructureBoxes", "Location (+):"), location_edit);
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	layout->addRow(buttons);
	if (dialog.exec() != QDialog::Accepted) {
		return false;
	}
		//Kept without their signs: the information holds A1, not =A1
	plant = plant_edit->text().trimmed();
	location = location_edit->text().trimmed();
	if (plant.startsWith(QLatin1Char('='))) plant.remove(0, 1);
	if (location.startsWith(QLatin1Char('+'))) location.remove(0, 1);
	return true;
}

}

QetShapeItem *StructureBoxes::boxAt(Diagram *folio, const QPointF &point)
{
	QetShapeItem *found = nullptr;
	qreal smallest = 0.0;
	if (!folio) {
		return found;
	}
	for (QGraphicsItem *item : folio->items()) {
		auto *shape = qgraphicsitem_cast<QetShapeItem *>(item);
		if (!shape || !shape->isStructureBox()) {
			continue;
		}
		const QRectF rect = shape->sceneBoundingRect();
		if (!rect.contains(point)) {
			continue;
		}
		const qreal area = rect.width() * rect.height();
		if (!found || area < smallest) {
			found = shape;
			smallest = area;
		}
	}
	return found;
}

void StructureBoxes::apply(Diagram *folio, QUndoCommand *parent, const QList<Element *> &only)
{
	if (!folio || !parent) {
		return;
	}
	bool any_box = false;
	for (QGraphicsItem *item : folio->items()) {
		auto *shape = qgraphicsitem_cast<QetShapeItem *>(item);
		any_box = any_box || (shape && shape->isStructureBox());
	}
	if (!any_box) {
		return;
	}
	const QList<Element *> elements = only.isEmpty() ? folio->elements() : only;
	for (Element *element : elements) {
		if (!element || element->diagram() != folio) {
			continue;
		}
		QetShapeItem *box = boxAt(folio, element->sceneBoundingRect().center());
		if (!box) {
			continue;
		}
		const DiagramContext before = element->elementInformations();
		DiagramContext after = before;
		if (!box->structurePlant().isEmpty()) {
			after.addValue(QETInformation::ELMT_PLANT, box->structurePlant());
		}
		if (!box->structureLocation().isEmpty()) {
			after.addValue(QETInformation::ELMT_LOCATION, box->structureLocation());
		}
		if (after != before) {
			new ChangeElementInformationCommand(element, before, after, parent);
		}
	}
}

void StructureBoxes::insert(DiagramView *view)
{
	Diagram *folio = view ? view->diagram() : nullptr;
	if (!folio || folio->isReadOnly()) {
		return;
	}
	QString plant, location;
	if (!ask(view, plant, location)) {
		return;
	}
	auto *box = new QetShapeItem(QPointF(0, 0), QPointF(300, 200), QetShapeItem::Rectangle);
	QPen pen(Qt::black);
	pen.setStyle(Qt::DashDotLine);
	pen.setWidthF(1.0);
	pen.setCosmetic(true);
	box->setPen(pen);
	box->setStructureBox(true);
	box->setStructurePlant(plant);
	box->setStructureLocation(location);

	const QString text = QCoreApplication::translate("StructureBoxes", "Add a structure box");
	QUndoStack &stack = folio->undoStack();
	stack.beginMacro(text);
	stack.push(new AddGraphicsObjectCommand(box, folio, view->defaultPlacementPos() - QPointF(150, 100)));
		//Now on the folio: what it covers takes its plant and location
	auto *given = new QUndoCommand(text);
	apply(folio, given);
	if (given->childCount()) {
		stack.push(given);
	} else {
		delete given;
	}
	stack.endMacro();
}

void StructureBoxes::edit(QetShapeItem *box, QWidget *parent)
{
	Diagram *folio = box ? box->diagram() : nullptr;
	if (!folio || !box->isStructureBox() || folio->isReadOnly()) {
		return;
	}
	QString plant = box->structurePlant(), location = box->structureLocation();
	if (!ask(parent, plant, location)
		|| (plant == box->structurePlant() && location == box->structureLocation())) {
		return;
	}
	QUndoStack &stack = folio->undoStack();
	stack.beginMacro(QCoreApplication::translate("StructureBoxes", "Edit a structure box"));
	auto *changed = new QUndoCommand(QCoreApplication::translate("StructureBoxes", "Edit a structure box"));
	new QPropertyUndoCommand(box, "structurePlant", box->structurePlant(), plant, changed);
	new QPropertyUndoCommand(box, "structureLocation", box->structureLocation(), location, changed);
	stack.push(changed);
	auto *given = new QUndoCommand(changed->text());
	apply(folio, given);
	if (given->childCount()) {
		stack.push(given);
	} else {
		delete given;
	}
	stack.endMacro();
}
