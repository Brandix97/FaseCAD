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
#include "cabinetstep.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>

#include <array>
#include <cmath>
#include <limits>

#ifdef QET_HAS_OPENCASCADE
#include <BRepBndLib.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <Message.hxx>
#include <Message_Messenger.hxx>
#include <Message_PrinterOStream.hxx>
#include <NCollection_DataMap.hxx>
#include <Poly_Triangulation.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <STEPControl_Reader.hxx>
#include <TDF_LabelSequence.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_Editor.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFPrs.hxx>
#include <XCAFPrs_DocumentExplorer.hxx>
#include <XCAFPrs_IndexedDataMapOfShapeStyle.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <sstream>
#endif

namespace CabinetStep
{
namespace
{
	QString tr(const char *text)
	{
		return QCoreApplication::translate("CabinetStep", text);
	}

	/// The 24 turns of a box onto itself, the one leaving it as it is first
	QList<QMatrix4x4> turns()
	{
		static QList<QMatrix4x4> list;
		if (!list.isEmpty()) return list;
		const std::array<std::array<int, 3>, 6> orders {{
			{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}}};
		for (const auto &order : orders)
		{
			for (int signs = 0 ; signs < 8 ; ++signs)
			{
				QMatrix4x4 turn;
				turn.fill(0);
				turn(3, 3) = 1;
				for (int row = 0 ; row < 3 ; ++row)
					turn(row, order[row]) = (signs >> row) & 1 ? -1 : 1;
				if (turn.determinant() > 0) list << turn;
			}
		}
		return list;
	}

	/// The box @p min to @p max turned by @p turn
	void turned(const QMatrix4x4 &turn, const QVector3D &min, const QVector3D &max,
				QVector3D &out_min, QVector3D &out_max)
	{
		bool first = true;
		for (int corner = 0 ; corner < 8 ; ++corner)
		{
			const QVector3D point(corner & 1 ? max.x() : min.x(),
								  corner & 2 ? max.y() : min.y(),
								  corner & 4 ? max.z() : min.z());
			const QVector3D moved = turn.map(point);
			if (first) {
				out_min = out_max = moved;
				first = false;
				continue;
			}
			out_min = QVector3D(qMin(out_min.x(), moved.x()), qMin(out_min.y(), moved.y()),
								qMin(out_min.z(), moved.z()));
			out_max = QVector3D(qMax(out_max.x(), moved.x()), qMax(out_max.y(), moved.y()),
								qMax(out_max.z(), moved.z()));
		}
	}

#ifdef QET_HAS_OPENCASCADE
	/// OpenCASCADE says what it reads and writes on the console: not here
	void quiet()
	{
		static bool done = false;
		if (done) return;
		done = true;
		Message::DefaultMessenger()->RemovePrinters(STANDARD_TYPE(Message_PrinterOStream));
	}

	gp_Trsf trsf(const QMatrix4x4 &matrix)
	{
		gp_Trsf result;
		result.SetValues(matrix(0, 0), matrix(0, 1), matrix(0, 2), matrix(0, 3),
						 matrix(1, 0), matrix(1, 1), matrix(1, 2), matrix(1, 3),
						 matrix(2, 0), matrix(2, 1), matrix(2, 2), matrix(2, 3));
		return result;
	}

	/**
		The model of the STEP file @p step, with its parts, their names and
		their colours, as a component of @p parent placed by @p place and
		called @p name; false when it cannot be read
	*/
	bool addModel(const QByteArray &step, const Handle(XCAFDoc_ShapeTool) &shapes,
				  const TDF_Label &parent, const gp_Trsf &place, const QString &name)
	{
		std::istringstream stream(std::string(step.constData(), size_t(step.size())));
		STEPCAFControl_Reader reader;
		reader.SetColorMode(true);
		reader.SetNameMode(true);
		reader.SetLayerMode(true);
		if (reader.ReadStream("model.stp", stream) != IFSelect_RetDone) return false;
		Handle(XCAFApp_Application) application = XCAFApp_Application::GetApplication();
		Handle(TDocStd_Document) source;
		application->NewDocument("MDTV-XCAF", source);
		bool copied = false;
		if (reader.Transfer(source)) {
			TDF_LabelSequence free_shapes;
			XCAFDoc_DocumentTool::ShapeTool(source->Main())->GetFreeShapes(free_shapes);
			if (!free_shapes.IsEmpty()) {
				const TDF_Label device = shapes->NewShape();
				copied = XCAFDoc_Editor::Extract(free_shapes, device, true);
				if (copied) {
					const TCollection_ExtendedString text(name.toUtf8().constData(), true);
					TDataStd_Name::Set(device, text);
					TDataStd_Name::Set(shapes->AddComponent(parent, device, TopLoc_Location(place)), text);
				} else {
					shapes->RemoveShape(device);
				}
			}
		}
		application->Close(source);
		return copied;
	}

	/// The name of @p label, empty when it has none
	QString nameOf(const TDF_Label &label)
	{
		Handle(TDataStd_Name) name;
		if (label.IsNull() || !label.FindAttribute(TDataStd_Name::GetID(), name)) return QString();
		const TCollection_ExtendedString &text = name->Get();
		return QString::fromUtf16(reinterpret_cast<const char16_t *>(text.ToExtString()), text.Length());
	}

	/// The triangles of the faces of @p shape into @p model, placed by
	/// @p place, in the colours of @p face_colors or @p color
	void mesh(const TopoDS_Shape &shape, const gp_Trsf &place, const Quantity_ColorRGBA &color,
			  const NCollection_DataMap<TopoDS_Shape, Quantity_ColorRGBA, TopTools_ShapeMapHasher> &face_colors,
			  Model &model)
	{
		Bnd_Box box;
		BRepBndLib::Add(shape, box);
		if (box.IsVoid()) return;
		Standard_Real x0, y0, z0, x1, y1, z1;
		box.Get(x0, y0, z0, x1, y1, z1);
		const double diagonal = std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)
										  + (z1 - z0) * (z1 - z0));
		BRepMesh_IncrementalMesh meshing(shape, qBound(0.05, diagonal / 300.0, 2.0), false, 0.5, true);
		for (TopExp_Explorer explore(shape, TopAbs_FACE) ; explore.More() ; explore.Next())
		{
			const TopoDS_Face face = TopoDS::Face(explore.Current());
			TopLoc_Location location;
			const Handle(Poly_Triangulation) triangles = BRep_Tool::Triangulation(face, location);
			if (triangles.IsNull()) continue;
			const gp_Trsf face_place = place.Multiplied(location.Transformation());
			const Quantity_ColorRGBA face_color = face_colors.IsBound(face) ? face_colors.Find(face) : color;
				//OpenCASCADE keeps linear RGB; drawn as the file has it, sRGB
			Standard_Real red, green, blue;
			face_color.GetRGB().Values(red, green, blue, Quantity_TOC_sRGB);
			const bool reversed = face.Orientation() == TopAbs_REVERSED;
			for (int t = 1 ; t <= triangles->NbTriangles() ; ++t)
			{
				int a, b, c;
				triangles->Triangle(t).Get(a, b, c);
				if (reversed) std::swap(b, c);
				const gp_Pnt points[3] = {triangles->Node(a).Transformed(face_place),
										  triangles->Node(b).Transformed(face_place),
										  triangles->Node(c).Transformed(face_place)};
				const QVector3D p0(float(points[0].X()), float(points[0].Y()), float(points[0].Z()));
				const QVector3D p1(float(points[1].X()), float(points[1].Y()), float(points[1].Z()));
				const QVector3D p2(float(points[2].X()), float(points[2].Y()), float(points[2].Z()));
				const QVector3D normal = QVector3D::normal(p0, p1, p2);
				for (const QVector3D &p : {p0, p1, p2})
				{
					model.vertices << p.x() << p.y() << p.z()
								   << normal.x() << normal.y() << normal.z()
								   << float(red) << float(green) << float(blue)
								   << face_color.Alpha();
				}
			}
		}
	}

	/// The box of the vertices @p first to @p first + @p count of @p model
	void boundsOf(const Model &model, int first, int count, QVector3D &min, QVector3D &max)
	{
		bool started = false;
		for (int v = first ; v < first + count ; ++v)
		{
			const int i = v * vertex_size;
			const QVector3D p(model.vertices[i], model.vertices[i + 1], model.vertices[i + 2]);
			if (!started) {
				min = max = p;
				started = true;
				continue;
			}
			min = QVector3D(qMin(min.x(), p.x()), qMin(min.y(), p.y()), qMin(min.z(), p.z()));
			max = QVector3D(qMax(max.x(), p.x()), qMax(max.y(), p.y()), qMax(max.z(), p.z()));
		}
	}

	Model readModel(const QByteArray &step)
	{
			//One file at a time: OpenCASCADE's application is shared
		static QMutex reading;
		QMutexLocker locker(&reading);
		Model model;
		quiet();
		std::istringstream stream(std::string(step.constData(), size_t(step.size())));
		STEPCAFControl_Reader reader;
		reader.SetColorMode(true);
		reader.SetNameMode(true);
		if (reader.ReadStream("model.stp", stream) != IFSelect_RetDone) {
			model.error = tr("The STEP file cannot be read.");
			return model;
		}
		Handle(XCAFApp_Application) application = XCAFApp_Application::GetApplication();
		Handle(TDocStd_Document) document;
		application->NewDocument("MDTV-XCAF", document);
		if (!reader.Transfer(document)) {
			model.error = tr("The STEP file holds no shape.");
			application->Close(document);
			return model;
		}

		const Quantity_ColorRGBA grey(Quantity_Color(0.72, 0.72, 0.70, Quantity_TOC_sRGB));
		QVector<int> open;	//The node of each depth above the current one
		for (XCAFPrs_DocumentExplorer explorer(document, XCAFPrs_DocumentExplorerFlags_None);
			 explorer.More() ; explorer.Next())
		{
			const XCAFPrs_DocumentNode &current = explorer.Current();
			Node node;
			node.depth = explorer.CurrentDepth();
			node.assembly = current.IsAssembly;
			node.id = QString::fromLatin1(current.Id.ToCString());
			node.name = nameOf(current.Label);
			if (node.name.isEmpty()) node.name = nameOf(current.RefLabel);
			if (node.name.isEmpty()) node.name = tr("Part %1").arg(model.nodes.size() + 1);
			open.resize(node.depth);
			node.parent = open.isEmpty() ? -1 : open.last();
			node.first = int(model.vertices.size() / vertex_size);
			const int index = int(model.nodes.size());
			model.nodes << node;
			open << index;
			if (node.assembly) continue;

			const TopoDS_Shape shape = XCAFDoc_ShapeTool::GetShape(current.RefLabel);
			if (shape.IsNull()) continue;
			const Quantity_ColorRGBA color = current.Style.IsSetColorSurf()
					? current.Style.GetColorSurfRGBA() : grey;
				//The colours of its faces, where they have their own: the
				//colour of a solid first, then those of its faces over it
				//(a PLC: sun yellow for the whole box, titanium
				//grey on each of its faces)
			NCollection_DataMap<TopoDS_Shape, Quantity_ColorRGBA, TopTools_ShapeMapHasher> face_colors;
			XCAFPrs_IndexedDataMapOfShapeStyle styles;
			XCAFPrs::CollectStyleSettings(current.RefLabel, TopLoc_Location(), styles);
			for (int type = TopAbs_COMPOUND ; type <= TopAbs_FACE ; ++type)
			{
				for (XCAFPrs_IndexedDataMapOfShapeStyle::Iterator it(styles) ; it.More() ; it.Next())
				{
					if (!it.Value().IsSetColorSurf() || it.Key().ShapeType() != type) continue;
					for (TopExp_Explorer face(it.Key(), TopAbs_FACE) ; face.More() ; face.Next())
						face_colors.Bind(face.Current(), it.Value().GetColorSurfRGBA());
				}
			}
			mesh(shape, current.Location.Transformation(), color, face_colors, model);
		}
		application->Close(document);

			//What each node holds: up to the next one as high as it
		const int total = int(model.vertices.size() / vertex_size);
		for (int i = 0 ; i < model.nodes.size() ; ++i)
		{
			int end = total;
			for (int j = i + 1 ; j < model.nodes.size() ; ++j)
			{
				if (model.nodes.at(j).depth <= model.nodes.at(i).depth) {
					end = model.nodes.at(j).first;
					break;
				}
			}
			Node &node = model.nodes[i];
			node.count = end - node.first;
			if (node.count > 0) boundsOf(model, node.first, node.count, node.min, node.max);
		}
		if (model.vertices.isEmpty()) {
			model.error = tr("The STEP file holds no surface.");
			return model;
		}
		boundsOf(model, 0, total, model.min, model.max);
		model.ok = true;
		return model;
	}
#endif

	QString &cacheFolderSetting()
	{
		static QString folder;
		return folder;
	}

	/// Version of the files of the disk cache
	const quint32 cache_magic = 0x46535450;	//FSTP
	const quint32 cache_version = 2;	//2: colours in sRGB, faces over solids

	QString cacheFile(const QByteArray &key)
	{
		return cacheFolder() + QLatin1Char('/') + QString::fromLatin1(key.toHex()) + QStringLiteral(".mesh");
	}

	bool loadFromDisk(const QByteArray &key, Model &model)
	{
		QFile file(cacheFile(key));
		if (!file.open(QIODevice::ReadOnly)) return false;
		QDataStream in(&file);
		in.setVersion(QDataStream::Qt_6_0);
		quint32 magic = 0, version = 0;
		in >> magic >> version;
		if (magic != cache_magic || version != cache_version) return false;
		Model read;
		qint32 nodes = 0;
		in >> read.vertices >> read.min >> read.max >> nodes;
		for (qint32 i = 0 ; i < nodes && in.status() == QDataStream::Ok ; ++i)
		{
			Node node;
			qint32 parent, depth, first, count;
			in >> node.name >> node.id >> parent >> depth >> node.assembly >> first >> count
			   >> node.min >> node.max;
			node.parent = parent;
			node.depth = depth;
			node.first = first;
			node.count = count;
			read.nodes << node;
		}
		if (in.status() != QDataStream::Ok || read.vertices.isEmpty()) return false;
		read.ok = true;
		model = read;
		return true;
	}

	void saveToDisk(const QByteArray &key, const Model &model)
	{
		if (!model.ok || !QDir().mkpath(cacheFolder())) return;
		QSaveFile file(cacheFile(key));
		if (!file.open(QIODevice::WriteOnly)) return;
		QDataStream out(&file);
		out.setVersion(QDataStream::Qt_6_0);
		out << cache_magic << cache_version << model.vertices << model.min << model.max
			<< qint32(model.nodes.size());
		for (const Node &node : model.nodes)
		{
			out << node.name << node.id << qint32(node.parent) << qint32(node.depth) << node.assembly
				<< qint32(node.first) << qint32(node.count) << node.min << node.max;
		}
		file.commit();
	}

	QMutex &keptMutex()
	{
		static QMutex mutex;
		return mutex;
	}

	QHash<QByteArray, Model> &kept()
	{
		static QHash<QByteArray, Model> models;
		return models;
	}

	QByteArray keyOf(const QByteArray &step)
	{
		return QCryptographicHash::hash(step, QCryptographicHash::Sha1);
	}
}

QList<QMatrix4x4> boxTurns()
{
	return turns();
}

int Model::node(const QString &id) const
{
	for (int i = 0 ; i < nodes.size() ; ++i)
	{
		if (nodes.at(i).id == id) return i;
	}
	return -1;
}

void setCacheFolder(const QString &folder)
{
	cacheFolderSetting() = folder;
}

QString cacheFolder()
{
	const QString folder = cacheFolderSetting();
	if (!folder.isEmpty()) return folder;
	return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/step");
}

bool cached(const QByteArray &step, Model *model)
{
	if (step.isEmpty()) return false;
	const QByteArray key = keyOf(step);
	{
		QMutexLocker locker(&keptMutex());
		const auto found = kept().constFind(key);
		if (found != kept().constEnd()) {
			if (model) *model = *found;
			return true;
		}
	}
	Model read;
	if (!loadFromDisk(key, read)) return false;
	QMutexLocker locker(&keptMutex());
	kept().insert(key, read);
	if (model) *model = read;
	return true;
}

void appendBox(QVector<float> &data, const QVector3D &min, const QVector3D &max,
			   const QColor &color, float alpha)
{
	const QVector3D c[8] = {
		{min.x(), min.y(), min.z()}, {max.x(), min.y(), min.z()},
		{max.x(), max.y(), min.z()}, {min.x(), max.y(), min.z()},
		{min.x(), min.y(), max.z()}, {max.x(), min.y(), max.z()},
		{max.x(), max.y(), max.z()}, {min.x(), max.y(), max.z()}};
	const int sides[6][4] = {{4, 5, 6, 7}, {1, 0, 3, 2}, {0, 4, 7, 3},
							 {5, 1, 2, 6}, {7, 6, 2, 3}, {0, 1, 5, 4}};
	const QVector3D normals[6] = {{0, 0, 1}, {0, 0, -1}, {-1, 0, 0},
								  {1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
	for (int side = 0 ; side < 6 ; ++side)
	{
		const int *s = sides[side];
		for (int corner : {s[0], s[1], s[2], s[0], s[2], s[3]})
		{
			data << c[corner].x() << c[corner].y() << c[corner].z()
				 << normals[side].x() << normals[side].y() << normals[side].z()
				 << float(color.redF()) << float(color.greenF()) << float(color.blueF()) << alpha;
		}
	}
}

bool available()
{
#ifdef QET_HAS_OPENCASCADE
	return true;
#else
	return false;
#endif
}

Model read(const QByteArray &step)
{
	if (step.isEmpty()) return Model();
	Model model;
	if (cached(step, &model)) return model;
#ifdef QET_HAS_OPENCASCADE
	model = readModel(step);
	const QByteArray key = keyOf(step);
	saveToDisk(key, model);
	QMutexLocker locker(&keptMutex());
	kept().insert(key, model);
	return model;
#else
	model.error = tr("FaseCAD was built without OpenCASCADE: it cannot read STEP files.");
	return model;
#endif
}

QMatrix4x4 orientation(const QVector3D &size, const QVector3D &target)
{
	QMatrix4x4 best;
	double best_score = std::numeric_limits<double>::max();
	auto ratio = [](float have, float want) {
		return want > 0 ? std::abs(double(have) - double(want)) / double(want) : 0.0;
	};
	for (const QMatrix4x4 &turn : turns())
	{
		QVector3D min, max;
		turned(turn, QVector3D(), size, min, max);
		const QVector3D extent = max - min;
		const double score = ratio(extent.x(), target.x()) + ratio(extent.y(), target.y())
				+ 0.3 * ratio(extent.z(), target.z());
			//The first of equal ones: the model as it is, when it fits
		if (score < best_score - 1e-6) {
			best_score = score;
			best = turn;
		}
	}
	return best;
}

QMatrix4x4 placement(const Model &model, const Cabinet3d::Solid &solid)
{
	const QMatrix4x4 turn = orientation(model.max - model.min, solid.size());
	QVector3D min, max;
	turned(turn, model.min, model.max, min, max);
		//In the middle of the drawing, its back on what it stands on
	const QVector3D center = solid.center();
	QMatrix4x4 move;
	move.translate(center.x() - (min.x() + max.x()) / 2,
				   center.y() - (min.y() + max.y()) / 2,
				   solid.min.z() - min.z());
	return move * turn;
}

bool write(const QList<Cabinet3d::Solid> &solids, const QString &path, QString *error,
		   const QList<Placed> &models)
{
#ifdef QET_HAS_OPENCASCADE
	quiet();
	Handle(XCAFApp_Application) application = XCAFApp_Application::GetApplication();
	Handle(TDocStd_Document) document;
	application->NewDocument("MDTV-XCAF", document);
	Handle(XCAFDoc_ShapeTool) shapes = XCAFDoc_DocumentTool::ShapeTool(document->Main());
	Handle(XCAFDoc_ColorTool) colors = XCAFDoc_DocumentTool::ColorTool(document->Main());

	const TDF_Label cabinet = shapes->NewShape();
	TDataStd_Name::Set(cabinet, TCollection_ExtendedString("Cabinet"));
	for (const Cabinet3d::Solid &solid : solids)
	{
		const QString name = solid.part.isEmpty() ? solid.name
												 : QStringLiteral("%1 (%2)").arg(solid.name, solid.part);
			//A device's model keeps its parts and their colours
		if (!solid.step.isEmpty()) {
			const Model model = read(solid.step);
			if (model.ok && addModel(solid.step, shapes, cabinet, trsf(placement(model, solid)), name))
				continue;
		}
		const QVector3D size = solid.size();
		if (size.x() <= 0 || size.y() <= 0 || size.z() <= 0) continue;
			//A rail and a duct as they are, of their pieces
		TopoDS_Compound shape;
		BRep_Builder builder;
		builder.MakeCompound(shape);
		for (const Cabinet3d::Piece &piece : Cabinet3d::piecesOf(solid, true))
		{
			builder.Add(shape, BRepPrimAPI_MakeBox(gp_Pnt(piece.min.x(), piece.min.y(), piece.min.z()),
												   gp_Pnt(piece.max.x(), piece.max.y(), piece.max.z())).Shape());
		}
		const TDF_Label part = shapes->AddShape(shape, false);
		const TCollection_ExtendedString text(name.toUtf8().constData(), true);
		TDataStd_Name::Set(part, text);
		colors->SetColor(part, Quantity_Color(solid.color.redF(), solid.color.greenF(),
											  solid.color.blueF(), Quantity_TOC_sRGB),
						 XCAFDoc_ColorSurf);
		TDataStd_Name::Set(shapes->AddComponent(cabinet, part, TopLoc_Location()), text);
	}
	for (const Placed &placed : models)
		addModel(placed.step, shapes, cabinet, trsf(placed.place), placed.name);
	shapes->UpdateAssemblies();

	STEPCAFControl_Writer writer;
	writer.SetColorMode(true);
	writer.SetNameMode(true);
	std::ostringstream stream;
	const bool written = writer.Transfer(document, STEPControl_AsIs)
			&& writer.WriteStream(stream) == IFSelect_RetDone;
	application->Close(document);
	if (!written) {
		if (error) *error = tr("The cabinet could not be written as STEP.");
		return false;
	}
	QFile file(path);
	const std::string bytes = stream.str();
	if (!file.open(QIODevice::WriteOnly)
		|| file.write(bytes.data(), qint64(bytes.size())) != qint64(bytes.size())) {
		if (error) *error = tr("%1 cannot be written: %2").arg(path, file.errorString());
		return false;
	}
	return true;
#else
	Q_UNUSED(solids)
	Q_UNUSED(path)
	Q_UNUSED(models)
	if (error) *error = tr("FaseCAD was built without OpenCASCADE: it cannot write STEP files.");
	return false;
#endif
}
}
