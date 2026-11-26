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
#include "cabinetthermal.h"

#include "cabinetenclosure.h"
#include "cabinetitem.h"
#include "cabinetparts.h"
#include "../diagram.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QLabel>
#include <QLinearGradient>
#include <QLocale>
#include <QSet>
#include <QSettings>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

QString CabinetThermal::placementName(Placement placement)
{
	switch (placement) {
		case Placement::FreeStanding:
			return QCoreApplication::translate("CabinetThermal", "Free-standing, every side free");
		case Placement::AgainstWall:
			return QCoreApplication::translate("CabinetThermal", "Against a wall");
		case Placement::EndOfRow:
			return QCoreApplication::translate("CabinetThermal", "At the end of a row");
		case Placement::EndOfRowAgainstWall:
			return QCoreApplication::translate("CabinetThermal", "At the end of a row, against a wall");
		case Placement::MiddleOfRow:
			return QCoreApplication::translate("CabinetThermal", "In the middle of a row");
		case Placement::MiddleOfRowAgainstWall:
			return QCoreApplication::translate("CabinetThermal", "In the middle of a row, against a wall");
		case Placement::MiddleOfRowAgainstWallTopCovered:
			return QCoreApplication::translate("CabinetThermal",
											   "In the middle of a row, against a wall, its top covered");
	}
	return QString();
}

QString CabinetThermal::materialName(Material material)
{
	switch (material) {
		case Material::PaintedSteel:
			return QCoreApplication::translate("CabinetThermal", "Painted sheet steel (5.5 W/m²K)");
		case Material::StainlessSteel:
			return QCoreApplication::translate("CabinetThermal", "Stainless steel (4.5 W/m²K)");
		case Material::Aluminium:
			return QCoreApplication::translate("CabinetThermal", "Aluminium (12 W/m²K)");
		case Material::Plastic:
			return QCoreApplication::translate("CabinetThermal", "Plastic (3.5 W/m²K)");
	}
	return QString();
}

double CabinetThermal::transferCoefficient(Material material)
{
	switch (material) {
		case Material::PaintedSteel:   return 5.5;
		case Material::StainlessSteel: return 4.5;
		case Material::Aluminium:      return 12.0;
		case Material::Plastic:        return 3.5;
	}
	return 5.5;
}

double CabinetThermal::effectiveSurface(double width, double height, double depth, Placement placement)
{
		//m; a wall free gives out 1.8 its surface, the top 1.4, a wall
		//against another cabinet or the building nothing more than it is
	const double w = width / 1000.0, h = height / 1000.0, d = depth / 1000.0;
	switch (placement) {
		case Placement::FreeStanding:
			return 1.8 * h * (w + d) + 1.4 * w * d;
		case Placement::AgainstWall:
			return 1.4 * w * (h + d) + 1.8 * d * h;
		case Placement::EndOfRow:
			return 1.4 * d * (h + w) + 1.8 * w * h;
		case Placement::EndOfRowAgainstWall:
			return 1.4 * h * (w + d) + 1.4 * w * d;
		case Placement::MiddleOfRow:
			return 1.8 * w * h + 1.4 * w * d + d * h;
		case Placement::MiddleOfRowAgainstWall:
			return 1.4 * w * (h + d) + d * h;
		case Placement::MiddleOfRowAgainstWallTopCovered:
			return 1.4 * w * h + 0.7 * w * d + d * h;
	}
	return 0;
}

CabinetThermal::Result CabinetThermal::compute(const Inputs &inputs, const QList<Device> &devices)
{
	Result result;
	for (const Device &device : devices) {
		result.losses += device.watts;
		if (!device.known) result.missing << device.label;
	}
	result.surface = effectiveSurface(inputs.width, inputs.height, inputs.depth, inputs.placement);
	const double k = transferCoefficient(inputs.material);
	const double gives_out = k * result.surface;
	result.rise = gives_out > 0 ? result.losses / gives_out : 0;
	result.inside = inputs.ambient + result.rise;
	if (result.inside > inputs.maximum + 1e-9) {
			//What the walls give out at the maximum, outside the rest
		result.cooling = result.losses - gives_out * (inputs.maximum - inputs.ambient);
		const double margin = inputs.maximum - inputs.ambient;
			//3.1 m³ K per Wh: air at sea level
		result.airflow = margin > 0 ? 3.1 * result.cooling / margin : 0;
	}
	return result;
}

QList<CabinetThermal::Device> CabinetThermal::devicesOf(Diagram *folio)
{
	QList<Device> devices;
	if (!folio) return devices;
	struct Placed { QUuid symbol; QString label; QList<PartsLinks::Link> links; bool own_links = false; QRectF area; };
	QList<Placed> placed;
	QSet<QUuid> symbols;
	QStringList seen;
	auto add = [&placed, &symbols, &seen](const QUuid &symbol, const QString &label,
										   const QList<PartsLinks::Link> &links, const QRectF &area) {
		const QString key = symbol.isNull() ? QStringLiteral("label:") + label.trimmed()
											: symbol.toString();
		if (label.trimmed().isEmpty() && symbol.isNull()) return;
		if (seen.contains(key)) return;
		seen << key;
		placed.append({symbol, label.trimmed(), links, !links.isEmpty(), area});
		if (!symbol.isNull()) symbols.insert(symbol);
	};
	for (QGraphicsItem *item : folio->items()) {
		auto *part = qgraphicsitem_cast<CabinetItem *>(item);
		if (!part || !part->isVisible()) continue;
		const CabinetItem::Properties properties = part->properties();
		if (properties.kind == CabinetItem::Kind::Drawing) {
			add(properties.device, properties.device_label, {}, part->mapRectToScene(part->rect()));
		} else if (properties.kind == CabinetItem::Kind::MountingPlate && !properties.enclosure.isEmpty()) {
				//The devices placed in 3D only: on the door, a side, the roof
			const CabinetEnclosure::Enclosure enclosure =
					CabinetEnclosure::Enclosure::fromString(properties.enclosure);
			for (const CabinetEnclosure::Mounted &mounted : enclosure.mounted) {
				QList<PartsLinks::Link> links;
				if (!mounted.order_number.isEmpty())
					links.append({mounted.manufacturer, mounted.order_number, 1});
				add(mounted.symbol, mounted.label, links, QRectF());
			}
		}
	}

	const CabinetParts parts(folio, symbols);
	for (const Placed &each : std::as_const(placed)) {
		Device device;
		device.symbol = each.symbol;
		device.label = each.label.isEmpty() ? QStringLiteral("?") : each.label;
		const QList<PartsLinks::Link> links = each.own_links
				? each.links : parts.linksOf(each.symbol, each.label);
		QStringList names;
		bool every_known = !links.isEmpty();
		for (const PartsLinks::Link &link : links) {
			names << QStringLiteral("%1 %2").arg(link.manufacturer, link.order_number).trimmed();
			const std::optional<Part> part = parts.part(link);
			if (part && part->power_loss) {
				device.watts += *part->power_loss * std::max(1.0, link.units);
			} else {
				every_known = false;
			}
		}
		device.parts = names.join(QStringLiteral(", "));
		device.known = every_known;
		device.area = each.area;
		devices << device;
	}
	std::sort(devices.begin(), devices.end(), [](const Device &a, const Device &b) {
		return QString::localeAwareCompare(a.label, b.label) < 0;
	});
	return devices;
}

bool CabinetThermal::enclosureSize(Diagram *folio, double *width, double *height, double *depth)
{
	if (!folio) return false;
	for (QGraphicsItem *item : folio->items()) {
		auto *plate = qgraphicsitem_cast<CabinetItem *>(item);
		if (!plate || plate->properties().kind != CabinetItem::Kind::MountingPlate) continue;
		const QString text = plate->properties().enclosure;
		if (text.isEmpty()) continue;
		const CabinetEnclosure::Enclosure enclosure = CabinetEnclosure::Enclosure::fromString(text);
		if (enclosure.source == CabinetEnclosure::Enclosure::Source::Manual) {
			*width = enclosure.width;
			*height = enclosure.height;
			*depth = enclosure.depth;
			return true;
		}
		if (enclosure.source == CabinetEnclosure::Enclosure::Source::Parts) {
			const CabinetParts parts(folio);
			const std::optional<Part> part = parts.part(enclosure.manufacturer, enclosure.order_number);
			if (part && part->width && part->height && part->depth) {
				*width = *part->width;
				*height = *part->height;
				*depth = *part->depth;
				return true;
			}
		}
	}
	return false;
}

QIcon CabinetThermalDialog::icon(const QColor &color)
{
	QPixmap pixmap(32, 32);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	QPen pen(color, 2.2);
	painter.setPen(pen);
	painter.setBrush(Qt::NoBrush);
		//The tube and its bulb
	QPainterPath tube;
	tube.addRoundedRect(QRectF(12, 3, 8, 19), 4, 4);
	QPainterPath bulb;
	bulb.addEllipse(QPointF(16, 24), 5.5, 5.5);
	painter.drawPath(tube.united(bulb));
	painter.setPen(Qt::NoPen);
	painter.setBrush(QColor(220, 40, 30));
	painter.drawEllipse(QPointF(16, 24), 3.3, 3.3);
	painter.drawRect(QRectF(14.6, 11, 2.8, 12));
		//The marks of the scale
	painter.setPen(QPen(color, 1.4));
	for (int y : {7, 11, 15}) painter.drawLine(QLineF(22, y, 26, y));
	painter.end();
	return QIcon(pixmap);
}

bool CabinetThermal::plateOf(Diagram *folio, QRectF *area, qreal *per_mm)
{
	if (!folio) return false;
	for (QGraphicsItem *item : folio->items()) {
		auto *plate = qgraphicsitem_cast<CabinetItem *>(item);
		if (!plate || plate->properties().kind != CabinetItem::Kind::MountingPlate || !plate->isVisible())
			continue;
		*area = plate->mapRectToScene(plate->rect());
		*per_mm = plate->pixelsPerMm() > 0 ? plate->pixelsPerMm() : 1;
		return true;
	}
	return false;
}

double CabinetThermal::temperatureAt(const QPointF &point, const QRectF &plate, qreal per_mm,
									 const Inputs &inputs, const Result &result,
									 const QList<Device> &devices)
{
		//The air: warmer at the top, 0.7 of the mean rise at the bottom
		//to 1.3 at the top
	const double height = plate.height() > 0 ? (plate.bottom() - point.y()) / plate.height() : 0.5;
	double t = inputs.ambient + result.rise * (0.7 + 0.6 * qBound(0.0, height, 1.0));
		//Around each device, what it gives off over its faces and the air
		//right around it
	for (const Device &device : devices) {
		if (device.area.isNull() || device.watts <= 0) continue;
		const double w = device.area.width() / per_mm / 1000.0;
		const double h = device.area.height() / per_mm / 1000.0;
		const double rise = device.watts / (8.0 * (4.0 * w * h + 0.05));
		const double dx = qMax(0.0, qMax(device.area.left() - point.x(), point.x() - device.area.right()));
		const double dy = qMax(0.0, qMax(device.area.top() - point.y(), point.y() - device.area.bottom()));
		const double distance = std::hypot(dx, dy) / per_mm;
		const double sigma = qMax(device.area.width(), device.area.height()) / per_mm / 2.0 + 30.0;
		t += rise * std::exp(-distance * distance / (2.0 * sigma * sigma));
	}
	return t;
}

namespace {
		/// Blue, cyan, green, yellow, red for 0 to 1
	QColor heatColor(double f)
	{
		f = qBound(0.0, f, 1.0);
		static const QColor stops[] = {QColor(40, 80, 220), QColor(0, 190, 220), QColor(40, 200, 80),
									   QColor(250, 220, 40), QColor(230, 40, 30)};
		const double scaled = f * 4.0;
		const int i = qMin(3, int(scaled));
		const double r = scaled - i;
		const QColor &a = stops[i], &b = stops[i + 1];
		return QColor(int(a.red() + (b.red() - a.red()) * r), int(a.green() + (b.green() - a.green()) * r),
					  int(a.blue() + (b.blue() - a.blue()) * r));
	}
}

CabinetHeatMap::CabinetHeatMap(QWidget *parent) :
	QWidget(parent)
{
	setObjectName(QStringLiteral("thermal_map"));
	setMinimumHeight(220);
}

void CabinetHeatMap::setData(const QRectF &plate, qreal per_mm, const CabinetThermal::Inputs &inputs,
							 const CabinetThermal::Result &result, const QList<CabinetThermal::Device> &devices)
{
	m_plate = plate;
	m_per_mm = per_mm;
	m_inputs = inputs;
	m_result = result;
	m_devices = devices;
	rebuild();
	update();
}

void CabinetHeatMap::rebuild()
{
	m_image = QImage();
	m_hottest = m_inputs.ambient;
	if (m_plate.isEmpty()) return;
	const int columns = 160;
	const int rows = qMax(20, int(columns * m_plate.height() / m_plate.width()));
	QVector<double> values(columns * rows);
	for (int y = 0 ; y < rows ; ++y) {
		for (int x = 0 ; x < columns ; ++x) {
			const QPointF point(m_plate.left() + (x + 0.5) * m_plate.width() / columns,
								m_plate.top() + (y + 0.5) * m_plate.height() / rows);
			const double t = CabinetThermal::temperatureAt(point, m_plate, m_per_mm, m_inputs, m_result, m_devices);
			values[y * columns + x] = t;
			m_hottest = qMax(m_hottest, t);
		}
	}
	const double top = qMax(m_inputs.maximum + 5.0, m_hottest);
	m_image = QImage(columns, rows, QImage::Format_RGB32);
	for (int y = 0 ; y < rows ; ++y)
		for (int x = 0 ; x < columns ; ++x)
			m_image.setPixelColor(x, y, heatColor((values[y * columns + x] - m_inputs.ambient)
												  / qMax(1.0, top - m_inputs.ambient)));
}

void CabinetHeatMap::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event)
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.fillRect(rect(), palette().base());
	if (m_image.isNull()) {
		painter.setPen(palette().color(QPalette::PlaceholderText));
		painter.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap,
						 QCoreApplication::translate("CabinetThermal", "Draw the mounting plate on the sheet to see where the heat is."));
		return;
	}
		//The plate, as large as it goes, the legend on its right
	const int legend = 70;
	const QRectF room = QRectF(rect()).adjusted(8, 8, -8 - legend, -8);
	const double scale = qMin(room.width() / m_plate.width(), room.height() / m_plate.height());
	const QRectF drawn(room.left(), room.top(), m_plate.width() * scale, m_plate.height() * scale);
	painter.drawImage(drawn, m_image.scaled(int(drawn.width()), int(drawn.height()),
											Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	painter.setPen(QPen(palette().color(QPalette::WindowText), 1));
	painter.setBrush(Qt::NoBrush);
	painter.drawRect(drawn);
	QFont small = font();
	small.setPointSizeF(qMax(6.5, small.pointSizeF() - 1.5));
	painter.setFont(small);
	for (const CabinetThermal::Device &device : std::as_const(m_devices)) {
		if (device.area.isNull()) continue;
		const QRectF box((device.area.left() - m_plate.left()) * scale + drawn.left(),
						 (device.area.top() - m_plate.top()) * scale + drawn.top(),
						 device.area.width() * scale, device.area.height() * scale);
		painter.setPen(QPen(QColor(20, 20, 20, 200), 1));
		painter.setBrush(QColor(255, 255, 255, device.known ? 60 : 30));
		painter.drawRect(box);
		painter.setPen(Qt::black);
		const QString text = device.known
				? QStringLiteral("%1\n%2 W").arg(device.label, QLocale().toString(device.watts, 'f', 1))
				: device.label + QStringLiteral("\n? W");
		painter.drawText(box.adjusted(-20, -2, 20, 2), Qt::AlignCenter, text);
	}
		//The legend: outside to the top of the scale
	const double top = qMax(m_inputs.maximum + 5.0, m_hottest);
	const QRectF bar(width() - legend + 6, room.top(), 14, room.height());
	QLinearGradient gradient(bar.bottomLeft(), bar.topLeft());
	for (int i = 0 ; i <= 4 ; ++i) gradient.setColorAt(i / 4.0, heatColor(i / 4.0));
	painter.setPen(Qt::NoPen);
	painter.setBrush(gradient);
	painter.drawRect(bar);
	painter.setPen(palette().color(QPalette::WindowText));
	auto mark = [&](double t) {
		const double y = bar.bottom() - (t - m_inputs.ambient) / qMax(1.0, top - m_inputs.ambient) * bar.height();
		painter.drawLine(QPointF(bar.right(), y), QPointF(bar.right() + 4, y));
		painter.drawText(QPointF(bar.right() + 6, y + 4), QStringLiteral("%1 °C").arg(QLocale().toString(t, 'f', 0)));
	};
	mark(m_inputs.ambient);
	mark(m_inputs.maximum);
	mark(top);
}

namespace {
	QDoubleSpinBox *spin(QWidget *parent, double from, double to, const QString &suffix,
						 const QString &name, int decimals = 0)
	{
		auto *box = new QDoubleSpinBox(parent);
		box->setRange(from, to);
		box->setDecimals(decimals);
		box->setSuffix(suffix);
		box->setObjectName(name);
		return box;
	}
}

CabinetThermalDialog::CabinetThermalDialog(Diagram *folio, QWidget *parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Thermal analysis of the cabinet"));
	m_devices = CabinetThermal::devicesOf(folio);

	auto *layout = new QVBoxLayout(this);
	auto *form = new QFormLayout();
	auto *size = new QHBoxLayout();
	m_width = spin(this, 50, 10000, tr(" mm"), QStringLiteral("thermal_width"));
	m_height = spin(this, 50, 10000, tr(" mm"), QStringLiteral("thermal_height"));
	m_depth = spin(this, 20, 5000, tr(" mm"), QStringLiteral("thermal_depth"));
	size->addWidget(m_width);
	size->addWidget(new QLabel(QStringLiteral("×"), this));
	size->addWidget(m_height);
	size->addWidget(new QLabel(QStringLiteral("×"), this));
	size->addWidget(m_depth);
	form->addRow(tr("Enclosure (width × height × depth):"), size);
	m_placement = new QComboBox(this);
	m_placement->setObjectName(QStringLiteral("thermal_placement"));
	for (int i = 0 ; i <= int(CabinetThermal::Placement::MiddleOfRowAgainstWallTopCovered) ; ++i)
		m_placement->addItem(CabinetThermal::placementName(CabinetThermal::Placement(i)), i);
	form->addRow(tr("Where it stands:"), m_placement);
	m_material = new QComboBox(this);
	m_material->setObjectName(QStringLiteral("thermal_material"));
	for (int i = 0 ; i <= int(CabinetThermal::Material::Plastic) ; ++i)
		m_material->addItem(CabinetThermal::materialName(CabinetThermal::Material(i)), i);
	form->addRow(tr("Enclosure made of:"), m_material);
	m_ambient = spin(this, -40, 70, tr(" °C"), QStringLiteral("thermal_ambient"), 1);
	form->addRow(tr("Temperature outside:"), m_ambient);
	m_maximum = spin(this, -20, 90, tr(" °C"), QStringLiteral("thermal_maximum"), 1);
	m_maximum->setToolTip(tr("The highest the devices may have inside, often 35 to 40 °C"));
	form->addRow(tr("Highest temperature inside:"), m_maximum);
	layout->addLayout(form);

	m_result_label = new QLabel(this);
	m_result_label->setObjectName(QStringLiteral("thermal_result"));
	m_result_label->setWordWrap(true);
	m_result_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
	layout->addWidget(m_result_label);
	m_warning = new QLabel(this);
	m_warning->setObjectName(QStringLiteral("thermal_warning"));
	m_warning->setWordWrap(true);
	m_warning->setStyleSheet(QStringLiteral("QLabel {background: #fff3cd; color: #664d03; "
											"border: 1px solid #ffda6a; padding: 6px;}"));
	layout->addWidget(m_warning);

		//Where the heat is (Brandon, 2026-11-26)
	CabinetThermal::plateOf(folio, &m_plate, &m_per_mm);
	m_map = new CabinetHeatMap(this);
	layout->addWidget(m_map, 2);
	auto *map_note = new QLabel(tr("Where the heat is: the hot air goes up, and each device warms "
								   "what is around it. An estimate to see the hot spots, not a "
								   "simulation of the air."), this);
	map_note->setWordWrap(true);
	map_note->setStyleSheet(QStringLiteral("color: gray;"));
	layout->addWidget(map_note);

	m_list = new QTreeWidget(this);
	m_list->setObjectName(QStringLiteral("thermal_devices"));
	m_list->setRootIsDecorated(false);
	m_list->setHeaderLabels({tr("Device"), tr("Part"), tr("Power loss")});
	m_list->header()->setSectionResizeMode(1, QHeaderView::Stretch);
	for (const CabinetThermal::Device &device : std::as_const(m_devices)) {
		auto *row = new QTreeWidgetItem(m_list);
		row->setText(0, device.label);
		row->setText(1, device.parts.isEmpty() ? tr("(no part)") : device.parts);
		row->setText(2, device.known ? tr("%1 W").arg(QLocale().toString(device.watts, 'f', 1))
									 : tr("not given"));
		row->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
		if (!device.known) {
			for (int column = 0 ; column < 3 ; ++column)
				row->setForeground(column, QColor(180, 90, 0));
		}
	}
	layout->addWidget(m_list, 1);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);

		//Last time's surroundings, the enclosure of this cabinet
	QSettings settings;
	CabinetThermal::Inputs inputs;
	inputs.placement = CabinetThermal::Placement(settings.value(QStringLiteral("cabinet-thermal/placement"),
																int(inputs.placement)).toInt());
	inputs.material = CabinetThermal::Material(settings.value(QStringLiteral("cabinet-thermal/material"),
															  int(inputs.material)).toInt());
	inputs.ambient = settings.value(QStringLiteral("cabinet-thermal/ambient"), inputs.ambient).toDouble();
	inputs.maximum = settings.value(QStringLiteral("cabinet-thermal/maximum"), inputs.maximum).toDouble();
	CabinetThermal::enclosureSize(folio, &inputs.width, &inputs.height, &inputs.depth);
	setInputs(inputs);

	for (QDoubleSpinBox *box : {m_width, m_height, m_depth, m_ambient, m_maximum})
		connect(box, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &CabinetThermalDialog::update);
	for (QComboBox *box : {m_placement, m_material})
		connect(box, qOverload<int>(&QComboBox::currentIndexChanged), this, &CabinetThermalDialog::update);
	connect(this, &QDialog::finished, this, [this]() {
		QSettings settings;
		const CabinetThermal::Inputs now = this->inputs();
		settings.setValue(QStringLiteral("cabinet-thermal/placement"), int(now.placement));
		settings.setValue(QStringLiteral("cabinet-thermal/material"), int(now.material));
		settings.setValue(QStringLiteral("cabinet-thermal/ambient"), now.ambient);
		settings.setValue(QStringLiteral("cabinet-thermal/maximum"), now.maximum);
	});
	resize(720, 820);
}

CabinetThermal::Inputs CabinetThermalDialog::inputs() const
{
	CabinetThermal::Inputs inputs;
	inputs.width = m_width->value();
	inputs.height = m_height->value();
	inputs.depth = m_depth->value();
	inputs.placement = CabinetThermal::Placement(m_placement->currentData().toInt());
	inputs.material = CabinetThermal::Material(m_material->currentData().toInt());
	inputs.ambient = m_ambient->value();
	inputs.maximum = m_maximum->value();
	return inputs;
}

void CabinetThermalDialog::setInputs(const CabinetThermal::Inputs &inputs)
{
	const QSignalBlocker blockers[] = {QSignalBlocker(m_width), QSignalBlocker(m_height),
									   QSignalBlocker(m_depth), QSignalBlocker(m_ambient),
									   QSignalBlocker(m_maximum), QSignalBlocker(m_placement),
									   QSignalBlocker(m_material)};
	m_width->setValue(inputs.width);
	m_height->setValue(inputs.height);
	m_depth->setValue(inputs.depth);
	m_ambient->setValue(inputs.ambient);
	m_maximum->setValue(inputs.maximum);
	m_placement->setCurrentIndex(std::max(0, m_placement->findData(int(inputs.placement))));
	m_material->setCurrentIndex(std::max(0, m_material->findData(int(inputs.material))));
	update();
}

QString CabinetThermalDialog::resultText() const
{
	return m_result_label->text();
}

QString CabinetThermalDialog::warningText() const
{
	return m_warning->isHidden() ? QString() : m_warning->text();
}

void CabinetThermalDialog::update()
{
	m_result = CabinetThermal::compute(inputs(), m_devices);
	if (m_map) m_map->setData(m_plate, m_per_mm, inputs(), m_result, m_devices);
	const QLocale locale;
	auto number = [&locale](double value, int decimals = 0) {
		return locale.toString(value, 'f', decimals);
	};
	QString text = tr("Heat given off: <b>%1 W</b> by %n device(s).", "", int(m_devices.size()))
			.arg(number(m_result.losses, 1));
	text += QStringLiteral("<br>") + tr("Surface giving it out: %1 m².").arg(number(m_result.surface, 2));
	text += QStringLiteral("<br>") + tr("Temperature inside: <b>%1 °C</b>, %2 K above the outside.")
			.arg(number(m_result.inside, 1), number(m_result.rise, 1));
	if (!m_result.needsCooling()) {
		text += QStringLiteral("<br><span style=\"color:#1a7f37\">")
				+ tr("Below the highest temperature: the enclosure needs no cooling.")
				+ QStringLiteral("</span>");
	} else if (m_result.airflow > 0) {
		text += QStringLiteral("<br><span style=\"color:#b42318\">")
				+ tr("Too hot: %1 W to take out, with a filter fan of at least %2 m³/h, "
					 "or a cooling unit of at least %1 W.")
				  .arg(number(m_result.cooling), number(m_result.airflow))
				+ QStringLiteral("</span>");
	} else {
		text += QStringLiteral("<br><span style=\"color:#b42318\">")
				+ tr("Too hot: a cooling unit of at least %1 W. A fan cannot help: "
					 "the outside is not cooler than the highest temperature.")
				  .arg(number(m_result.cooling))
				+ QStringLiteral("</span>");
	}
	m_result_label->setText(text);

	if (m_result.missing.isEmpty()) {
		m_warning->hide();
	} else {
		m_warning->setText(tr("Warning: %n device(s) have no power loss in the parts database and "
							  "count as 0 W, so the cabinet is hotter than shown: %1",
							  "", int(m_result.missing.size()))
						   .arg(m_result.missing.join(QStringLiteral(", "))));
		m_warning->show();
	}
}
