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
#ifndef CABINET3DVIEW_H
#define CABINET3DVIEW_H

#include "cabinet3d.h"
#include "cabinetenclosure.h"
#include "cabinetspace.h"
#include "cabinetstep.h"

#include <QDialog>
#include <QHash>
#include <QIcon>
#include <QMatrix4x4>
#include <QPointer>
#include <QtGui/QOpenGLFunctions>
#include <QtOpenGL/QOpenGLBuffer>
#include <QtOpenGLWidgets/QOpenGLWidget>

#include <functional>
#include <memory>

class CabinetItem;
class Diagram;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QOpenGLShaderProgram;
class QProgressBar;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
template <typename T> class QFutureWatcher;

/**
	@brief The Cabinet3dView class draws the solids of a cabinet
	(Cabinet3d) with OpenGL, and the enclosure its plate stands in: a
	device with a STEP model as that model, the other parts as boxes.
	Left button turns around the cabinet, right or middle button moves
	it, the wheel comes nearer, a click picks a part, a right click asks
	what to do with it, a double click shows everything again.
*/
class Cabinet3dView : public QOpenGLWidget, protected QOpenGLFunctions
{
	Q_OBJECT

	public:
			/// A solid of the folio, or a node of the enclosure
		struct Pick
		{
			int solid = -1;
			int node = -1;
			bool isNull() const {return solid < 0 && node < 0;}
			bool operator==(const Pick &other) const
			{return solid == other.solid && node == other.node;}
		};

		explicit Cabinet3dView(QWidget *parent = nullptr);
		~Cabinet3dView() override;

		void setSolids(const QList<Cabinet3d::Solid> &solids);
		QList<Cabinet3d::Solid> solids() const {return m_solids;}
			/// The enclosure of the plate solid @p plate and its model
		void setEnclosure(int plate, const CabinetEnclosure::Enclosure &enclosure,
						  const CabinetStep::Model &model);
		int enclosurePlate() const {return m_enclosure_plate;}
		CabinetEnclosure::Enclosure enclosure() const {return m_enclosure;}
		const CabinetStep::Model &enclosureModel() const {return m_enclosure_model;}
		QMatrix4x4 enclosurePlacement() const {return m_enclosure_place;}
			/// Whether the enclosure's plate takes the folio's plate
		bool enclosurePlaced() const {return m_enclosure_placed;}
			/// "item:<uuid>" and "node:<id>" hidden
		void setHidden(const QStringList &hidden);
		QStringList hidden() const {return m_hidden;}
		bool isShown(int solid) const;
		static QString key(const Cabinet3d::Solid &solid);

		void setDuctCovers(bool covers);
		void fit();
		Pick selected() const {return m_selected;}
		void select(const Pick &pick);
			/// The solids drawn from their STEP model
		int withModel() const {return m_with_model;}
			/// The box solid @p index takes as drawn
		void drawnBox(int index, QVector3D &min, QVector3D &max) const;
			/// The box node @p index of the enclosure takes as drawn
		void nodeBox(int index, QVector3D &min, QVector3D &max) const;
			/// What is under @p point of the widget
		Pick pickAt(const QPoint &point) const;
			/// Models read since the solids were given: draw them
		void modelsChanged();
			/// The next click on a face says where a device goes
		void setPlacing(bool placing);
		bool isPlacing() const {return m_placing;}
			/// The face under @p point: where it is hit, the way it looks
			/// (towards the one looking) and what it belongs to
		bool faceAt(const QPoint &point, QVector3D &at, QVector3D &normal, Pick &pick) const;
			/// Where the model of the device placed in 3D @p solid stands,
			/// in the axes of the cabinet; @p ok false when it cannot be
		QMatrix4x4 mountedWorld(int solid, bool *ok = nullptr) const;

			/// A box drawn see-through over the cabinet: the free space a
			/// device asks, a problem of room (Brandon, 2026-11-21)
		struct Mark
		{
			QVector3D min;
			QVector3D max;
			QColor color;
		};
		void setMarks(const QList<Mark> &marks);
			/// The parts of the cabinet as they stand, for CabinetSpace:
			/// a device with its model read as its model, a device placed
			/// in 3D only once its model is read
		QList<CabinetSpace::Body> bodies() const;
			/// The room inside the enclosure, from its walls and its door
			/// as seen from the middle of the plate, the front of the box
			/// for an open one; false without an enclosure taking the
			/// plate
		bool inside(QVector3D &min, QVector3D &max) const;

		QSize sizeHint() const override {return QSize(900, 620);}

	signals:
		void picked(const Cabinet3dView::Pick &pick);
		void contextMenuRequested(const Cabinet3dView::Pick &pick, const QPoint &global);
		void faceClicked(const Cabinet3dView::Pick &pick, const QVector3D &at, const QVector3D &normal);
			/// A device placed in 3D dragged on its face to @p at, in the
			/// axes it is placed in
		void mountedMoved(int mounted, const QVector3D &at);

	protected:
		void initializeGL() override;
		void resizeGL(int width, int height) override;
		void paintGL() override;
		void mousePressEvent(QMouseEvent *event) override;
		void mouseMoveEvent(QMouseEvent *event) override;
		void mouseReleaseEvent(QMouseEvent *event) override;
		void mouseDoubleClickEvent(QMouseEvent *event) override;
		void wheelEvent(QWheelEvent *event) override;

	private:
		void build();
		void ray(const QPoint &point, QVector3D &origin, QVector3D &direction) const;
		QMatrix4x4 frameOf(const CabinetEnclosure::Mounted &mounted) const;
		QVector3D eye() const;
		QMatrix4x4 viewMatrix() const;
		QMatrix4x4 projectionMatrix() const;
		void upload(QOpenGLBuffer &buffer, const QVector<float> &data);
		void draw(QOpenGLBuffer &buffer, int count, GLenum mode, bool lit);

		QList<Cabinet3d::Solid> m_solids;
		QList<Mark> m_marks;
		QList<QPair<QVector3D, QVector3D>> m_boxes;
		QVector<float> m_opaque;
		QVector<float> m_clear;
		QVector<float> m_edges;
		int m_with_model = 0;
		bool m_duct_covers = true;
		bool m_dirty = true;
		bool m_fitted = false;
		QStringList m_hidden;

		int m_enclosure_plate = -1;
		CabinetEnclosure::Enclosure m_enclosure;
		CabinetStep::Model m_enclosure_model;
		QMatrix4x4 m_enclosure_place;
		bool m_enclosure_placed = false;
			/// Vertices of m_opaque of each part of the enclosure drawn
		QHash<int, QPair<int, int>> m_node_ranges;
		QVector<QPair<QVector3D, QVector3D>> m_node_boxes;

		std::unique_ptr<QOpenGLShaderProgram> m_program;
		QOpenGLBuffer m_opaque_buffer;
		QOpenGLBuffer m_clear_buffer;
		QOpenGLBuffer m_edge_buffer;
		QOpenGLBuffer m_selection_buffer;
		bool m_ready = false;

		QVector3D m_target;
		float m_distance = 1000;
		float m_yaw = -28;
		float m_pitch = 22;
		QPoint m_last;
		QPoint m_pressed_at;
		Qt::MouseButton m_button = Qt::NoButton;
		Pick m_selected;
		bool m_placing = false;
			/// A device placed in 3D being dragged on its face
		int m_moving = -1;
		QVector3D m_plane_at;
		QVector3D m_plane_normal;
};

/**
	@brief The Cabinet3dWindow class shows the cabinet of a cabinet folio
	in 3D (Brandon, 2026-11-20): a navigator of its parts and of the
	parts of its enclosure, each with an eye to hide it, the enclosure
	chosen from the parts database, a STEP file or measures, its mounting
	plate taken for the folio's, and the cabinet written as STEP. What it
	changes is kept in the folio's plate, one step of undo each.
*/
class Cabinet3dWindow : public QDialog
{
	Q_OBJECT

	public:
		explicit Cabinet3dWindow(Diagram *cabinet, QWidget *parent = nullptr);
		~Cabinet3dWindow() override;

		Cabinet3dView *view() const {return m_view;}
		QTreeWidget *navigator() const {return m_navigator;}
		bool exportStep(const QString &path, QString *error = nullptr) const;
			/// A box seen from above at an angle, in @p color
		static QIcon icon(const QColor &color = Qt::black);

			/// Give the folio's plate @p enclosure; false when the folio
			/// has no plate
		bool setEnclosure(const CabinetEnclosure::Enclosure &enclosure);
			/// Take node @p id of the enclosure for the mounting plate, the
			/// folio's plate given its size when @p adjust
		bool usePlate(const QString &id, bool adjust);
			/// Wait for a click on the part of the enclosure which is its
			/// mounting plate: the plate of the sheet takes its size
			/// (Brandon, 2026-11-25)
		void askForPlate();
		bool isAskingForPlate() const {return m_choosing_plate;}
		void setHiddenKey(const QString &key, bool hidden);
			/// Wait for the models read in the background
		void waitForModels();
			/// The devices to place in 3D (the "Unplaced 3D" of the
			/// navigator)
		QList<Cabinet3d::Unplaced> unplaced() const {return m_unplaced;}
			/// Place unplaced device @p index on the face of @p pick at
			/// @p at looking towards @p normal (axes of the cabinet), as a
			/// click does after "Place in 3D"
		bool placeOnFace(int index, const Cabinet3dView::Pick &pick, const QVector3D &at,
						 const QVector3D &normal);
			/// Change device placed in 3D @p mounted by @p change, one step
			/// of undo called @p text; remove it when @p change gives an
			/// empty symbol
		bool changeMounted(int mounted, const std::function<void(CabinetEnclosure::Mounted &)> &change,
						   const QString &text);

			/// Whether the cabinet has room for what is in it (Brandon,
			/// 2026-11-21): the problems listed under the navigator
		void checkRoom();
		QTreeWidget *problems() const {return m_problems;}
		QList<CabinetSpace::Problem> problemList() const {return m_problem_list;}
			/// The free space each device asks, in 3D and on the sheet
		void showClearances(bool shown);
		void showProblem(int index);

	protected:
		void reject() override;

	private:
		void reload();
		void loadModels();
		void applyEnclosure();
		void fillNavigator();
		void updateEyes();
		void showPick(const Cabinet3dView::Pick &pick);
		void showMenu(const Cabinet3dView::Pick &pick, const QPoint &global);
		void navigatorClicked(QTreeWidgetItem *item, int column);
		void changePlate(const CabinetItem *plate, const QString &enclosure, const QString &text,
						 qreal length = -1, qreal width = -1);
		CabinetItem *plateItem() const;
		void chooseFromParts();
		void chooseFile();
		void enterMeasures();
		void exportWithDialog();
		void startPlacing(int index);
		void updateMountedTools();
		void updateMarks();

		QPointer<Diagram> m_cabinet;
		QPointer<CabinetItem> m_plate;
		QString m_loaded_source;
		Cabinet3dView *m_view = nullptr;
		QTreeWidget *m_navigator = nullptr;
		QToolButton *m_enclosure_button = nullptr;
		QLabel *m_info = nullptr;
		QLabel *m_summary = nullptr;
		QProgressBar *m_progress = nullptr;
		QFutureWatcher<void> *m_loader = nullptr;
		bool m_filling = false;
		QList<Cabinet3d::Unplaced> m_unplaced;
		int m_placing = -1;
		bool m_choosing_plate = false;
		QWidget *m_mounted_tools = nullptr;
		QDoubleSpinBox *m_sink = nullptr;
		QCheckBox *m_clearances = nullptr;
		QTreeWidget *m_problems = nullptr;
		QList<CabinetSpace::Body> m_bodies;
		QList<CabinetSpace::Problem> m_problem_list;
		int m_problem_shown = -1;
};

#endif // CABINET3DVIEW_H
