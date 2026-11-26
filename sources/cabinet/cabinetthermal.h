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
#ifndef CABINETTHERMAL_H
#define CABINETTHERMAL_H

#include <QDialog>
#include <QImage>
#include <QRectF>
#include <QWidget>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUuid>

class Diagram;
class QColor;
class QComboBox;
class QIcon;
class QDoubleSpinBox;
class QLabel;
class QTreeWidget;

/**
	The heat of a cabinet (Brandon, 2026-11-25): what its devices give off,
	the power loss of their parts in the parts database, against what the
	enclosure gives out through its walls, P = k A (Ti - Ta). The surface
	that gives it out, A, is the one the way it stands leaves free, as the
	climate control of enclosures reckons it. The temperature inside, and
	what a fan or a cooling unit has to take out when it is too high.
*/
namespace CabinetThermal
{
		/// How the enclosure stands: which of its walls give heat out
	enum class Placement {
		FreeStanding,
		AgainstWall,
		EndOfRow,
		EndOfRowAgainstWall,
		MiddleOfRow,
		MiddleOfRowAgainstWall,
		MiddleOfRowAgainstWallTopCovered
	};
	enum class Material { PaintedSteel, StainlessSteel, Aluminium, Plastic };

	QString placementName(Placement placement);
	QString materialName(Material material);
		/// W per m² and per K the walls give out
	double transferCoefficient(Material material);
		/// The surface giving heat out, m², of an enclosure of these
		/// measures (mm) standing so
	double effectiveSurface(double width, double height, double depth, Placement placement);

		/// A device of the cabinet and the heat it gives off
	struct Device
	{
		QUuid symbol;
		QString label;
			/// Its parts, "manufacturer order number", one after the other
		QString parts;
		double watts = 0;
			/// Every part of it says its power loss
		bool known = false;
			/// Where its drawing stands on the folio (scene); null for a
			/// device placed in 3D only
		QRectF area;
	};

	struct Inputs
	{
			/// The enclosure, mm
		double width = 600;
		double height = 800;
		double depth = 300;
		Placement placement = Placement::AgainstWall;
		Material material = Material::PaintedSteel;
			/// °C
		double ambient = 35;
		double maximum = 40;
	};

	struct Result
	{
		double losses = 0;		///< W, of every device
		double surface = 0;		///< m²
		double rise = 0;		///< K, inside above outside
		double inside = 0;		///< °C
			/// W to take out to stay at the maximum, 0 when none
		double cooling = 0;
			/// m³/h of outside air a fan blows for it; 0 when no fan can
			/// (the outside is not cooler than the maximum)
		double airflow = 0;
			/// The devices which say no power loss, counted as 0 W
		QStringList missing;
		bool needsCooling() const {return cooling > 0;}
	};

	Result compute(const Inputs &inputs, const QList<Device> &devices);

		/// The devices of the cabinet folio @p folio: its drawings and the
		/// devices placed in 3D only, one per device of the schematic,
		/// with the power loss of their parts
	QList<Device> devicesOf(Diagram *folio);
		/// The measures of the enclosure of the plate of @p folio, mm,
		/// when it has one; false otherwise
	bool enclosureSize(Diagram *folio, double *width, double *height, double *depth);
		/// The mounting plate of @p folio on the folio (scene), and how
		/// many units of the folio a millimetre is; false without a plate
	bool plateOf(Diagram *folio, QRectF *area, qreal *per_mm);
		/// The temperature (°C) at @p point of the plate: the air heated by
		/// the walls' rise, warmer at the top where hot air goes, and the
		/// heat around each device (Brandon, 2026-11-26). A picture of
		/// where the heat is, not a simulation of the air
	double temperatureAt(const QPointF &point, const QRectF &plate, qreal per_mm,
						 const Inputs &inputs, const Result &result, const QList<Device> &devices);
}

/**
	The heat map of the plate: from blue, the temperature outside, to red,
	the highest allowed and above, each device outlined with its watts.
*/
class CabinetHeatMap : public QWidget
{
	Q_OBJECT

	public:
		explicit CabinetHeatMap(QWidget *parent = nullptr);
		void setData(const QRectF &plate, qreal per_mm, const CabinetThermal::Inputs &inputs,
					 const CabinetThermal::Result &result, const QList<CabinetThermal::Device> &devices);
			/// The hottest point of the last map, °C
		double hottest() const {return m_hottest;}
		QSize sizeHint() const override {return QSize(420, 320);}

	protected:
		void paintEvent(QPaintEvent *event) override;

	private:
		void rebuild();

		QRectF m_plate;
		qreal m_per_mm = 1;
		CabinetThermal::Inputs m_inputs;
		CabinetThermal::Result m_result;
		QList<CabinetThermal::Device> m_devices;
		QImage m_image;
		double m_hottest = 0;
};

/**
	The window of the heat of a cabinet folio: the enclosure and its
	surroundings, worked out as they change.
*/
class CabinetThermalDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit CabinetThermalDialog(Diagram *folio, QWidget *parent = nullptr);
			/// A thermometer, drawn in @p color
		static QIcon icon(const QColor &color);

		CabinetThermal::Inputs inputs() const;
		void setInputs(const CabinetThermal::Inputs &inputs);
		CabinetThermal::Result result() const {return m_result;}
			/// The text of the result shown, the warning with it
		QString resultText() const;
		QString warningText() const;

	private:
		void update();

		QList<CabinetThermal::Device> m_devices;
		CabinetThermal::Result m_result;
		QDoubleSpinBox *m_width = nullptr;
		QDoubleSpinBox *m_height = nullptr;
		QDoubleSpinBox *m_depth = nullptr;
		QComboBox *m_placement = nullptr;
		QComboBox *m_material = nullptr;
		QDoubleSpinBox *m_ambient = nullptr;
		QDoubleSpinBox *m_maximum = nullptr;
		QLabel *m_result_label = nullptr;
		QLabel *m_warning = nullptr;
		QTreeWidget *m_list = nullptr;
		CabinetHeatMap *m_map = nullptr;
		QRectF m_plate;
		qreal m_per_mm = 1;
};

#endif // CABINETTHERMAL_H
