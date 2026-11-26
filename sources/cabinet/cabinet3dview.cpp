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
#include "cabinet3dview.h"

#include "../diagram.h"
#include "../partsdb/partpickerdialog.h"
#include "../partsdb/partsdatabase.h"
#include "../qetproject.h"
#include "cabinetcommands.h"
#include "cabinetitem.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QSplitter>
#include <QSurfaceFormat>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtConcurrent/QtConcurrentRun>
#include <QtMath>
#include <QtOpenGL/QOpenGLShaderProgram>

#include <limits>

using CabinetEnclosure::Enclosure;
using Pick = Cabinet3dView::Pick;

namespace
{
	const float field_of_view = 35.0f;
	const QColor accent(0xFA, 0x13, 0x0C);

	const char *vertex_shader =
			"attribute vec3 position;\n"
			"attribute vec3 normal;\n"
			"attribute vec4 color;\n"
			"uniform mat4 mvp;\n"
			"varying vec3 v_normal;\n"
			"varying vec4 v_color;\n"
			"void main() {\n"
			"    v_normal = normal;\n"
			"    v_color = color;\n"
			"    gl_Position = mvp * vec4(position, 1.0);\n"
			"}\n";
	const char *fragment_shader =
			"#ifdef GL_ES\n"
			"precision mediump float;\n"
			"#endif\n"
			"varying vec3 v_normal;\n"
			"varying vec4 v_color;\n"
			"uniform vec3 light;\n"
			"uniform float lit;\n"
			"void main() {\n"
			"    float shade = 1.0;\n"
			"    if (lit > 0.5) {\n"
			"        vec3 n = normalize(v_normal);\n"
			"        shade = 0.52 + 0.48 * abs(dot(n, light));\n"
			"    }\n"
			"    gl_FragColor = vec4(v_color.rgb * shade, v_color.a);\n"
			"}\n";

	void vertex(QVector<float> &data, const QVector3D &p, const QColor &c)
	{
		data << p.x() << p.y() << p.z() << 0 << 0 << 0
			 << float(c.redF()) << float(c.greenF()) << float(c.blueF()) << 1;
	}

	/// The twelve edges of the box @p min to @p max
	void edges(QVector<float> &data, const QVector3D &min, const QVector3D &max, const QColor &color)
	{
		const QVector3D c[8] = {
			{min.x(), min.y(), min.z()}, {max.x(), min.y(), min.z()},
			{max.x(), max.y(), min.z()}, {min.x(), max.y(), min.z()},
			{min.x(), min.y(), max.z()}, {max.x(), min.y(), max.z()},
			{max.x(), max.y(), max.z()}, {min.x(), max.y(), max.z()}};
		const int lines[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
								  {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
		for (const auto &line : lines)
		{
			vertex(data, c[line[0]], color);
			vertex(data, c[line[1]], color);
		}
	}

	/// The vertices @p first to @p first + @p count of @p from, moved by
	/// @p place, added to @p to; their box into @p min and @p max
	void appendPlaced(QVector<float> &to, const QVector<float> &from, int first, int count,
					  const QMatrix4x4 &place, QVector3D &min, QVector3D &max)
	{
		bool started = false;
		const int size = CabinetStep::vertex_size;
		to.reserve(to.size() + count * size);
		for (int v = first ; v < first + count ; ++v)
		{
			const int i = v * size;
			const QVector3D p = place.map(QVector3D(from[i], from[i + 1], from[i + 2]));
			const QVector3D n = place.mapVector(QVector3D(from[i + 3], from[i + 4], from[i + 5]));
			to << p.x() << p.y() << p.z() << n.x() << n.y() << n.z()
			   << from[i + 6] << from[i + 7] << from[i + 8] << from[i + 9];
			if (!started) {
				min = max = p;
				started = true;
			} else {
				min = QVector3D(qMin(min.x(), p.x()), qMin(min.y(), p.y()), qMin(min.z(), p.z()));
				max = QVector3D(qMax(max.x(), p.x()), qMax(max.y(), p.y()), qMax(max.z(), p.z()));
			}
		}
	}

	/// The box @p min to @p max moved by @p place
	void placedBox(const QMatrix4x4 &place, const QVector3D &min, const QVector3D &max,
				   QVector3D &out_min, QVector3D &out_max)
	{
		for (int corner = 0 ; corner < 8 ; ++corner)
		{
			const QVector3D p = place.map(QVector3D(corner & 1 ? max.x() : min.x(),
													corner & 2 ? max.y() : min.y(),
													corner & 4 ? max.z() : min.z()));
			if (!corner) {
				out_min = out_max = p;
				continue;
			}
			out_min = QVector3D(qMin(out_min.x(), p.x()), qMin(out_min.y(), p.y()), qMin(out_min.z(), p.z()));
			out_max = QVector3D(qMax(out_max.x(), p.x()), qMax(out_max.y(), p.y()), qMax(out_max.z(), p.z()));
		}
	}

	/// Where the ray from @p origin along @p direction enters the box,
	/// -1 when it misses it
	float hitBox(const QVector3D &origin, const QVector3D &direction,
				 const QVector3D &min, const QVector3D &max)
	{
		float t_in = -std::numeric_limits<float>::max();
		float t_out = std::numeric_limits<float>::max();
		for (int axis = 0 ; axis < 3 ; ++axis)
		{
			const float d = direction[axis];
			if (qAbs(d) < 1e-9f) {
				if (origin[axis] < min[axis] || origin[axis] > max[axis]) return -1;
				continue;
			}
			float t1 = (min[axis] - origin[axis]) / d, t2 = (max[axis] - origin[axis]) / d;
			if (t1 > t2) std::swap(t1, t2);
			t_in = qMax(t_in, t1);
			t_out = qMin(t_out, t2);
			if (t_in > t_out) return -1;
		}
		if (t_out < 0) return -1;
		return qMax(0.0f, t_in);
	}

	/// Where the ray hits the triangle @p a @p b @p c, -1 when it misses
	float hitTriangle(const QVector3D &origin, const QVector3D &direction,
					  const QVector3D &a, const QVector3D &b, const QVector3D &c)
	{
		const QVector3D e1 = b - a, e2 = c - a;
		const QVector3D p = QVector3D::crossProduct(direction, e2);
		const float det = QVector3D::dotProduct(e1, p);
		if (qAbs(det) < 1e-9f) return -1;
		const float inverse = 1.0f / det;
		const QVector3D s = origin - a;
		const float u = QVector3D::dotProduct(s, p) * inverse;
		if (u < 0 || u > 1) return -1;
		const QVector3D q = QVector3D::crossProduct(s, e1);
		const float v = QVector3D::dotProduct(direction, q) * inverse;
		if (v < 0 || u + v > 1) return -1;
		const float t = QVector3D::dotProduct(e2, q) * inverse;
		return t >= 0 ? t : -1;
	}

	QString millimetres(const QVector3D &size)
	{
		const QLocale locale;
		return QStringLiteral("%1 × %2 × %3 mm")
				.arg(locale.toString(qRound(size.x())), locale.toString(qRound(size.y())),
					 locale.toString(qRound(size.z())));
	}

	/// An eye, open or shut, in @p color
	QIcon eyeIcon(bool open, const QColor &color)
	{
		QPixmap pixmap(32, 32);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		QPainterPath eye;
		eye.moveTo(4, 16);
		eye.quadTo(16, 4, 28, 16);
		eye.quadTo(16, 28, 4, 16);
		if (open) {
			painter.drawPath(eye);
			painter.setBrush(color);
			painter.drawEllipse(QPointF(16, 16), 4, 4);
		} else {
			QColor faint = color;
			faint.setAlpha(110);
			painter.setPen(QPen(faint, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
			painter.drawPath(eye);
			painter.drawLine(QPointF(6, 26), QPointF(26, 6));
		}
		painter.end();
		return QIcon(pixmap);
	}

	/// What makes the enclosure what it is: its source and its measures
	QString sourceKey(Enclosure enclosure)
	{
		enclosure.hidden.clear();
		enclosure.mounted.clear();
		enclosure.plate.clear();
		enclosure.quarter_turns = 0;
		return enclosure.toString();
	}

	enum Role {KeyRole = Qt::UserRole, SolidRole, NodeRole, UnplacedRole};

	QList<QUuid> symbolsOf(const Enclosure &enclosure)
	{
		QList<QUuid> symbols;
		for (const CabinetEnclosure::Mounted &each : enclosure.mounted) symbols << each.symbol;
		return symbols;
	}
}

Cabinet3dView::Cabinet3dView(QWidget *parent) :
	QOpenGLWidget(parent),
	m_opaque_buffer(QOpenGLBuffer::VertexBuffer),
	m_clear_buffer(QOpenGLBuffer::VertexBuffer),
	m_edge_buffer(QOpenGLBuffer::VertexBuffer),
	m_selection_buffer(QOpenGLBuffer::VertexBuffer)
{
	QSurfaceFormat format = QSurfaceFormat::defaultFormat();
	format.setDepthBufferSize(24);
	format.setSamples(4);
	setFormat(format);
	setFocusPolicy(Qt::StrongFocus);
	setMinimumSize(320, 240);
}

Cabinet3dView::~Cabinet3dView()
{
	if (!m_ready) return;
	makeCurrent();
	m_opaque_buffer.destroy();
	m_clear_buffer.destroy();
	m_edge_buffer.destroy();
	m_selection_buffer.destroy();
	m_program.reset();
	doneCurrent();
}

QString Cabinet3dView::key(const Cabinet3d::Solid &solid)
{
	return QStringLiteral("item:") + solid.item.toString(QUuid::WithoutBraces);
}

bool Cabinet3dView::isShown(int solid) const
{
	if (solid < 0 || solid >= m_solids.size()) return false;
	if (solid == m_enclosure_plate && m_enclosure_placed) return false;
	if (m_hidden.contains(key(m_solids.at(solid)))) return false;
	const int mounted = m_solids.at(solid).mounted;
	if (mounted >= 0) {
			//On the door: hidden with it
		if (mounted >= m_enclosure.mounted.size()) return false;
		const QString node = m_enclosure.mounted.at(mounted).node;
		if (node.isEmpty()) return true;
		const int index = m_enclosure_model.node(node);
		return m_enclosure_model.ok && index >= 0
				&& !CabinetEnclosure::isHidden(m_enclosure_model, index, m_hidden);
	}
	return true;
}

QMatrix4x4 Cabinet3dView::frameOf(const CabinetEnclosure::Mounted &mounted) const
{
	return mounted.node.isEmpty() ? QMatrix4x4() : m_enclosure_place;
}

QMatrix4x4 Cabinet3dView::mountedWorld(int solid, bool *ok) const
{
	if (ok) *ok = false;
	if (solid < 0 || solid >= m_solids.size()) return QMatrix4x4();
	const int mounted = m_solids.at(solid).mounted;
	if (mounted < 0 || mounted >= m_enclosure.mounted.size()) return QMatrix4x4();
	const CabinetEnclosure::Mounted &each = m_enclosure.mounted.at(mounted);
	if (!each.node.isEmpty() && !m_enclosure_model.ok) return QMatrix4x4();
	CabinetStep::Model model;
	if (!CabinetStep::cached(m_solids.at(solid).step, &model) || !model.ok) return QMatrix4x4();
	if (ok) *ok = true;
	return frameOf(each) * CabinetEnclosure::mountedPlacement(each, model);
}

void Cabinet3dView::setPlacing(bool placing)
{
	m_placing = placing;
	if (placing) {
		setCursor(Qt::CrossCursor);
	} else {
		unsetCursor();
	}
}

void Cabinet3dView::ray(const QPoint &point, QVector3D &origin, QVector3D &direction) const
{
	const QMatrix4x4 inverse = (projectionMatrix() * viewMatrix()).inverted();
	const float x = 2.0f * float(point.x()) / float(qMax(1, width())) - 1.0f;
	const float y = 1.0f - 2.0f * float(point.y()) / float(qMax(1, height()));
	origin = inverse.map(QVector3D(x, y, -1));
	direction = (inverse.map(QVector3D(x, y, 1)) - origin).normalized();
}

bool Cabinet3dView::faceAt(const QPoint &point, QVector3D &at, QVector3D &normal, Pick &pick) const
{
	if (width() <= 0 || height() <= 0) return false;
	QVector3D origin, direction;
	ray(point, origin, direction);
	float nearest = std::numeric_limits<float>::max();
	bool found = false;
		//The parts of the folio, but its devices: their faces
	for (int i = 0 ; i < m_boxes.size() ; ++i)
	{
		if (!isShown(i) || m_solids.at(i).kind == Cabinet3d::Solid::Kind::Device) continue;
		const QVector3D &min = m_boxes.at(i).first, &max = m_boxes.at(i).second;
		float t_in = -std::numeric_limits<float>::max();
		float t_out = std::numeric_limits<float>::max();
		int axis_in = -1;
		bool missed = false;
		for (int axis = 0 ; axis < 3 && !missed ; ++axis)
		{
			const float d = direction[axis];
			if (qAbs(d) < 1e-9f) {
				missed = origin[axis] < min[axis] || origin[axis] > max[axis];
				continue;
			}
			float t1 = (min[axis] - origin[axis]) / d, t2 = (max[axis] - origin[axis]) / d;
			if (t1 > t2) std::swap(t1, t2);
			if (t1 > t_in) {
				t_in = t1;
				axis_in = axis;
			}
			t_out = qMin(t_out, t2);
			missed = t_in > t_out;
		}
		if (missed || t_in < 0 || axis_in < 0 || t_in >= nearest) continue;
		nearest = t_in;
		at = origin + direction * t_in;
		normal = QVector3D();
		normal[axis_in] = direction[axis_in] > 0 ? -1.0f : 1.0f;
		pick = Pick();
		pick.solid = i;
		found = true;
	}
	for (auto it = m_node_ranges.constBegin() ; it != m_node_ranges.constEnd() ; ++it)
	{
		const auto &box = m_node_boxes.at(it.key());
		const float entry = hitBox(origin, direction, box.first, box.second);
		if (entry < 0 || entry >= nearest) continue;
		const int size = CabinetStep::vertex_size;
		for (int v = it.value().first ; v + 2 < it.value().first + it.value().second ; v += 3)
		{
			const float *p = m_opaque.constData() + v * size;
			const QVector3D a(p[0], p[1], p[2]), b(p[size], p[size + 1], p[size + 2]),
					c(p[2 * size], p[2 * size + 1], p[2 * size + 2]);
			const float t = hitTriangle(origin, direction, a, b, c);
			if (t < 0 || t >= nearest) continue;
			nearest = t;
			at = origin + direction * t;
			normal = QVector3D::normal(a, b, c);
			if (QVector3D::dotProduct(normal, direction) > 0) normal = -normal;
			pick = Pick();
			pick.node = it.key();
			found = true;
		}
	}
	return found;
}

void Cabinet3dView::setSolids(const QList<Cabinet3d::Solid> &solids)
{
	m_solids = solids;
	m_selected = Pick();
	m_enclosure_plate = -1;
	m_enclosure = Enclosure();
	m_enclosure_model = CabinetStep::Model();
	m_enclosure_placed = false;
	build();
	m_dirty = true;
	if (!m_fitted) {
		fit();
		m_fitted = true;
	}
	update();
}

void Cabinet3dView::setEnclosure(int plate, const Enclosure &enclosure, const CabinetStep::Model &model)
{
	const bool new_model = model.ok && !m_enclosure_model.ok;
	m_enclosure_plate = plate >= 0 && plate < m_solids.size() ? plate : -1;
	m_enclosure = enclosure;
	m_enclosure_model = m_enclosure_plate >= 0 ? model : CabinetStep::Model();
	m_enclosure_placed = false;
	m_enclosure_place = QMatrix4x4();
	m_node_boxes.clear();
	if (m_enclosure_model.ok) {
		const Cabinet3d::Solid &solid = m_solids.at(m_enclosure_plate);
		bool ok = false;
		if (enclosure.placed())
			m_enclosure_place = CabinetEnclosure::placement(m_enclosure_model, enclosure, solid, &ok);
		m_enclosure_placed = ok;
		if (!ok) {
				//Beside the plate, until its own plate is chosen
			const CabinetStep::Model &model_ref = m_enclosure_model;
			m_enclosure_place = QMatrix4x4();
			m_enclosure_place.translate(solid.max.x() + 200 - model_ref.min.x(),
										solid.center().y() - (model_ref.min.y() + model_ref.max.y()) / 2,
										solid.min.z() - model_ref.min.z());
		}
		m_node_boxes.resize(m_enclosure_model.nodes.size());
		for (int i = 0 ; i < m_enclosure_model.nodes.size() ; ++i)
		{
			const CabinetStep::Node &node = m_enclosure_model.nodes.at(i);
			if (node.count > 0)
				placedBox(m_enclosure_place, node.min, node.max, m_node_boxes[i].first, m_node_boxes[i].second);
		}
	}
	if (m_selected.node >= m_node_boxes.size()) m_selected = Pick();
	build();
	m_dirty = true;
	if (new_model) fit();
	update();
}

void Cabinet3dView::setHidden(const QStringList &hidden)
{
	if (m_hidden == hidden) return;
	m_hidden = hidden;
	build();
	m_dirty = true;
	update();
}

void Cabinet3dView::setDuctCovers(bool covers)
{
	if (m_duct_covers == covers) return;
	m_duct_covers = covers;
	build();
	m_dirty = true;
	update();
}

void Cabinet3dView::setMarks(const QList<Mark> &marks)
{
	m_marks = marks;
	build();
	m_dirty = true;
	update();
}

QList<CabinetSpace::Body> Cabinet3dView::bodies() const
{
	QList<CabinetSpace::Body> result;
	for (int i = 0 ; i < m_solids.size() ; ++i)
	{
		const Cabinet3d::Solid &solid = m_solids.at(i);
		CabinetSpace::Body body;
		body.solid = i;
		body.kind = solid.kind;
		body.name = solid.name;
		body.clearance = solid.clearance;
		body.min = solid.min;
		body.max = solid.max;
		CabinetStep::Model model;
		if (solid.mounted >= 0) {
			bool ok = false;
			const QMatrix4x4 world = mountedWorld(i, &ok);
			if (!ok || !CabinetStep::cached(solid.step, &model) || !model.ok) continue;
			placedBox(world, model.min, model.max, body.min, body.max);
			body.mounted = true;
		} else if (solid.kind == Cabinet3d::Solid::Kind::Device && !solid.step.isEmpty()
				   && CabinetStep::cached(solid.step, &model) && model.ok) {
			placedBox(CabinetStep::placement(model, solid), model.min, model.max, body.min, body.max);
		}
		result << body;
	}
	return result;
}

bool Cabinet3dView::inside(QVector3D &min, QVector3D &max) const
{
	if (!m_enclosure_model.ok || !m_enclosure_placed || m_enclosure_plate < 0
		|| m_enclosure_plate >= m_solids.size()) {
		return false;
	}
	const Cabinet3d::Solid &plate = m_solids.at(m_enclosure_plate);
		//Every part of the enclosure but its plate, hidden or not: a door
		//hidden to look inside still closes
	const int plate_node = m_enclosure_model.node(m_enclosure.plate);
	auto ofPlate = [this, plate_node](int n) {
		for (int at = n ; at >= 0 ; at = m_enclosure_model.nodes.at(at).parent)
			if (at == plate_node) return true;
		return false;
	};
	auto nearest = [&](const QVector3D &origin, const QVector3D &direction) {
		float best = -1;
		for (int n = 0 ; n < m_enclosure_model.nodes.size() ; ++n)
		{
			const CabinetStep::Node &node = m_enclosure_model.nodes.at(n);
			if (node.assembly || node.count <= 0 || ofPlate(n)) continue;
			const float t = CabinetSpace::hit(m_enclosure_model.vertices, node.first, node.count,
											  m_enclosure_place, origin, direction);
			if (t >= 0 && (best < 0 || t < best)) best = t;
		}
		return best;
	};
	QVector3D model_min, model_max;
	placedBox(m_enclosure_place, m_enclosure_model.min, m_enclosure_model.max, model_min, model_max);
	const QVector3D centre(plate.center().x(), plate.center().y(), plate.max.z() + 1.0f);
	const float front = nearest(centre, QVector3D(0, 0, 1));
	const float door = front >= 0 ? centre.z() + front : model_max.z();
		//The walls at the height of the devices
	const QVector3D middle(centre.x(), centre.y(), plate.max.z() + qMin(30.0f, (door - plate.max.z()) / 2));
	const float right = nearest(middle, QVector3D(1, 0, 0));
	const float left = nearest(middle, QVector3D(-1, 0, 0));
	const float up = nearest(middle, QVector3D(0, 1, 0));
	const float down = nearest(middle, QVector3D(0, -1, 0));
	min = QVector3D(left >= 0 ? middle.x() - left : model_min.x(),
					down >= 0 ? middle.y() - down : model_min.y(), model_min.z());
	max = QVector3D(right >= 0 ? middle.x() + right : model_max.x(),
					up >= 0 ? middle.y() + up : model_max.y(), door);
	return true;
}

void Cabinet3dView::modelsChanged()
{
	build();
	m_dirty = true;
	update();
}

void Cabinet3dView::select(const Pick &pick)
{
	Pick checked = pick;
	if (checked.solid >= m_solids.size()) checked.solid = -1;
	if (checked.node >= m_node_boxes.size()) checked.node = -1;
	m_selected = checked;
	update();
	emit picked(m_selected);
}

void Cabinet3dView::drawnBox(int index, QVector3D &min, QVector3D &max) const
{
	if (index < 0 || index >= m_boxes.size()) {
		min = max = QVector3D();
		return;
	}
	min = m_boxes.at(index).first;
	max = m_boxes.at(index).second;
}

void Cabinet3dView::nodeBox(int index, QVector3D &min, QVector3D &max) const
{
	if (index < 0 || index >= m_node_boxes.size()) {
		min = max = QVector3D();
		return;
	}
	min = m_node_boxes.at(index).first;
	max = m_node_boxes.at(index).second;
}

void Cabinet3dView::build()
{
	m_opaque.clear();
	m_clear.clear();
	m_edges.clear();
	m_boxes.clear();
	m_node_ranges.clear();
	m_with_model = 0;
	for (int i = 0 ; i < m_solids.size() ; ++i)
	{
		const Cabinet3d::Solid &solid = m_solids.at(i);
		m_boxes << qMakePair(solid.min, solid.max);
		if (solid.mounted >= 0) {
				//On its face; a small box until its model is read
			if (!isShown(i)) continue;
			const CabinetEnclosure::Mounted &each = m_enclosure.mounted.at(solid.mounted);
			const QMatrix4x4 frame = frameOf(each);
			CabinetStep::Model model;
			if (!solid.step.isEmpty() && CabinetStep::cached(solid.step, &model) && model.ok) {
				++m_with_model;
				QVector3D min, max;
				appendPlaced(m_opaque, model.vertices, 0, int(model.vertices.size() / CabinetStep::vertex_size),
							 frame * CabinetEnclosure::mountedPlacement(each, model), min, max);
				m_boxes[i] = qMakePair(min, max);
			} else {
				const QVector3D at = frame.map(each.at);
				const QVector3D middle = at + frame.mapVector(each.normal).normalized() * 10;
				const QVector3D half(10, 10, 10);
				CabinetStep::appendBox(m_opaque, middle - half, middle + half, solid.color);
				edges(m_edges, middle - half, middle + half, solid.color.darker(170));
				m_boxes[i] = qMakePair(middle - half, middle + half);
			}
			continue;
		}
		if (solid.kind == Cabinet3d::Solid::Kind::Device && !solid.step.isEmpty()) {
			CabinetStep::Model model;
				//Read in the background: a box until then
			if (CabinetStep::cached(solid.step, &model) && model.ok) {
				++m_with_model;
				if (!isShown(i)) continue;
				QVector3D min, max;
				appendPlaced(m_opaque, model.vertices, 0, int(model.vertices.size() / CabinetStep::vertex_size),
							 CabinetStep::placement(model, solid), min, max);
				m_boxes[i] = qMakePair(min, max);
				continue;
			}
		}
		if (!isShown(i)) continue;
		if (solid.kind == Cabinet3d::Solid::Kind::Rail || solid.kind == Cabinet3d::Solid::Kind::Duct) {
				//As they are: the profile of the rail, the slots of the duct
			for (const Cabinet3d::Piece &piece : Cabinet3d::piecesOf(solid, m_duct_covers))
				CabinetStep::appendBox(m_opaque, piece.min, piece.max, piece.color);
			continue;
		}
		CabinetStep::appendBox(m_opaque, solid.min, solid.max, solid.color);
		edges(m_edges, solid.min, solid.max, solid.color.darker(170));
	}

		//The free spaces and the problems, seen through
	for (const Mark &mark : std::as_const(m_marks))
	{
		CabinetStep::appendBox(m_clear, mark.min, mark.max, mark.color, float(mark.color.alphaF()));
		QColor edge = mark.color.darker(140);
		edge.setAlpha(255);
		edges(m_edges, mark.min, mark.max, edge);
	}

	if (m_enclosure_model.ok) {
		for (int n = 0 ; n < m_enclosure_model.nodes.size() ; ++n)
		{
			const CabinetStep::Node &node = m_enclosure_model.nodes.at(n);
			if (node.assembly || node.count <= 0) continue;
			if (CabinetEnclosure::isHidden(m_enclosure_model, n, m_hidden)) continue;
			const int first = int(m_opaque.size() / CabinetStep::vertex_size);
			QVector3D min, max;
			appendPlaced(m_opaque, m_enclosure_model.vertices, node.first, node.count,
						 m_enclosure_place, min, max);
			m_node_ranges.insert(n, qMakePair(first, node.count));
		}
	}
}

void Cabinet3dView::fit()
{
	bool started = false;
	QVector3D min, max;
	auto add = [&](const QVector3D &a, const QVector3D &b) {
		if (!started) {
			min = a;
			max = b;
			started = true;
			return;
		}
		min = QVector3D(qMin(min.x(), a.x()), qMin(min.y(), a.y()), qMin(min.z(), a.z()));
		max = QVector3D(qMax(max.x(), b.x()), qMax(max.y(), b.y()), qMax(max.z(), b.z()));
	};
	for (const auto &box : std::as_const(m_boxes)) add(box.first, box.second);
	if (m_enclosure_model.ok && !m_node_boxes.isEmpty()) {
		QVector3D a, b;
		placedBox(m_enclosure_place, m_enclosure_model.min, m_enclosure_model.max, a, b);
		add(a, b);
	}
	if (!started) {
		m_target = QVector3D();
		m_distance = 1000;
		update();
		return;
	}
	m_target = (min + max) / 2;
	const float radius = qMax(1.0f, (max - min).length() / 2);
	m_distance = radius / float(qTan(qDegreesToRadians(field_of_view / 2))) * 1.08f;
	update();
}

QVector3D Cabinet3dView::eye() const
{
	const float yaw = qDegreesToRadians(m_yaw);
	const float pitch = qDegreesToRadians(m_pitch);
	return m_target + m_distance * QVector3D(qCos(pitch) * qSin(yaw), qSin(pitch), qCos(pitch) * qCos(yaw));
}

QMatrix4x4 Cabinet3dView::viewMatrix() const
{
	QMatrix4x4 view;
	view.lookAt(eye(), m_target, QVector3D(0, 1, 0));
	return view;
}

QMatrix4x4 Cabinet3dView::projectionMatrix() const
{
	QMatrix4x4 projection;
	const float aspect = height() > 0 ? float(width()) / float(height()) : 1.0f;
	projection.perspective(field_of_view, aspect, qMax(1.0f, m_distance * 0.01f), m_distance * 20 + 5000);
	return projection;
}

void Cabinet3dView::initializeGL()
{
	initializeOpenGLFunctions();
	m_program = std::make_unique<QOpenGLShaderProgram>();
	m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertex_shader);
	m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment_shader);
	m_program->bindAttributeLocation("position", 0);
	m_program->bindAttributeLocation("normal", 1);
	m_program->bindAttributeLocation("color", 2);
	m_ready = m_program->link();
	for (QOpenGLBuffer *buffer : {&m_opaque_buffer, &m_clear_buffer, &m_edge_buffer, &m_selection_buffer})
		buffer->create();
	m_dirty = true;
}

void Cabinet3dView::resizeGL(int width, int height)
{
	Q_UNUSED(width)
	Q_UNUSED(height)
}

void Cabinet3dView::upload(QOpenGLBuffer &buffer, const QVector<float> &data)
{
	buffer.bind();
	buffer.allocate(data.constData(), int(data.size() * sizeof(float)));
	buffer.release();
}

void Cabinet3dView::draw(QOpenGLBuffer &buffer, int count, GLenum mode, bool lit)
{
	if (count <= 0) return;
	const int stride = CabinetStep::vertex_size * int(sizeof(float));
	buffer.bind();
	m_program->enableAttributeArray(0);
	m_program->enableAttributeArray(1);
	m_program->enableAttributeArray(2);
	m_program->setAttributeBuffer(0, GL_FLOAT, 0, 3, stride);
	m_program->setAttributeBuffer(1, GL_FLOAT, 3 * sizeof(float), 3, stride);
	m_program->setAttributeBuffer(2, GL_FLOAT, 6 * sizeof(float), 4, stride);
	m_program->setUniformValue("lit", lit ? 1.0f : 0.0f);
	glDrawArrays(mode, 0, count);
	m_program->disableAttributeArray(0);
	m_program->disableAttributeArray(1);
	m_program->disableAttributeArray(2);
	buffer.release();
}

void Cabinet3dView::paintGL()
{
	glClearColor(0.93f, 0.94f, 0.95f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	if (!m_ready) return;
	if (m_dirty) {
		upload(m_opaque_buffer, m_opaque);
		upload(m_clear_buffer, m_clear);
		upload(m_edge_buffer, m_edges);
		m_dirty = false;
	}

	const QMatrix4x4 mvp = projectionMatrix() * viewMatrix();
		//A light over the left shoulder of the one looking
	const QVector3D forward = (m_target - eye()).normalized();
	const QVector3D right = QVector3D::crossProduct(forward, QVector3D(0, 1, 0)).normalized();
	const QVector3D light = (-forward + QVector3D(0, 0.35f, 0) - right * 0.25f).normalized();

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDisable(GL_CULL_FACE);
	m_program->bind();
	m_program->setUniformValue("mvp", mvp);
	m_program->setUniformValue("light", light);
	draw(m_opaque_buffer, int(m_opaque.size() / CabinetStep::vertex_size), GL_TRIANGLES, true);
	draw(m_edge_buffer, int(m_edges.size() / CabinetStep::vertex_size), GL_LINES, false);
	if (!m_clear.isEmpty()) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE);
		draw(m_clear_buffer, int(m_clear.size() / CabinetStep::vertex_size), GL_TRIANGLES, true);
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
	}
	QVector3D min, max;
	bool highlight = false;
	if (m_selected.solid >= 0 && m_selected.solid < m_boxes.size() && isShown(m_selected.solid)) {
		drawnBox(m_selected.solid, min, max);
		highlight = true;
	} else if (m_selected.node >= 0 && m_selected.node < m_node_boxes.size()) {
		nodeBox(m_selected.node, min, max);
		highlight = min != max;
	}
	if (highlight) {
		QVector<float> selection;
		edges(selection, min - QVector3D(1, 1, 1), max + QVector3D(1, 1, 1), accent);
		upload(m_selection_buffer, selection);
		glDisable(GL_DEPTH_TEST);
		draw(m_selection_buffer, int(selection.size() / CabinetStep::vertex_size), GL_LINES, false);
		glEnable(GL_DEPTH_TEST);
	}
	m_program->release();
	glDisable(GL_DEPTH_TEST);

		//The labels of the devices, on their front
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	QFont font = this->font();
	font.setBold(true);
	painter.setFont(font);
	for (int i = 0 ; i < m_solids.size() && i < m_boxes.size() ; ++i)
	{
		const Cabinet3d::Solid &solid = m_solids.at(i);
		if (solid.kind != Cabinet3d::Solid::Kind::Device || solid.name.isEmpty() || !isShown(i)) continue;
		const QVector3D front((m_boxes.at(i).first.x() + m_boxes.at(i).second.x()) / 2,
							  (m_boxes.at(i).first.y() + m_boxes.at(i).second.y()) / 2,
							  m_boxes.at(i).second.z());
		const QVector4D clip = mvp * QVector4D(front, 1);
		if (clip.w() <= 0) continue;
		const QPointF at((clip.x() / clip.w() + 1) / 2 * width(), (1 - clip.y() / clip.w()) / 2 * height());
		QPainterPath text;
		text.addText(0, 0, font, solid.name);
		const QRectF bounds = text.boundingRect();
		text.translate(at.x() - bounds.center().x(), at.y() - bounds.center().y());
		painter.strokePath(text, QPen(QColor(255, 255, 255, 220), 3));
		painter.fillPath(text, i == m_selected.solid ? accent : QColor(30, 30, 30));
	}
}

Pick Cabinet3dView::pickAt(const QPoint &point) const
{
	Pick found;
	if (width() <= 0 || height() <= 0) return found;
	const QMatrix4x4 inverse = (projectionMatrix() * viewMatrix()).inverted();
	const float x = 2.0f * float(point.x()) / float(width()) - 1.0f;
	const float y = 1.0f - 2.0f * float(point.y()) / float(height());
	const QVector3D origin = inverse.map(QVector3D(x, y, -1));
	const QVector3D direction = (inverse.map(QVector3D(x, y, 1)) - origin).normalized();

	float nearest = std::numeric_limits<float>::max();
	for (int i = 0 ; i < m_boxes.size() ; ++i)
	{
		if (!isShown(i)) continue;
		const float t = hitBox(origin, direction, m_boxes.at(i).first, m_boxes.at(i).second);
		if (t < 0) continue;
			//A plate behind the rest only when nothing else is there
		const float weight = m_solids.at(i).kind == Cabinet3d::Solid::Kind::Plate ? t + 1e6f : t;
		if (weight < nearest) {
			nearest = weight;
			found = Pick();
			found.solid = i;
		}
	}
		//The parts of the enclosure, by their triangles: its plate, not
		//the box holding everything
	for (auto it = m_node_ranges.constBegin() ; it != m_node_ranges.constEnd() ; ++it)
	{
		const auto &box = m_node_boxes.at(it.key());
		const float entry = hitBox(origin, direction, box.first, box.second);
		if (entry < 0 || entry >= nearest) continue;
		const int size = CabinetStep::vertex_size;
		for (int v = it.value().first ; v + 2 < it.value().first + it.value().second ; v += 3)
		{
			const float *p = m_opaque.constData() + v * size;
			const float t = hitTriangle(origin, direction, QVector3D(p[0], p[1], p[2]),
										QVector3D(p[size], p[size + 1], p[size + 2]),
										QVector3D(p[2 * size], p[2 * size + 1], p[2 * size + 2]));
			if (t >= 0 && t < nearest) {
				nearest = t;
				found = Pick();
				found.node = it.key();
			}
		}
	}
	return found;
}

void Cabinet3dView::mousePressEvent(QMouseEvent *event)
{
	m_last = m_pressed_at = event->position().toPoint();
	m_button = event->button();
		//A device placed in 3D, picked, dragged on its face
	const int selected = m_selected.solid;
	if (m_button == Qt::LeftButton && !m_placing && selected >= 0 && selected < m_solids.size()
		&& m_solids.at(selected).mounted >= 0 && pickAt(m_pressed_at).solid == selected) {
		const CabinetEnclosure::Mounted &each = m_enclosure.mounted.at(m_solids.at(selected).mounted);
		const QMatrix4x4 frame = frameOf(each);
		m_moving = selected;
		m_plane_at = frame.map(each.at);
		m_plane_normal = frame.mapVector(each.normal).normalized();
		setCursor(Qt::SizeAllCursor);
	}
}

void Cabinet3dView::mouseMoveEvent(QMouseEvent *event)
{
	const QPoint point = event->position().toPoint();
	const QPoint delta = point - m_last;
	m_last = point;
	if (m_moving >= 0) {
		QVector3D origin, direction;
		ray(point, origin, direction);
		const float across = QVector3D::dotProduct(direction, m_plane_normal);
		if (qAbs(across) < 1e-6f) return;
		const float t = QVector3D::dotProduct(m_plane_at - origin, m_plane_normal) / across;
		if (t < 0) return;
		CabinetEnclosure::Mounted &each = m_enclosure.mounted[m_solids.at(m_moving).mounted];
		each.at = frameOf(each).inverted().map(origin + direction * t);
		build();
		m_dirty = true;
		update();
		return;
	}
	if (m_button == Qt::LeftButton) {
		m_yaw -= delta.x() * 0.4f;
		m_pitch = qBound(-89.0f, m_pitch + delta.y() * 0.4f, 89.0f);
		update();
	} else if (m_button == Qt::RightButton || m_button == Qt::MiddleButton) {
		const QVector3D forward = (m_target - eye()).normalized();
		const QVector3D right = QVector3D::crossProduct(forward, QVector3D(0, 1, 0)).normalized();
		const QVector3D up = QVector3D::crossProduct(right, forward).normalized();
		const float per_pixel = 2 * m_distance * float(qTan(qDegreesToRadians(field_of_view / 2)))
				/ float(qMax(1, height()));
		m_target += (-right * float(delta.x()) + up * float(delta.y())) * per_pixel;
		update();
	}
}

void Cabinet3dView::mouseReleaseEvent(QMouseEvent *event)
{
	const QPoint point = event->position().toPoint();
	const bool clicked = (point - m_pressed_at).manhattanLength() < 4;
	if (m_moving >= 0) {
		const int mounted = m_solids.at(m_moving).mounted;
		m_moving = -1;
		unsetCursor();
		m_button = Qt::NoButton;
		if (!clicked) {
			emit mountedMoved(mounted, m_enclosure.mounted.at(mounted).at);
			return;
		}
	}
	if (clicked && m_button == Qt::LeftButton && m_placing) {
		QVector3D at, normal;
		Pick pick;
		if (faceAt(point, at, normal, pick)) {
			setPlacing(false);
			emit faceClicked(pick, at, normal);
		}
	} else if (clicked && m_button == Qt::LeftButton) {
		select(pickAt(point));
	} else if (clicked && m_button == Qt::RightButton) {
		const Pick pick = pickAt(point);
		select(pick);
		emit contextMenuRequested(pick, mapToGlobal(point));
	}
	m_button = Qt::NoButton;
}

void Cabinet3dView::mouseDoubleClickEvent(QMouseEvent *event)
{
	Q_UNUSED(event)
	fit();
}

void Cabinet3dView::wheelEvent(QWheelEvent *event)
{
	const float steps = float(event->angleDelta().y()) / 120.0f;
	m_distance = qBound(10.0f, m_distance * float(qPow(0.88, steps)), 100000.0f);
	update();
}

Cabinet3dWindow::Cabinet3dWindow(Diagram *cabinet, QWidget *parent) :
	QDialog(parent),
	m_cabinet(cabinet)
{
	setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
	const QString title = cabinet ? cabinet->title().trimmed() : QString();
	setWindowTitle(title.isEmpty() ? tr("Cabinet in 3D") : tr("Cabinet in 3D: %1").arg(title));
	resize(1280, 800);

	auto *layout = new QVBoxLayout(this);
	auto *tools = new QHBoxLayout();
	auto *fit = new QPushButton(tr("Show everything"), this);
	auto *covers = new QCheckBox(tr("Duct covers"), this);
	covers->setObjectName(QStringLiteral("duct_covers"));
	covers->setChecked(true);
	m_enclosure_button = new QToolButton(this);
	m_enclosure_button->setText(tr("Enclosure"));
	m_enclosure_button->setPopupMode(QToolButton::InstantPopup);
	m_enclosure_button->setToolButtonStyle(Qt::ToolButtonTextOnly);
	auto *enclosure_menu = new QMenu(m_enclosure_button);
	enclosure_menu->addAction(tr("From the parts database…"), this, &Cabinet3dWindow::chooseFromParts);
	enclosure_menu->addAction(tr("STEP file from the disk…"), this, &Cabinet3dWindow::chooseFile);
	enclosure_menu->addAction(tr("Measures by hand…"), this, &Cabinet3dWindow::enterMeasures);
	enclosure_menu->addSeparator();
	enclosure_menu->addAction(tr("Turn it 90° about its plate"), this, [this]() {
		if (!m_plate) return;
		Enclosure enclosure = Enclosure::fromString(m_plate->properties().enclosure);
		if (!enclosure.hasModel()) return;
		enclosure.quarter_turns = (enclosure.quarter_turns + 1) % 4;
		changePlate(m_plate, enclosure.toString(), tr("Turn the enclosure"));
	});
	enclosure_menu->addAction(tr("No enclosure"), this, [this]() {
		if (!m_plate) return;
		Enclosure none;
		for (const QString &key : Enclosure::fromString(m_plate->properties().enclosure).hidden)
			if (key.startsWith(QLatin1String("item:"))) none.hidden << key;
		changePlate(m_plate, none.toString(), tr("Take the enclosure away"));
	});
	m_enclosure_button->setMenu(enclosure_menu);
	auto *export_step = new QPushButton(tr("Export as STEP…"), this);
	export_step->setObjectName(QStringLiteral("export_step"));
	export_step->setEnabled(CabinetStep::available());
		//Room enough for every part (Brandon, 2026-11-21)
	m_clearances = new QCheckBox(tr("Mounting clearances"), this);
	m_clearances->setObjectName(QStringLiteral("mounting_clearances"));
	m_clearances->setToolTip(tr("The free space the part of each device asks around it in the"
								" parts database, here and on the sheet"));
	m_clearances->setChecked(CabinetItem::clearancesShown());
	auto *check_room = new QPushButton(tr("Check the room"), this);
	check_room->setObjectName(QStringLiteral("check_room"));
	check_room->setToolTip(tr("Parts in each other, parts in the free space a device asks, devices"
							  " through the enclosure or keeping its door from closing"));
	tools->addWidget(fit);
	tools->addWidget(covers);
	tools->addWidget(m_enclosure_button);
	tools->addWidget(export_step);
	tools->addWidget(m_clearances);
	tools->addWidget(check_room);
	tools->addStretch();
	m_progress = new QProgressBar(this);
	m_progress->setMaximumWidth(220);
	m_progress->setFormat(tr("Reading STEP models… %v/%m"));
	m_progress->hide();
	tools->addWidget(m_progress);
	m_summary = new QLabel(this);
	tools->addWidget(m_summary);
	layout->addLayout(tools);

	auto *splitter = new QSplitter(this);
	auto *left = new QSplitter(Qt::Vertical, splitter);
	m_navigator = new QTreeWidget(left);
	m_navigator->setObjectName(QStringLiteral("navigator"));
	m_navigator->setColumnCount(2);
	m_navigator->setHeaderLabels({tr("Navigator"), QString()});
	m_navigator->header()->setStretchLastSection(false);
	m_navigator->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	m_navigator->header()->setSectionResizeMode(1, QHeaderView::Fixed);
	m_navigator->header()->resizeSection(1, 30);
	m_navigator->setIconSize(QSize(18, 18));
	m_navigator->setContextMenuPolicy(Qt::CustomContextMenu);
		//The problems of room, under the navigator once checked
	m_problems = new QTreeWidget(left);
	m_problems->setObjectName(QStringLiteral("room_problems"));
	m_problems->setHeaderLabels({tr("Room")});
	m_problems->setRootIsDecorated(false);
	m_problems->setWordWrap(true);
	m_problems->hide();
	left->addWidget(m_navigator);
	left->addWidget(m_problems);
	left->setStretchFactor(0, 3);
	left->setStretchFactor(1, 2);
	m_view = new Cabinet3dView(splitter);
	splitter->addWidget(left);
	splitter->addWidget(m_view);
	splitter->setStretchFactor(1, 1);
	splitter->setSizes({300, 980});
	layout->addWidget(splitter, 1);
	m_info = new QLabel(tr("Left button: turn. Right button: move, or a click for what to do with a part. "
						   "Wheel: nearer. Double click: everything."), this);
	m_info->setWordWrap(true);
	m_info->setTextInteractionFlags(Qt::TextSelectableByMouse);
	layout->addWidget(m_info);

		//A device placed in 3D, picked: turn it, its other side out, sink
		//it into the sheet, take it away
	m_mounted_tools = new QWidget(this);
	auto *mounted_layout = new QHBoxLayout(m_mounted_tools);
	mounted_layout->setContentsMargins(0, 0, 0, 0);
	auto *turn = new QPushButton(tr("Turn 90°"), m_mounted_tools);
	auto *side = new QPushButton(tr("Another side out"), m_mounted_tools);
	side->setToolTip(tr("For a model drawn with other axes: which of its sides looks out of the face"));
	m_sink = new QDoubleSpinBox(m_mounted_tools);
	m_sink->setRange(-500, 500);
	m_sink->setDecimals(1);
	m_sink->setSuffix(QStringLiteral(" mm"));
	m_sink->setToolTip(tr("How far it goes into the sheet: a push button through the door"));
	auto *remove = new QPushButton(tr("Take it out of 3D"), m_mounted_tools);
	mounted_layout->addWidget(turn);
	mounted_layout->addWidget(side);
	mounted_layout->addWidget(new QLabel(tr("Sink:"), m_mounted_tools));
	mounted_layout->addWidget(m_sink);
	mounted_layout->addWidget(remove);
	mounted_layout->addStretch();
	m_mounted_tools->hide();
	layout->addWidget(m_mounted_tools);
	auto selected_mounted = [this]() {
		const Pick pick = m_view->selected();
		return pick.solid >= 0 && pick.solid < m_view->solids().size()
				? m_view->solids().at(pick.solid).mounted : -1;
	};
	connect(turn, &QPushButton::clicked, this, [this, selected_mounted]() {
		changeMounted(selected_mounted(), [](CabinetEnclosure::Mounted &each) {
			each.quarter_turns = (each.quarter_turns + 1) % 4;
		}, tr("Turn %1"));
	});
	connect(side, &QPushButton::clicked, this, [this, selected_mounted]() {
		changeMounted(selected_mounted(), [](CabinetEnclosure::Mounted &each) {
			each.side = (each.side + 1) % 6;
		}, tr("Turn %1"));
	});
	connect(m_sink, &QDoubleSpinBox::editingFinished, this, [this, selected_mounted]() {
		const double sink = m_sink->value();
		changeMounted(selected_mounted(), [sink](CabinetEnclosure::Mounted &each) {
			each.sink = sink;
		}, tr("Sink %1"));
	});
	connect(remove, &QPushButton::clicked, this, [this, selected_mounted]() {
		changeMounted(selected_mounted(), [](CabinetEnclosure::Mounted &each) {
			each.symbol = QUuid();
		}, tr("Take %1 out of 3D"));
	});

	m_loader = new QFutureWatcher<void>(this);
	connect(m_loader, &QFutureWatcher<void>::finished, this, [this]() {
		m_progress->hide();
		m_view->modelsChanged();
		applyEnclosure();
		fillNavigator();
			//The models stand as they are: the free spaces with them
		updateMarks();
	});
	connect(fit, &QPushButton::clicked, m_view, &Cabinet3dView::fit);
	connect(m_clearances, &QCheckBox::toggled, this, &Cabinet3dWindow::showClearances);
	connect(check_room, &QPushButton::clicked, this, &Cabinet3dWindow::checkRoom);
	connect(m_problems, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) {
		showProblem(item ? item->data(0, Qt::UserRole).toInt() : -1);
	});
	connect(covers, &QCheckBox::toggled, m_view, &Cabinet3dView::setDuctCovers);
	connect(export_step, &QPushButton::clicked, this, &Cabinet3dWindow::exportWithDialog);
	connect(m_view, &Cabinet3dView::picked, this, &Cabinet3dWindow::showPick);
	connect(m_view, &Cabinet3dView::contextMenuRequested, this, &Cabinet3dWindow::showMenu);
	connect(m_view, &Cabinet3dView::faceClicked, this,
			[this](const Pick &pick, const QVector3D &at, const QVector3D &normal) {
		const int index = m_placing;
		m_placing = -1;
		placeOnFace(index, pick, at, normal);
	});
	connect(m_view, &Cabinet3dView::mountedMoved, this, [this](int mounted, const QVector3D &at) {
		changeMounted(mounted, [at](CabinetEnclosure::Mounted &each) {each.at = at;}, tr("Move %1"));
	});
	connect(m_navigator, &QTreeWidget::itemClicked, this, &Cabinet3dWindow::navigatorClicked);
	connect(m_navigator, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) {
		const QVariant unplaced = item ? item->data(0, UnplacedRole) : QVariant();
		if (unplaced.isValid()) startPlacing(unplaced.toInt());
	});
	connect(m_navigator, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint &point) {
		QTreeWidgetItem *item = m_navigator->itemAt(point);
		if (!item) return;
		const QVariant unplaced = item->data(0, UnplacedRole);
		if (unplaced.isValid()) {
			QMenu menu(this);
			const int index = unplaced.toInt();
			menu.addAction(tr("Place in 3D"), this, [this, index]() {startPlacing(index);});
			menu.exec(m_navigator->viewport()->mapToGlobal(point));
			return;
		}
		Pick pick;
		pick.solid = item->data(0, SolidRole).toInt();
		pick.node = item->data(0, NodeRole).toInt();
		if (pick.isNull()) return;
		m_view->select(pick);
		showMenu(pick, m_navigator->viewport()->mapToGlobal(point));
	});

	reload();
}

Cabinet3dWindow::~Cabinet3dWindow()
{
		//What is being read goes on, and is kept for the next time
	if (m_loader) m_loader->disconnect(this);
}

CabinetItem *Cabinet3dWindow::plateItem() const
{
	if (!m_cabinet) return nullptr;
	CabinetItem *largest = nullptr;
	for (QGraphicsItem *item : m_cabinet->items())
	{
		auto *part = qgraphicsitem_cast<CabinetItem *>(item);
		if (!part || part->kind() != CabinetItem::Kind::MountingPlate) continue;
			//The one holding an enclosure, else the largest
		if (!part->properties().enclosure.isEmpty()) return part;
		const QSizeF size = part->sizeMm();
		if (!largest || size.width() * size.height()
			> largest->sizeMm().width() * largest->sizeMm().height()) {
			largest = part;
		}
	}
	return largest;
}

void Cabinet3dWindow::reload()
{
	if (m_plate) m_plate->disconnect(this);
	m_plate = plateItem();
	if (m_plate) {
		connect(m_plate, &CabinetItem::propertiesChanged, this, [this]() {
			const Enclosure enclosure = Enclosure::fromString(m_plate->properties().enclosure);
			const QList<Cabinet3d::Solid> solids = m_view->solids();
			const int plate = m_view->enclosurePlate();
			const QSizeF size = m_plate->sizeMm();
			const bool same_size = plate >= 0
					&& qAbs(solids.at(plate).size().x() - size.width()) < 0.01
					&& qAbs(solids.at(plate).size().y() - size.height()) < 0.01;
			if (sourceKey(enclosure) != m_loaded_source || !same_size
				|| symbolsOf(enclosure) != symbolsOf(m_view->enclosure())) {
				reload();
				return;
			}
				//Only what is hidden, its plate, its turn, where the
				//devices placed in 3D stand
			const Pick selected = m_view->selected();
			m_view->setHidden(enclosure.hidden);
			m_view->setEnclosure(plate, enclosure, m_view->enclosureModel());
			m_view->select(selected);
			updateEyes();
		});
	}
	m_enclosure_button->setEnabled(m_plate);
	m_enclosure_button->setToolTip(m_plate ? QString()
										   : tr("Draw the mounting plate on the sheet first"));

	QApplication::setOverrideCursor(Qt::WaitCursor);
	const Pick selected = m_view->selected();
	const QUuid selected_item = selected.solid >= 0 && selected.solid < m_view->solids().size()
			? m_view->solids().at(selected.solid).item : QUuid();
	m_view->setSolids(Cabinet3d::solidsOf(m_cabinet));
	m_unplaced = Cabinet3d::unplacedOf(m_cabinet, m_plate ? symbolsOf(Enclosure::fromString(
															  m_plate->properties().enclosure))
														  : QList<QUuid>());
	QApplication::restoreOverrideCursor();
	applyEnclosure();
	fillNavigator();
		//What was checked was another cabinet
	m_problem_list.clear();
	m_problem_shown = -1;
	m_problems->clear();
	m_problems->hide();
	showClearances(m_clearances->isChecked());
	loadModels();
		//What was picked stays picked
	for (int i = 0 ; i < m_view->solids().size() && !selected_item.isNull() ; ++i)
	{
		if (m_view->solids().at(i).item != selected_item) continue;
		Pick pick;
		pick.solid = i;
		m_view->select(pick);
		break;
	}
}

void Cabinet3dWindow::reject()
{
		//Esc stops placing a device, or choosing the plate, before it
		//closes the window
	if (m_choosing_plate) {
		m_choosing_plate = false;
		m_info->clear();
		return;
	}
	if (m_placing >= 0 || m_view->isPlacing()) {
		m_placing = -1;
		m_view->setPlacing(false);
		m_info->clear();
		return;
	}
	QDialog::reject();
}

void Cabinet3dWindow::startPlacing(int index)
{
	if (index < 0 || index >= m_unplaced.size()) return;
	if (!m_plate) {
		m_info->setText(tr("Draw the mounting plate on the sheet first"));
		return;
	}
	m_placing = index;
	m_view->setPlacing(true);
	m_info->setText(tr("Click the face where %1 goes: the door, a side, the roof, the plate. Esc: no.")
					.arg(m_unplaced.at(index).label.toHtmlEscaped()));
}

bool Cabinet3dWindow::placeOnFace(int index, const Pick &pick, const QVector3D &at, const QVector3D &normal)
{
	if (!m_plate || index < 0 || index >= m_unplaced.size()) return false;
	const Cabinet3d::Unplaced &device = m_unplaced.at(index);
	CabinetEnclosure::Mounted mounted;
	mounted.symbol = device.symbol;
	mounted.label = device.label;
	mounted.manufacturer = device.manufacturer;
	mounted.order_number = device.order_number;
	const QVector3D up = CabinetEnclosure::faceUp(normal);
	if (pick.node >= 0 && pick.node < m_view->enclosureModel().nodes.size()) {
			//On a part of the enclosure: in its axes, it goes with it
		const QMatrix4x4 inverse = m_view->enclosurePlacement().inverted();
		mounted.node = m_view->enclosureModel().nodes.at(pick.node).id;
		mounted.at = inverse.map(at);
		mounted.normal = inverse.mapVector(normal).normalized();
		mounted.up = inverse.mapVector(up).normalized();
	} else {
		mounted.at = at;
		mounted.normal = normal.normalized();
		mounted.up = up;
	}
	Enclosure enclosure = Enclosure::fromString(m_plate->properties().enclosure);
	enclosure.mounted << mounted;
	m_info->clear();
	changePlate(m_plate, enclosure.toString(), tr("Place %1 in 3D").arg(device.label));
		//The new one picked
	for (int i = 0 ; i < m_view->solids().size() ; ++i)
	{
		if (m_view->solids().at(i).item != device.symbol) continue;
		Pick placed;
		placed.solid = i;
		m_view->select(placed);
		break;
	}
	return true;
}

bool Cabinet3dWindow::changeMounted(int mounted, const std::function<void(CabinetEnclosure::Mounted &)> &change,
									const QString &text)
{
	if (!m_plate) return false;
	Enclosure enclosure = Enclosure::fromString(m_plate->properties().enclosure);
	if (mounted < 0 || mounted >= enclosure.mounted.size()) return false;
	const QString label = enclosure.mounted.at(mounted).label;
	change(enclosure.mounted[mounted]);
	if (enclosure.mounted.at(mounted).symbol.isNull()) {
		enclosure.hidden.removeAll(QStringLiteral("item:")
								   + enclosure.mounted.at(mounted).symbol.toString(QUuid::WithoutBraces));
		enclosure.mounted.removeAt(mounted);
	}
	changePlate(m_plate, enclosure.toString(), text.arg(label));
	return true;
}

void Cabinet3dWindow::updateMountedTools()
{
	const Pick pick = m_view->selected();
	const QList<Cabinet3d::Solid> solids = m_view->solids();
	const int mounted = pick.solid >= 0 && pick.solid < solids.size() ? solids.at(pick.solid).mounted : -1;
	const Enclosure enclosure = m_view->enclosure();
	m_mounted_tools->setVisible(mounted >= 0 && mounted < enclosure.mounted.size());
	if (m_mounted_tools->isVisible()) {
		const QSignalBlocker blocker(m_sink);
		m_sink->setValue(enclosure.mounted.at(mounted).sink);
	}
}

void Cabinet3dWindow::applyEnclosure()
{
	const QList<Cabinet3d::Solid> solids = m_view->solids();
	int plate = -1;
	for (int i = 0 ; i < solids.size() && m_plate ; ++i)
	{
		if (solids.at(i).item == m_plate->uuid()) plate = i;
	}
	const Enclosure enclosure = plate >= 0 ? Enclosure::fromString(solids.at(plate).enclosure) : Enclosure();
	CabinetStep::Model model;
	if (enclosure.source == Enclosure::Source::Manual) {
		model = CabinetEnclosure::manualModel(enclosure);
	} else if (enclosure.hasModel() && plate >= 0) {
		CabinetStep::cached(solids.at(plate).enclosure_step, &model);
	}
	m_view->setHidden(enclosure.hidden);
	m_view->setEnclosure(plate, enclosure, model);
	m_loaded_source = sourceKey(enclosure);

	int devices = 0;
	for (const Cabinet3d::Solid &solid : solids) devices += solid.kind == Cabinet3d::Solid::Kind::Device;
	QString summary = tr("%n part(s)", "", int(solids.size()));
	if (CabinetStep::available()) {
		summary += QStringLiteral(", ") + tr("%n with a STEP model", "", m_view->withModel());
	} else {
		summary += QStringLiteral(". ") + tr("Without OpenCASCADE the devices are boxes.");
	}
	if (plate >= 0 && enclosure.hasModel() && !solids.at(plate).enclosure_error.isEmpty()
		&& enclosure.source != Enclosure::Source::Manual) {
		summary += QStringLiteral(". ") + solids.at(plate).enclosure_error;
	}
	m_summary->setText(summary);
	Q_UNUSED(devices)
}

void Cabinet3dWindow::loadModels()
{
	if (!CabinetStep::available()) return;
	QList<QByteArray> blobs;
	auto add = [&blobs](const QByteArray &step) {
		if (step.isEmpty() || blobs.contains(step) || CabinetStep::cached(step)) return;
		blobs << step;
	};
	for (const Cabinet3d::Solid &solid : m_view->solids())
	{
		add(solid.step);
		if (!solid.enclosure.isEmpty()) add(solid.enclosure_step);
	}
		//Those to place, read before they are placed
	for (const Cabinet3d::Unplaced &device : std::as_const(m_unplaced)) add(device.step);
	if (blobs.isEmpty()) return;
	m_progress->setRange(0, int(blobs.size()));
	m_progress->setValue(0);
	m_progress->show();
	QPointer<Cabinet3dWindow> self(this);
	m_loader->setFuture(QtConcurrent::run([blobs, self]() {
		int done = 0;
		for (const QByteArray &blob : blobs)
		{
			CabinetStep::read(blob);
			++done;
			QMetaObject::invokeMethod(qApp, [self, done]() {
				if (self) self->m_progress->setValue(done);
			}, Qt::QueuedConnection);
		}
	}));
}

void Cabinet3dWindow::waitForModels()
{
	if (m_loader->isRunning()) m_loader->waitForFinished();
	for (int i = 0 ; i < 3 ; ++i) QCoreApplication::processEvents();
}

void Cabinet3dWindow::fillNavigator()
{
	m_filling = true;
	const QString current = m_navigator->currentItem()
			? m_navigator->currentItem()->data(0, KeyRole).toString() : QString();
	m_navigator->clear();
	const QList<Cabinet3d::Solid> solids = m_view->solids();
	auto row = [](QTreeWidgetItem *parent, const QString &name, const QString &key, int solid, int node) {
		auto *item = new QTreeWidgetItem(parent);
		item->setText(0, name);
		item->setData(0, KeyRole, key);
		item->setData(0, SolidRole, solid);
		item->setData(0, NodeRole, node);
		return item;
	};
	auto group = [this](const QString &name) {
		auto *item = new QTreeWidgetItem(m_navigator);
		item->setText(0, name);
		item->setData(0, SolidRole, -1);
		item->setData(0, NodeRole, -1);
		QFont font = item->font(0);
		font.setBold(true);
		item->setFont(0, font);
		item->setExpanded(true);
		return item;
	};

	const struct {Cabinet3d::Solid::Kind kind; QString name;} kinds[] = {
		{Cabinet3d::Solid::Kind::Plate, tr("Mounting plate")},
		{Cabinet3d::Solid::Kind::Duct, tr("Wire ducts")},
		{Cabinet3d::Solid::Kind::Rail, tr("DIN rails")},
		{Cabinet3d::Solid::Kind::Device, tr("Devices")}};
	for (const auto &kind : kinds)
	{
		QTreeWidgetItem *parent = nullptr;
		for (int i = 0 ; i < solids.size() ; ++i)
		{
			if (solids.at(i).kind != kind.kind || solids.at(i).mounted >= 0) continue;
			if (!parent) parent = group(kind.name);
			QString name = solids.at(i).name;
			if (!solids.at(i).part.isEmpty()) name += QStringLiteral("  ·  ") + solids.at(i).part;
			row(parent, name, Cabinet3dView::key(solids.at(i)), i, -1);
		}
		if (parent) parent->setExpanded(kind.kind != Cabinet3d::Solid::Kind::Device || parent->childCount() < 30);
	}

	const Enclosure enclosure = m_view->enclosure();
	const CabinetStep::Model &model = m_view->enclosureModel();
	QVector<QTreeWidgetItem *> items(model.nodes.size(), nullptr);
	if (enclosure.hasModel()) {
		QTreeWidgetItem *parent = group(tr("Enclosure: %1").arg(enclosure.title()));
		if (!model.ok) {
			auto *waiting = row(parent, m_loader->isRunning() ? tr("Reading…") : tr("Not read"),
								QString(), -1, -1);
			waiting->setDisabled(true);
		}
		for (int n = 0 ; n < model.nodes.size() ; ++n)
		{
			const CabinetStep::Node &node = model.nodes.at(n);
			QTreeWidgetItem *holder = node.parent >= 0 && items.at(node.parent) ? items.at(node.parent) : parent;
			QString name = node.name;
			if (enclosure.plate == node.id && m_view->enclosurePlaced())
				name += QStringLiteral("  ·  ") + tr("mounting plate");
			items[n] = row(holder, name, QStringLiteral("node:") + node.id, -1, n);
			items[n]->setExpanded(node.depth == 0);
		}
	}

		//The devices placed in 3D, under the face they stand on
	QTreeWidgetItem *devices_group = nullptr;
	for (int i = 0 ; i < m_navigator->topLevelItemCount() ; ++i)
	{
		if (m_navigator->topLevelItem(i)->text(0) == tr("Devices")) devices_group = m_navigator->topLevelItem(i);
	}
	for (int i = 0 ; i < solids.size() ; ++i)
	{
		const int mounted = solids.at(i).mounted;
		if (mounted < 0 || mounted >= enclosure.mounted.size()) continue;
		const int node = model.node(enclosure.mounted.at(mounted).node);
		QTreeWidgetItem *holder = node >= 0 ? items.at(node) : nullptr;
		if (!holder) {
			if (!devices_group) devices_group = group(tr("Devices"));
			holder = devices_group;
		}
		QString name = solids.at(i).name + QStringLiteral("  ·  ") + tr("in 3D");
		if (!solids.at(i).part.isEmpty()) name += QStringLiteral("  ·  ") + solids.at(i).part;
		auto *item = row(holder, name, Cabinet3dView::key(solids.at(i)), i, -1);
		item->setIcon(0, icon(palette().color(QPalette::Text)));
		holder->setExpanded(true);
	}

		//Those to place: on the door, a side, the roof
	if (!m_unplaced.isEmpty()) {
		QTreeWidgetItem *parent = group(tr("Unplaced 3D"));
		parent->setToolTip(0, tr("Devices of the schematic with a STEP model and no drawing on a cabinet "
								 "sheet: right click, Place in 3D"));
		for (int i = 0 ; i < m_unplaced.size() ; ++i)
		{
			const Cabinet3d::Unplaced &device = m_unplaced.at(i);
			QString name = device.label;
			const QString part = QStringLiteral("%1 %2").arg(device.manufacturer, device.order_number).trimmed();
			if (!part.isEmpty()) name += QStringLiteral("  ·  ") + part;
			auto *item = row(parent, name, QString(), -1, -1);
			item->setData(0, UnplacedRole, i);
			item->setToolTip(0, device.description);
		}
	}
	m_filling = false;
	updateEyes();
	if (!current.isEmpty()) {
		for (QTreeWidgetItemIterator it(m_navigator) ; *it ; ++it)
		{
			if ((*it)->data(0, KeyRole).toString() == current) {
				m_navigator->setCurrentItem(*it);
				break;
			}
		}
	}
}

void Cabinet3dWindow::updateEyes()
{
	const QColor ink = palette().color(QPalette::Text);
	const QIcon open = eyeIcon(true, ink), shut = eyeIcon(false, ink);
	const QStringList hidden = m_view->hidden();
	const CabinetStep::Model &model = m_view->enclosureModel();
		//Children first: a group shows open while one of them is
	QList<QTreeWidgetItem *> items;
	for (QTreeWidgetItemIterator it(m_navigator) ; *it ; ++it) items << *it;
	for (int i = int(items.size()) - 1 ; i >= 0 ; --i)
	{
		QTreeWidgetItem *item = items.at(i);
		const QString key = item->data(0, KeyRole).toString();
		const int node = item->data(0, NodeRole).toInt();
		bool shown;
		if (!key.isEmpty()) {
			shown = node >= 0 ? !CabinetEnclosure::isHidden(model, node, hidden) : !hidden.contains(key);
		} else {
			shown = false;
			for (int c = 0 ; c < item->childCount() ; ++c)
				shown = shown || item->child(c)->data(0, Qt::UserRole + 10).toBool();
		}
		item->setData(0, Qt::UserRole + 10, shown);
		if (key.isEmpty() && item->childCount() == 0) continue;
		item->setIcon(1, shown ? open : shut);
		item->setForeground(0, shown ? palette().color(QPalette::Text)
									 : palette().color(QPalette::Disabled, QPalette::Text));
	}
}

void Cabinet3dWindow::navigatorClicked(QTreeWidgetItem *item, int column)
{
	if (!item || m_filling) return;
	if (column == 1) {
		QStringList keys;
		const QString key = item->data(0, KeyRole).toString();
		if (!key.isEmpty()) {
			keys << key;
		} else {
			for (int c = 0 ; c < item->childCount() ; ++c)
			{
				const QString child = item->child(c)->data(0, KeyRole).toString();
				if (!child.isEmpty()) keys << child;
			}
		}
		if (keys.isEmpty()) return;
		const bool shown = item->data(0, Qt::UserRole + 10).toBool();
		QStringList hidden = m_view->hidden();
		for (const QString &each : std::as_const(keys))
		{
			hidden.removeAll(each);
			if (shown) hidden << each;
		}
		if (m_plate) {
			Enclosure enclosure = Enclosure::fromString(m_plate->properties().enclosure);
			enclosure.hidden = hidden;
			changePlate(m_plate, enclosure.toString(),
						(shown ? tr("Hide %1") : tr("Show %1")).arg(item->text(0)));
		} else {
			m_view->setHidden(hidden);
			updateEyes();
		}
		return;
	}
	Pick pick;
	pick.solid = item->data(0, SolidRole).toInt();
	pick.node = item->data(0, NodeRole).toInt();
	if (!pick.isNull()) m_view->select(pick);
}

void Cabinet3dWindow::setHiddenKey(const QString &key, bool hidden)
{
	for (QTreeWidgetItemIterator it(m_navigator) ; *it ; ++it)
	{
		if ((*it)->data(0, KeyRole).toString() != key) continue;
		if ((*it)->data(0, Qt::UserRole + 10).toBool() == hidden) navigatorClicked(*it, 1);
		return;
	}
}

namespace
{
	/// A round sign of @p color for a line of the problems
	QIcon dot(const QColor &color)
	{
		QPixmap pixmap(14, 14);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setPen(color.darker(130));
		painter.setBrush(color);
		painter.drawEllipse(QRectF(1.5, 1.5, 11, 11));
		return QIcon(pixmap);
	}
}

void Cabinet3dWindow::showClearances(bool shown)
{
	CabinetItem::setClearancesShown(shown);
	if (m_cabinet) {
		QHash<QUuid, Cabinet3d::Clearance> by_item;
		for (const Cabinet3d::Solid &solid : m_view->solids())
		{
			if (solid.kind == Cabinet3d::Solid::Kind::Device && solid.mounted < 0
				&& solid.clearance.any()) {
				by_item.insert(solid.item, solid.clearance);
			}
		}
		for (QGraphicsItem *item : m_cabinet->items())
		{
			auto *part = qgraphicsitem_cast<CabinetItem *>(item);
			if (!part) continue;
			const Cabinet3d::Clearance clearance = by_item.value(part->uuid());
			part->setClearance(QMarginsF(clearance.left, clearance.top,
										 clearance.right, clearance.bottom));
		}
	}
	updateMarks();
}

void Cabinet3dWindow::updateMarks()
{
	QList<Cabinet3dView::Mark> marks;
	if (m_clearances->isChecked()) {
		for (const CabinetSpace::Body &body : m_view->bodies())
		{
			for (int side = CabinetSpace::Top ; side <= CabinetSpace::Front ; ++side)
			{
				Cabinet3dView::Mark mark;
				mark.color = QColor(255, 170, 0, 55);
				if (CabinetSpace::zone(body, CabinetSpace::Side(side), mark.min, mark.max))
					marks << mark;
			}
		}
	}
	if (m_problem_shown >= 0 && m_problem_shown < m_problem_list.size()) {
		const CabinetSpace::Problem &problem = m_problem_list.at(m_problem_shown);
			//The free space asked, then where it goes wrong
		if (problem.kind == CabinetSpace::Problem::Kind::Clearance
			&& problem.first >= 0 && problem.first < m_bodies.size()) {
			Cabinet3dView::Mark asked;
			asked.color = QColor(255, 170, 0, 70);
			if (CabinetSpace::zone(m_bodies.at(problem.first), problem.side, asked.min, asked.max))
				marks << asked;
		}
		Cabinet3dView::Mark mark;
		const QVector3D margin(0.5f, 0.5f, 0.5f);
		mark.min = problem.min - margin;
		mark.max = problem.max + margin;
		mark.color = problem.kind == CabinetSpace::Problem::Kind::Clearance
				? QColor(255, 110, 0, 130) : QColor(230, 30, 30, 140);
		marks << mark;
	}
	m_view->setMarks(marks);
}

void Cabinet3dWindow::checkRoom()
{
	m_bodies = m_view->bodies();
	QVector3D inside_min, inside_max;
	const bool has_inside = m_view->inside(inside_min, inside_max);
	m_problem_list = CabinetSpace::check(m_bodies, has_inside, inside_min, inside_max);
	m_problem_shown = -1;
	{
		const QSignalBlocker blocker(m_problems);
		m_problems->clear();
		auto note = [this](const QString &text, const QColor &color) {
			auto *item = new QTreeWidgetItem(m_problems, {text});
			item->setData(0, Qt::UserRole, -1);
			item->setIcon(0, dot(color));
			item->setToolTip(0, text);
		};
		for (int i = 0 ; i < m_problem_list.size() ; ++i)
		{
			const CabinetSpace::Problem &problem = m_problem_list.at(i);
			const QString text = CabinetSpace::describe(problem, m_bodies);
			auto *item = new QTreeWidgetItem(m_problems, {text});
			item->setData(0, Qt::UserRole, i);
			item->setToolTip(0, text);
			item->setIcon(0, dot(problem.kind == CabinetSpace::Problem::Kind::Clearance
								 ? QColor(255, 140, 0) : QColor(220, 40, 40)));
		}
		if (m_problem_list.isEmpty())
			note(tr("Room enough: no part in another, none in the free space a device asks"),
				 QColor(40, 170, 70));
		if (!has_inside)
			note(tr("No enclosure takes the plate: its walls and its door are not checked"), Qt::gray);
		if (m_progress->isVisible())
			note(tr("STEP models still being read: their boxes until then"), Qt::gray);
		m_problems->headerItem()->setText(0, m_problem_list.isEmpty()
										  ? tr("Room")
										  : tr("Room: %n problem(s)", "", int(m_problem_list.size())));
	}
	m_problems->show();
	updateMarks();
}

void Cabinet3dWindow::showProblem(int index)
{
	m_problem_shown = index;
	updateMarks();
	if (index < 0 || index >= m_problem_list.size()) return;
	const CabinetSpace::Problem &problem = m_problem_list.at(index);
	const QList<Cabinet3d::Solid> solids = m_view->solids();
	auto solidOf = [this, &solids](int body) {
		const int solid = body >= 0 && body < m_bodies.size() ? m_bodies.at(body).solid : -1;
		return solid < solids.size() ? solid : -1;
	};
	Pick pick;
	pick.solid = solidOf(problem.first);
	m_view->select(pick);
	showPick(pick);
		//The parts on the sheet too
	if (!m_cabinet) return;
	QSet<QUuid> items;
	for (int body : {problem.first, problem.second})
	{
		const int solid = solidOf(body);
		if (solid >= 0) items.insert(solids.at(solid).item);
	}
	m_cabinet->clearSelection();
	for (QGraphicsItem *item : m_cabinet->items())
	{
		auto *part = qgraphicsitem_cast<CabinetItem *>(item);
		if (part && items.contains(part->uuid())) part->setSelected(true);
	}
}

void Cabinet3dWindow::askForPlate()
{
	m_choosing_plate = true;
	m_info->setText(tr("Click the part of the enclosure which is its mounting plate: "
					   "the plate of the sheet takes its size. Esc: no."));
}

void Cabinet3dWindow::showPick(const Pick &pick)
{
	if (m_choosing_plate && pick.node >= 0 && pick.node < m_view->enclosureModel().nodes.size()) {
		const CabinetStep::Node &node = m_view->enclosureModel().nodes.at(pick.node);
		m_choosing_plate = false;
		if (usePlate(node.id, true) && m_plate) {
			const QSizeF size = m_plate->sizeMm();
			m_info->setText(tr("The mounting plate is %1: %2 × %3 mm.")
							.arg(node.name.toHtmlEscaped(),
								 QLocale().toString(size.width(), 'f', 0),
								 QLocale().toString(size.height(), 'f', 0)));
		} else {
			m_info->setText(tr("%1 cannot be the mounting plate.").arg(node.name.toHtmlEscaped()));
		}
		return;
	}
	updateMountedTools();
	const QList<Cabinet3d::Solid> solids = m_view->solids();
	QString key;
	QVector3D min, max;
	QStringList parts;
	if (pick.solid >= 0 && pick.solid < solids.size()) {
		const Cabinet3d::Solid &solid = solids.at(pick.solid);
		key = Cabinet3dView::key(solid);
		m_view->drawnBox(pick.solid, min, max);
		parts << QStringLiteral("<b>%1</b>").arg(solid.name.toHtmlEscaped()) << millimetres(max - min);
		if (solid.kind == Cabinet3d::Solid::Kind::Device) {
			if (!solid.part.isEmpty()) parts << solid.part.toHtmlEscaped();
			parts << (solid.step_name.isEmpty() ? tr("no STEP model in the parts database: a box")
												: tr("STEP: %1").arg(solid.step_name.toHtmlEscaped()));
		}
	} else if (pick.node >= 0 && pick.node < m_view->enclosureModel().nodes.size()) {
		const CabinetStep::Node &node = m_view->enclosureModel().nodes.at(pick.node);
		key = QStringLiteral("node:") + node.id;
		m_view->nodeBox(pick.node, min, max);
		parts << QStringLiteral("<b>%1</b>").arg(node.name.toHtmlEscaped()) << millimetres(max - min)
			  << tr("of the enclosure");
		if (!m_view->enclosurePlaced())
			parts << tr("right click its mounting plate: Use as the mounting plate");
	}
	m_info->setText(parts.join(QStringLiteral(" · ")));
	if (key.isEmpty() || m_filling) return;
	for (QTreeWidgetItemIterator it(m_navigator) ; *it ; ++it)
	{
		if ((*it)->data(0, KeyRole).toString() != key) continue;
		const QSignalBlocker blocker(m_navigator);
		m_navigator->setCurrentItem(*it);
		m_navigator->scrollToItem(*it);
		break;
	}
}

void Cabinet3dWindow::showMenu(const Pick &pick, const QPoint &global)
{
	if (pick.isNull()) return;
	QString key;
	QString name;
	if (pick.solid >= 0) {
		key = Cabinet3dView::key(m_view->solids().at(pick.solid));
		name = m_view->solids().at(pick.solid).name;
	} else {
		key = QStringLiteral("node:") + m_view->enclosureModel().nodes.at(pick.node).id;
		name = m_view->enclosureModel().nodes.at(pick.node).name;
	}
	QMenu menu(this);
	const Enclosure enclosure = m_view->enclosure();
	if (pick.node >= 0 && enclosure.source != Enclosure::Source::Manual && m_plate) {
		const QString id = m_view->enclosureModel().nodes.at(pick.node).id;
		menu.addAction(tr("Use as the mounting plate"), this, [this, id]() {
			Enclosure taken = m_view->enclosure();
			taken.plate = id;
			taken.quarter_turns = 0;
			const int plate = m_view->enclosurePlate();
			const QSizeF size = CabinetEnclosure::plateSize(m_view->enclosureModel(), taken,
															m_view->solids().at(plate));
			const QVector3D ours = m_view->solids().at(plate).size();
			bool adjust = false;
			if (size.isValid() && (qAbs(size.width() - ours.x()) > 1 || qAbs(size.height() - ours.y()) > 1)) {
				const QLocale locale;
				adjust = QMessageBox::question(
							this, windowTitle(),
							tr("The plate chosen is %1 × %2 mm, the plate of the sheet %3 × %4 mm. "
							   "Give the plate of the sheet the size of the one chosen?")
							.arg(locale.toString(qRound(size.width())), locale.toString(qRound(size.height())),
								 locale.toString(qRound(ours.x())), locale.toString(qRound(ours.y()))))
						== QMessageBox::Yes;
			}
			usePlate(id, adjust);
		});
		menu.addSeparator();
	}
	if (pick.solid >= 0 && m_view->solids().at(pick.solid).mounted >= 0) {
		const int mounted = m_view->solids().at(pick.solid).mounted;
		menu.addAction(tr("Take it out of 3D"), this, [this, mounted]() {
			changeMounted(mounted, [](CabinetEnclosure::Mounted &each) {each.symbol = QUuid();},
						  tr("Take %1 out of 3D"));
		});
		menu.addSeparator();
	}
	const bool hidden = m_view->hidden().contains(key);
	menu.addAction(hidden ? tr("Show") : tr("Hide"), this, [this, key, hidden]() {
		setHiddenKey(key, !hidden);
	});
	menu.addAction(tr("Show everything"), this, [this]() {
		if (m_plate) {
			Enclosure all = Enclosure::fromString(m_plate->properties().enclosure);
			if (all.hidden.isEmpty()) return;
			all.hidden.clear();
			changePlate(m_plate, all.toString(), tr("Show everything"));
		} else {
			m_view->setHidden({});
			updateEyes();
		}
	});
	Q_UNUSED(name)
	menu.exec(global);
}

void Cabinet3dWindow::changePlate(const CabinetItem *plate, const QString &enclosure, const QString &text,
								  qreal length, qreal width)
{
	if (!plate || !m_cabinet) return;
	CabinetItem::Properties properties = plate->properties();
	properties.enclosure = enclosure;
	if (length > 0) properties.length = length;
	if (width > 0) properties.width = width;
	if (properties == plate->properties()) return;
	auto *command = new ChangeCabinetItemCommand(const_cast<CabinetItem *>(plate), properties);
	command->setText(text);
	m_cabinet->undoStack().push(command);
}

bool Cabinet3dWindow::setEnclosure(const Enclosure &enclosure)
{
	if (!m_plate) return false;
	Enclosure kept = enclosure;
		//What is hidden of the folio stays so
	for (const QString &key : Enclosure::fromString(m_plate->properties().enclosure).hidden)
	{
		if (key.startsWith(QLatin1String("item:")) && !kept.hidden.contains(key)) kept.hidden << key;
	}
	changePlate(m_plate, kept.toString(), tr("Change the enclosure"));
	if (kept.hasModel() && kept.source != Enclosure::Source::Manual) {
		m_info->setText(tr("Right click the mounting plate of the enclosure, in the navigator or "
						   "in the view: Use as the mounting plate."));
	}
	return true;
}

bool Cabinet3dWindow::usePlate(const QString &id, bool adjust)
{
	if (!m_plate || m_view->enclosurePlate() < 0) return false;
	Enclosure enclosure = Enclosure::fromString(m_plate->properties().enclosure);
	if (!enclosure.hasModel() || enclosure.source == Enclosure::Source::Manual) return false;
	const CabinetStep::Model &model = m_view->enclosureModel();
	const int node = model.node(id);
	if (node < 0) return false;
	enclosure.plate = id;
	enclosure.quarter_turns = 0;
	const QString text = tr("Use %1 as the mounting plate").arg(model.nodes.at(node).name);
	if (adjust) {
		const QSizeF size = CabinetEnclosure::plateSize(model, enclosure,
														m_view->solids().at(m_view->enclosurePlate()));
		changePlate(m_plate, enclosure.toString(), text, size.width(), size.height());
	} else {
		changePlate(m_plate, enclosure.toString(), text);
	}
	return true;
}

void Cabinet3dWindow::chooseFromParts()
{
	if (!PartsDatabase::isEnabled()) {
		QMessageBox::information(this, windowTitle(), tr("The parts database is not in use."));
		return;
	}
	PartsDatabase database;
	QString error;
	if (!database.open(PartsDatabase::path(), &error)) {
		QMessageBox::warning(this, windowTitle(), error);
		return;
	}
	PartPickerDialog picker(&database, this, QStringLiteral("enclosure"));
	if (picker.exec() != QDialog::Accepted) return;
	const std::optional<Part> part = picker.selectedPart();
	if (!part) return;
	bool has_step = false;
	for (const PartFile &file : database.files(part->id)) has_step = has_step || file.kind == PartFileKind::Step;
	if (!has_step) {
		QMessageBox::information(this, windowTitle(),
								 tr("%1 %2 has no STEP model in the parts database.")
								 .arg(part->manufacturer, part->order_number));
		return;
	}
	Enclosure enclosure;
	enclosure.source = Enclosure::Source::Parts;
	enclosure.manufacturer = part->manufacturer;
	enclosure.order_number = part->order_number;
	setEnclosure(enclosure);
}

void Cabinet3dWindow::chooseFile()
{
	const QString path = QFileDialog::getOpenFileName(this, tr("STEP of the enclosure"), QString(),
													  tr("STEP files (*.step *.stp)"));
	if (path.isEmpty()) return;
	Enclosure enclosure;
	enclosure.source = Enclosure::Source::File;
	enclosure.path = path;
	setEnclosure(enclosure);
}

void Cabinet3dWindow::enterMeasures()
{
	Enclosure current = m_plate ? Enclosure::fromString(m_plate->properties().enclosure) : Enclosure();
	if (current.source != Enclosure::Source::Manual && m_plate) {
			//The plate of the sheet, in an enclosure a little larger
		const QSizeF plate = m_plate->sizeMm();
		current.plate_width = plate.width();
		current.plate_height = plate.height();
		current.width = plate.width() + 100;
		current.height = plate.height() + 100;
		current.depth = 300;
	}
	QDialog dialog(this);
	dialog.setWindowTitle(tr("Enclosure by measures"));
	auto *form = new QFormLayout(&dialog);
	auto spin = [&dialog](double value) {
		auto *box = new QDoubleSpinBox(&dialog);
		box->setRange(50, 5000);
		box->setDecimals(0);
		box->setSuffix(QStringLiteral(" mm"));
		box->setValue(value);
		return box;
	};
	auto *height = spin(current.height);
	auto *width = spin(current.width);
	auto *depth = spin(current.depth);
	auto *plate_height = spin(current.plate_height);
	auto *plate_width = spin(current.plate_width);
	form->addRow(tr("Height of the enclosure:"), height);
	form->addRow(tr("Width of the enclosure:"), width);
	form->addRow(tr("Depth of the enclosure:"), depth);
	form->addRow(tr("Height of the plate:"), plate_height);
	form->addRow(tr("Width of the plate:"), plate_width);
	auto *note = new QLabel(tr("A hollow box of %1 mm sheet, open at the front, the plate against its back.")
							.arg(QLocale().toString(CabinetEnclosure::sheet)), &dialog);
	note->setWordWrap(true);
	form->addRow(note);
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	form->addRow(buttons);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	if (dialog.exec() != QDialog::Accepted) return;
	Enclosure enclosure;
	enclosure.source = Enclosure::Source::Manual;
	enclosure.height = height->value();
	enclosure.width = width->value();
	enclosure.depth = depth->value();
	enclosure.plate_height = plate_height->value();
	enclosure.plate_width = plate_width->value();
	enclosure.plate = QStringLiteral("plate");
	setEnclosure(enclosure);
}

QIcon Cabinet3dWindow::icon(const QColor &color)
{
	QPixmap pixmap(32, 32);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
	const QPointF top(16, 4), left(4, 10), right(28, 10), middle(16, 16);
	const QPointF bottom_left(4, 23), bottom_right(28, 23), bottom(16, 29);
	QColor side = color;
	side.setAlpha(60);
	painter.setBrush(side);
	painter.drawPolygon(QPolygonF({left, middle, bottom, bottom_left}));
	side.setAlpha(110);
	painter.setBrush(side);
	painter.drawPolygon(QPolygonF({middle, right, bottom_right, bottom}));
	painter.setBrush(Qt::NoBrush);
	painter.drawPolygon(QPolygonF({top, right, middle, left}));
	painter.end();
	return QIcon(pixmap);
}

bool Cabinet3dWindow::exportStep(const QString &path, QString *error) const
{
	const QList<Cabinet3d::Solid> all = m_view->solids();
	QList<Cabinet3d::Solid> solids;
	QList<CabinetStep::Placed> models;
	for (int i = 0 ; i < all.size() ; ++i)
	{
		if (!m_view->isShown(i)) continue;
		if (all.at(i).mounted < 0) {
			solids << all.at(i);
			continue;
		}
			//Placed in 3D: its model where it stands
		bool ok = false;
		const QMatrix4x4 place = m_view->mountedWorld(i, &ok);
		if (ok) {
			const QString name = all.at(i).part.isEmpty()
					? all.at(i).name : QStringLiteral("%1 (%2)").arg(all.at(i).name, all.at(i).part);
			models << CabinetStep::Placed{all.at(i).step, place, name};
		}
	}
	const CabinetStep::Model &model = m_view->enclosureModel();
	const int plate = m_view->enclosurePlate();
	if (model.ok && plate >= 0) {
		const Enclosure enclosure = m_view->enclosure();
		if (enclosure.source == Enclosure::Source::Manual) {
				//Its sides and its plate, as boxes
			for (int n = 0 ; n < model.nodes.size() ; ++n)
			{
				const CabinetStep::Node &node = model.nodes.at(n);
				if (node.assembly || node.count <= 0) continue;
				Cabinet3d::Solid side;
				side.kind = Cabinet3d::Solid::Kind::Plate;
				side.name = node.name;
				m_view->nodeBox(n, side.min, side.max);
				const int i = node.first * CabinetStep::vertex_size;
				side.color = QColor::fromRgbF(model.vertices.at(i + 6), model.vertices.at(i + 7),
											  model.vertices.at(i + 8));
				solids << side;
			}
		} else {
			models << CabinetStep::Placed{all.at(plate).enclosure_step, m_view->enclosurePlacement(),
										  enclosure.title()};
		}
	}
	return CabinetStep::write(solids, path, error, models);
}

void Cabinet3dWindow::exportWithDialog()
{
	QString suggested = m_cabinet && m_cabinet->project() ? m_cabinet->project()->title() : QString();
	if (suggested.trimmed().isEmpty()) suggested = tr("cabinet");
	const QString path = QFileDialog::getSaveFileName(this, tr("Export the cabinet as STEP"),
													  suggested + QStringLiteral(".step"),
													  tr("STEP files (*.step *.stp)"));
	if (path.isEmpty()) return;
	QApplication::setOverrideCursor(Qt::WaitCursor);
	QString error;
	const bool written = exportStep(path, &error);
	QApplication::restoreOverrideCursor();
	if (!written) {
		QMessageBox::warning(this, windowTitle(), error);
		return;
	}
	m_info->setText(tr("Written: %1").arg(path.toHtmlEscaped()));
}
