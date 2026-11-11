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
#include "cabinetdialogs.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QVBoxLayout>

#include <cmath>

namespace {

QString number(qreal value)
{
	return QString::number(std::round(value * 100.0) / 100.0, 'g', 8);
}

	/// @p text read as a measure: a comma is a decimal point too
qreal measure(QString text, bool *ok)
{
	text = text.trimmed();
	text.remove(QStringLiteral("mm"), Qt::CaseInsensitive);
	text = text.trimmed().replace(QLatin1Char(','), QLatin1Char('.'));
	const qreal value = text.toDouble(ok);
	if (ok && *ok && value <= 0) *ok = false;
	return value;
}

	/// A list of usual measures in which another one can be typed
QComboBox *measureCombo(const QList<qreal> &values, qreal current, QWidget *parent)
{
	auto *combo = new QComboBox(parent);
	combo->setEditable(true);
	combo->setInsertPolicy(QComboBox::NoInsert);
	for (qreal value : values) combo->addItem(number(value));
	combo->setEditText(number(current));
	combo->setMinimumWidth(110);
	return combo;
}

QDoubleSpinBox *measureSpin(qreal value, QWidget *parent)
{
	auto *spin = new QDoubleSpinBox(parent);
	spin->setRange(1.0, 100000.0);
	spin->setDecimals(1);
	spin->setSuffix(QStringLiteral(" mm"));
	spin->setValue(value);
	spin->setMinimumWidth(110);
	return spin;
}

QDialogButtonBox *buttonsOf(QDialog *dialog)
{
	auto *buttons = new QDialogButtonBox(
				QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
	QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
	return buttons;
}

QComboBox *orientationCombo(Qt::Orientation orientation, QWidget *parent)
{
	auto *combo = new QComboBox(parent);
	combo->addItem(CabinetDialogs::tr("Horizontal"), int(Qt::Horizontal));
	combo->addItem(CabinetDialogs::tr("Vertical"), int(Qt::Vertical));
	combo->setCurrentIndex(orientation == Qt::Vertical ? 1 : 0);
	return combo;
}

	/// The colour the icons of the cabinet are drawn in: read on a light
	/// and on a dark theme alike
const QColor icon_color(86, 132, 190);

} // namespace

/**
	@brief CabinetDialogs::ductWidths
	@return the usual widths of wire duct, in millimetres
*/
QList<qreal> CabinetDialogs::ductWidths()
{
	return {15, 25, 30, 40, 60, 80, 100, 120};
}

/**
	@brief CabinetDialogs::scales
	@return the usual scales of a drawing: 5 stands for 1:5
*/
QList<qreal> CabinetDialogs::scales()
{
	return {1, 2, 2.5, 5, 10, 20, 50};
}

/**
	@brief CabinetDialogs::plateSizes
	@return usual widths and heights of mounting plate, in millimetres
*/
QList<QSizeF> CabinetDialogs::plateSizes()
{
	return {{300, 400}, {400, 500}, {400, 600}, {500, 700}, {600, 600},
			{600, 800}, {600, 1000}, {800, 800}, {800, 1000}, {800, 1200},
			{1000, 1000}, {1000, 1200}, {1200, 1800}};
}

QString CabinetDialogs::scaleText(qreal scale)
{
	return QStringLiteral("1:%1").arg(number(scale));
}

/**
	@brief CabinetDialogs::askDuct
	Ask for the width and the height of the wire duct to draw.
*/
bool CabinetDialogs::askDuct(qreal &width, qreal &depth, QWidget *parent)
{
	QSettings settings;
	width = settings.value(QStringLiteral("cabinet/duct_width"), 40.0).toDouble();
	depth = settings.value(QStringLiteral("cabinet/duct_depth"), 60.0).toDouble();

	QDialog dialog(parent);
	dialog.setWindowTitle(tr("Add a wire duct"));
	auto *layout = new QVBoxLayout(&dialog);
	auto *form = new QFormLayout();
	QComboBox *width_combo = measureCombo(ductWidths(), width, &dialog);
	QComboBox *depth_combo = measureCombo({25, 40, 60, 80, 100}, depth, &dialog);
	form->addRow(tr("Width (mm):"), width_combo);
	form->addRow(tr("Height (mm):"), depth_combo);
	layout->addLayout(form);
	auto *hint = new QLabel(tr("Then draw the duct on the sheet: click where it "
							   "starts, click again where it ends."), &dialog);
	hint->setWordWrap(true);
	layout->addWidget(hint);
	auto *error = new QLabel(&dialog);
	error->setStyleSheet(QStringLiteral("color: #c0392b;"));
	error->hide();
	layout->addWidget(error);
	layout->addWidget(buttonsOf(&dialog));
	width_combo->lineEdit()->selectAll();
	width_combo->setFocus();

	while (dialog.exec() == QDialog::Accepted) {
		bool width_ok = false;
		bool depth_ok = false;
		const qreal new_width = measure(width_combo->currentText(), &width_ok);
		const qreal new_depth = measure(depth_combo->currentText(), &depth_ok);
		if (width_ok && depth_ok) {
			width = new_width;
			depth = new_depth;
			settings.setValue(QStringLiteral("cabinet/duct_width"), width);
			settings.setValue(QStringLiteral("cabinet/duct_depth"), depth);
			return true;
		}
		error->setText(tr("Type the width and the height in millimetres, for example 40."));
		error->show();
	}
	return false;
}

/**
	@brief CabinetDialogs::askPlate
	Ask for the width and the height of the mounting plate to place.
*/
bool CabinetDialogs::askPlate(QSizeF &size, QWidget *parent)
{
	QSettings settings;
	size = QSizeF(settings.value(QStringLiteral("cabinet/plate_width"), 600.0).toDouble(),
				  settings.value(QStringLiteral("cabinet/plate_height"), 800.0).toDouble());

	QDialog dialog(parent);
	dialog.setWindowTitle(tr("Add a mounting plate"));
	auto *layout = new QVBoxLayout(&dialog);
	auto *form = new QFormLayout();
	auto *presets = new QComboBox(&dialog);
	presets->addItem(tr("Other size"));
	const QList<QSizeF> sizes = plateSizes();
	for (const QSizeF &preset : sizes) {
		presets->addItem(tr("%1 × %2 mm", "width and height of a mounting plate")
						 .arg(number(preset.width()), number(preset.height())),
						 preset);
	}
	QDoubleSpinBox *width_spin = measureSpin(size.width(), &dialog);
	QDoubleSpinBox *height_spin = measureSpin(size.height(), &dialog);
	form->addRow(tr("Usual sizes:"), presets);
	form->addRow(tr("Width:"), width_spin);
	form->addRow(tr("Height:"), height_spin);
	layout->addLayout(form);
	auto *hint = new QLabel(tr("Then click on the sheet where the plate goes."), &dialog);
	hint->setWordWrap(true);
	layout->addWidget(hint);
	layout->addWidget(buttonsOf(&dialog));

	const int current = sizes.indexOf(size);
	presets->setCurrentIndex(current < 0 ? 0 : current + 1);
	QObject::connect(presets, &QComboBox::activated, &dialog, [=](int index) {
		const QSizeF preset = presets->itemData(index).toSizeF();
		if (!preset.isValid()) return;
		width_spin->setValue(preset.width());
		height_spin->setValue(preset.height());
	});
	auto typed = [=]() {
		const int index = plateSizes().indexOf(QSizeF(width_spin->value(), height_spin->value()));
		presets->setCurrentIndex(index < 0 ? 0 : index + 1);
	};
	QObject::connect(width_spin, &QDoubleSpinBox::valueChanged, &dialog, typed);
	QObject::connect(height_spin, &QDoubleSpinBox::valueChanged, &dialog, typed);
	width_spin->selectAll();
	width_spin->setFocus();

	if (dialog.exec() != QDialog::Accepted) return false;
	size = QSizeF(width_spin->value(), height_spin->value());
	settings.setValue(QStringLiteral("cabinet/plate_width"), size.width());
	settings.setValue(QStringLiteral("cabinet/plate_height"), size.height());
	return true;
}

/**
	@brief CabinetDialogs::askScale
	Ask for the scale of a cabinet folio; @p scale is the one it has.
*/
bool CabinetDialogs::askScale(qreal &scale, QWidget *parent)
{
	QDialog dialog(parent);
	dialog.setWindowTitle(tr("Scale of the cabinet"));
	auto *layout = new QVBoxLayout(&dialog);
	auto *form = new QFormLayout();
	auto *combo = new QComboBox(&dialog);
	combo->setEditable(true);
	combo->setInsertPolicy(QComboBox::NoInsert);
	const QList<qreal> usual = scales();
	for (qreal value : usual) combo->addItem(scaleText(value));
	combo->setEditText(scaleText(scale));
	form->addRow(tr("Scale:"), combo);
	layout->addLayout(form);
	auto *hint = new QLabel(tr("At 1:5, 5 mm of the cabinet take 1 mm of the sheet. "
							   "The parts keep their measures: they are drawn again "
							   "at the new scale."), &dialog);
	hint->setWordWrap(true);
	layout->addWidget(hint);
	auto *error = new QLabel(&dialog);
	error->setStyleSheet(QStringLiteral("color: #c0392b;"));
	error->hide();
	layout->addWidget(error);
	layout->addWidget(buttonsOf(&dialog));

	while (dialog.exec() == QDialog::Accepted) {
		QString text = combo->currentText().trimmed();
		const int colon = text.indexOf(QLatin1Char(':'));
		bool ok = true;
		qreal first = 1.0;
		if (colon >= 0) {
			first = measure(text.left(colon), &ok);
			text = text.mid(colon + 1);
		}
		bool second_ok = false;
		const qreal second = measure(text, &second_ok);
		if (ok && second_ok && second / first >= 0.1 && second / first <= 1000.0) {
			scale = second / first;
			return true;
		}
		error->setText(tr("Type a scale such as 1:5."));
		error->show();
	}
	return false;
}

/**
	@brief CabinetDialogs::editItem
	Show the measures of a part of a cabinet and let the user change them.
*/
bool CabinetDialogs::editItem(CabinetItem::Properties &properties, QWidget *parent)
{
	using Kind = CabinetItem::Kind;
	QDialog dialog(parent);
	auto *layout = new QVBoxLayout(&dialog);
	auto *form = new QFormLayout();
	layout->addLayout(form);

	QDoubleSpinBox *length_spin = measureSpin(properties.length, &dialog);
	QDoubleSpinBox *width_spin = measureSpin(properties.width, &dialog);
	QDoubleSpinBox *depth_spin = nullptr;
	QComboBox *profile_combo = nullptr;
	QComboBox *orientation_combo = nullptr;

	switch (properties.kind) {
		case Kind::MountingPlate:
			dialog.setWindowTitle(tr("Mounting plate"));
			form->addRow(tr("Width:"), length_spin);
			form->addRow(tr("Height:"), width_spin);
			break;
		case Kind::DinRail: {
			dialog.setWindowTitle(tr("DIN rail"));
			profile_combo = new QComboBox(&dialog);
			const QList<CabinetItem::RailProfile> profiles = CabinetItem::railProfiles();
			for (const CabinetItem::RailProfile &profile : profiles)
				profile_combo->addItem(profile.name);
			profile_combo->setCurrentText(properties.profile);
			form->addRow(tr("Profile:"), profile_combo);
			form->addRow(tr("Length:"), length_spin);
			width_spin->hide();
			orientation_combo = orientationCombo(properties.orientation, &dialog);
			form->addRow(tr("Direction:"), orientation_combo);
			break;
		}
		case Kind::WireDuct:
			dialog.setWindowTitle(tr("Wire duct"));
			depth_spin = measureSpin(qMax(1.0, properties.depth), &dialog);
			form->addRow(tr("Width:"), width_spin);
			form->addRow(tr("Height:"), depth_spin);
			form->addRow(tr("Length:"), length_spin);
			orientation_combo = orientationCombo(properties.orientation, &dialog);
			form->addRow(tr("Direction:"), orientation_combo);
			break;
		case Kind::Drawing:
			return false;
	}
	layout->addWidget(buttonsOf(&dialog));
	length_spin->selectAll();
	length_spin->setFocus();

	if (dialog.exec() != QDialog::Accepted) return false;

	properties.length = length_spin->value();
	if (profile_combo) {
		const CabinetItem::RailProfile profile =
				CabinetItem::railProfile(profile_combo->currentText());
		properties.profile = profile.name;
		properties.width = profile.width;
		properties.depth = profile.depth;
	} else {
		properties.width = width_spin->value();
	}
	if (depth_spin) properties.depth = depth_spin->value();
	if (orientation_combo)
		properties.orientation = Qt::Orientation(orientation_combo->currentData().toInt());
	return true;
}

/**
	@brief CabinetDialogs::folioIcon
	@return the icon of a cabinet folio and of the commands of the cabinet:
	a cabinet with three rails, two devices on each, drawn in @p color, or
	in the colour of the text if @p color is not valid
*/
QIcon CabinetDialogs::folioIcon(const QColor &color)
{
	const QColor ink = color.isValid()
			? color : QGuiApplication::palette().color(QPalette::Text);
	QIcon icon;
	for (int size : {16, 32}) {
		QPixmap pixmap(size, size);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		painter.setRenderHint(QPainter::Antialiasing, true);
		const qreal unit = size / 32.0;
		painter.scale(unit, unit);
		painter.setPen(QPen(ink, size < 32 ? 2.4 : 2.0));
		painter.drawRect(QRectF(5, 3, 22, 26));
		painter.setPen(QPen(ink, size < 32 ? 1.8 : 1.5));
		for (int y : {10, 17, 24}) {
			painter.drawLine(QPointF(8, y), QPointF(24, y));
			painter.drawRect(QRectF(10, y - 4, 4, 4));
			painter.drawRect(QRectF(17, y - 4, 4, 4));
		}
		painter.end();
		icon.addPixmap(pixmap);
	}
	return icon;
}

/**
	@brief CabinetDialogs::dxfIcon
	@return the icon of the command inserting a DXF drawing: a sheet with
	its corner folded and the front of a device on it, in black, for the
	command tabs to give it the colour of the text on a dark theme
*/
QIcon CabinetDialogs::dxfIcon()
{
	QIcon icon;
	for (int size : {16, 32}) {
		QPixmap pixmap(size, size);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		painter.setRenderHint(QPainter::Antialiasing, true);
		const qreal unit = size / 32.0;
		painter.scale(unit, unit);
		painter.setPen(QPen(Qt::black, size < 32 ? 2.4 : 2.0, Qt::SolidLine,
							Qt::RoundCap, Qt::RoundJoin));
		painter.drawPolyline(QPolygonF({QPointF(20, 3), QPointF(6, 3), QPointF(6, 29),
										QPointF(26, 29), QPointF(26, 9), QPointF(20, 3),
										QPointF(20, 9), QPointF(26, 9)}));
		painter.setPen(QPen(Qt::black, size < 32 ? 1.8 : 1.4));
		painter.drawRect(QRectF(11, 12, 10, 13));
		painter.drawEllipse(QPointF(16, 16.5), 2.2, 2.2);
		painter.drawLine(QPointF(13, 22), QPointF(19, 22));
		painter.end();
		icon.addPixmap(pixmap);
	}
	return icon;
}

/**
	@brief CabinetDialogs::icon
	@return the icon of the command adding a part of @p kind
*/
QIcon CabinetDialogs::icon(CabinetItem::Kind kind)
{
	QIcon icon;
	for (int size : {16, 32}) {
		QPixmap pixmap(size, size);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		painter.setRenderHint(QPainter::Antialiasing, true);
		const qreal unit = size / 32.0;
		painter.scale(unit, unit);
		painter.setPen(QPen(icon_color, 2.2));
		switch (kind) {
			case CabinetItem::Kind::MountingPlate:
				painter.drawRect(QRectF(6, 4, 20, 24));
				for (const QPointF &hole : {QPointF(10, 8), QPointF(22, 8),
											QPointF(10, 24), QPointF(22, 24)})
					painter.drawPoint(hole);
				break;
			case CabinetItem::Kind::DinRail:
				painter.drawRect(QRectF(3, 10, 26, 12));
				painter.setPen(QPen(icon_color, 1.6));
				for (int x : {8, 16, 24})
					painter.drawLine(QPointF(x - 2, 16), QPointF(x + 2, 16));
				break;
			case CabinetItem::Kind::WireDuct:
				painter.drawRect(QRectF(3, 9, 26, 14));
				painter.setPen(QPen(icon_color, 1.4));
				for (int x : {8, 13, 18, 23}) {
					painter.drawLine(QPointF(x, 9), QPointF(x, 13));
					painter.drawLine(QPointF(x, 19), QPointF(x, 23));
				}
				break;
			case CabinetItem::Kind::Drawing:
				return dxfIcon();
		}
		painter.end();
		icon.addPixmap(pixmap);
	}
	return icon;
}
