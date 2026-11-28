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
#include "summarysymbols.h"

#include "ElementsCollection/elementslocation.h"
#include "dataBase/projectdatabase.h"
#include "devicexrefs.h"
#include "diagram.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/independenttextitem.h"
#include "qetgraphicsitem/terminal.h"
#include "qetinformation.h"
#include "qetproject.h"

#include <QDomDocument>

#include <QDomElement>
#include <QSqlQuery>
#include <QRegularExpression>
#include <QCollator>

#include <algorithm>
#include <cmath>
#include <QUuid>

namespace {

	/// A terminal of the schematic a summary symbol can stand for
struct Target
{
	DeviceXRefs::Part place;
	bool is_main = false;
};

QString key(const QString &label, const QString &pin)
{
	return label.trimmed().toLower() + QLatin1Char('\n') + pin.trimmed().toLower();
}

bool isSummary(const Element *element)
{
	return element->linkType() == Element::PlcSummary
			|| element->linkType() == Element::NetworkSummary;
}

	/// The names of the terminals of the symbol @p definition (its
	/// location in @p project), read once from its definition: a terminal
	/// without a wire is no row of the database
QStringList terminalNames(QETProject *project, const QString &definition,
						  QHash<QString, QStringList> &cache)
{
	const auto found = cache.constFind(definition);
	if (found != cache.constEnd()) {
		return found.value();
	}
	QStringList names;
	const ElementsLocation location(definition, project);
	const QDomElement xml = location.exist() ? location.xml() : QDomElement();
	const QDomElement description = xml.firstChildElement(QStringLiteral("description"));
	for (QDomElement e = description.firstChildElement(QStringLiteral("terminal")) ; !e.isNull() ;
		 e = e.nextSiblingElement(QStringLiteral("terminal"))) {
		const QString name = e.attribute(QStringLiteral("name")).trimmed();
		if (!name.isEmpty()) {
			names << name;
		}
	}
	cache.insert(definition, names);
	return names;
}

	/// The terminals of the symbols of @p project that are no summary
	/// symbols, by label and terminal name; and by label alone (empty
	/// terminal), the first symbol of the label
QHash<QString, Target> targets(QETProject *project)
{
	QHash<QString, Target> result;
	if (!project->dataBase()) {
		return result;
	}
	QSqlQuery query = project->dataBase()->newQuery(QStringLiteral(
		"SELECT ei.label, e.definition, e.uuid, e.diagram_uuid, d.pos, di.folio, e.pos, e.type "
		"FROM element e "
		"JOIN diagram d ON d.uuid = e.diagram_uuid "
		"LEFT JOIN diagram_info di ON di.diagram_uuid = d.uuid "
		"LEFT JOIN element_info ei ON ei.element_uuid = e.uuid "
		"WHERE e.type NOT IN ('plc_summary', 'network_summary', 'next_report', "
		"'previous_report', 'thumbnail', 'conductor_definition') "
		"ORDER BY d.pos"));
	if (!query.exec()) {
		return result;
	}
	QHash<QString, QStringList> names_of;
	while (query.next()) {
		const QString label = query.value(0).toString().trimmed();
		if (label.isEmpty()) {
			continue;
		}
		Target target;
		target.place.element = QUuid(query.value(2).toString());
		target.place.folio = QUuid(query.value(3).toString());
		target.place.folio_index = query.value(4).toInt() - 1;
		target.place.folio_label = query.value(5).toString().trimmed();
		target.place.cell = query.value(6).toString().trimmed();
		for (const QString &pin : terminalNames(project, query.value(1).toString(), names_of)) {
			if (!result.contains(key(label, pin))) {
				result.insert(key(label, pin), target);
			}
		}
			//The device itself: its main part first, else its first symbol
		const bool main = query.value(7).toString() != QLatin1String("slave");
		const QString device = key(label, QString());
		if (!result.contains(device) || (main && !result.value(device).is_main)) {
			target.is_main = main;
			result.insert(device, target);
		}
	}
	return result;
}

Element *elementOf(QETProject *project, const DeviceXRefs::Part &place)
{
	Diagram *folio = project->diagramByUuid(place.folio);
	if (!folio) {
		return nullptr;
	}
	if (!folio->isLoaded()) {
		project->buildFolios({folio});
	}
	for (Element *element : folio->elements()) {
		if (element->uuid() == place.element) {
			return element;
		}
	}
	return nullptr;
}

}

QString SummarySymbols::functionTextAt(Diagram *folio, const QRectF &rect)
{
	if (!folio) {
		return QString();
	}
	const qreal x = rect.center().x();
	QString found;
	qreal nearest = 0.0;
	for (QGraphicsItem *item : folio->items()) {
		auto *text = qgraphicsitem_cast<IndependentTextItem *>(item);
		if (!text || !text->isFunctionText()) {
			continue;
		}
		const QRectF span = text->sceneBoundingRect();
		if (x < span.left() || x > span.right()) {
			continue;
		}
		const qreal distance = rect.bottom() < span.top() ? span.top() - rect.bottom()
							 : rect.top() > span.bottom() ? rect.top() - span.bottom()
							 : 0.0;
		if (found.isNull() || distance < nearest) {
			found = text->toPlainText().simplified();
			nearest = distance;
		}
	}
	return found;
}

void SummarySymbols::refresh(QETProject *project)
{
	if (!project) {
		return;
	}
	const QList<Diagram *> built = project->builtFolios();

		//The function of each symbol from the function texts over it
	QHash<Element *, QHash<QString, QString>> computed;
	for (Diagram *folio : built) {
		bool has_function_texts = false;
		for (QGraphicsItem *item : folio->items()) {
			auto *text = qgraphicsitem_cast<IndependentTextItem *>(item);
			has_function_texts = has_function_texts || (text && text->isFunctionText());
		}
		for (Element *element : folio->elements()) {
			QHash<QString, QString> values;
			if (has_function_texts && !isSummary(element)) {
				const QString function = functionTextAt(folio, element->sceneBoundingRect());
				if (!function.isEmpty()) {
					values.insert(QETInformation::ELMT_FUNCTION, function);
				}
			}
			computed.insert(element, values);
		}
	}

		//What each summary symbol stands for
	QList<Element *> summaries;
	for (auto it = computed.constBegin() ; it != computed.constEnd() ; ++it) {
		if (isSummary(it.key())) summaries << it.key();
	}
	if (!summaries.isEmpty()) {
		const QHash<QString, Target> all = targets(project);
		QString formula = project->defaultXRefProperties(QStringLiteral("coil")).masterLabel();
		if (formula.isEmpty()) formula = QStringLiteral("%f-%l%c");
			//The place and the function of the terminal @p pin of the
			//device @p label, under the keys @p reference_key and
			//@p function_key
		auto show = [&](QHash<QString, QString> &values, const QString &label, const QString &pin,
						const QString &reference_key, const QString &function_key) {
			const auto found = all.constFind(key(label, pin));
			if (label.trimmed().isEmpty() || found == all.constEnd()) {
				values.insert(reference_key, QStringLiteral("?"));
				return;
			}
			DeviceXRefs::Part place = found->place;
			QString function;
			if (Element *target = elementOf(project, found->place)) {
					//The terminal itself: its column, the function text
					//over or under it (a device of eight inputs has eight)
				QRectF at = target->sceneBoundingRect();
				for (Terminal *terminal : target->terminals()) {
					if (!pin.trimmed().isEmpty()
						&& terminal->name().trimmed().compare(pin.trimmed(), Qt::CaseInsensitive) == 0) {
						const QPointF point = terminal->dockConductor();
						at = QRectF(point - QPointF(1, 1), QSizeF(2, 2));
						if (target->diagram()) {
							place.cell = target->diagram()->convertPosition(point).toString();
						}
						break;
					}
				}
				function = target->elementInformations()
						.value(QETInformation::ELMT_FUNCTION).toString().trimmed();
				if (function.isEmpty()) {
					function = functionTextAt(target->diagram(), at);
				}
			}
			values.insert(reference_key, DeviceXRefs::placeText(formula, place));
			values.insert(function_key, function);
		};
		for (Element *summary : std::as_const(summaries)) {
			const DiagramContext information = summary->elementInformations();
			const QString label = information.value(QETInformation::ELMT_LABEL).toString();
			QHash<QString, QString> &values = computed[summary];
				//The terminal named in its information
			show(values, label, information.value(QETInformation::ELMT_SUMMARY_PIN).toString(),
				 QETInformation::ELMT_SUMMARY_REFERENCE, QETInformation::ELMT_SUMMARY_FUNCTION);
				//Each terminal of the symbol: the one of the same name
				//Numbered as they stand: from the top down, then from the
				//left to the right
			QList<Terminal *> terminals = summary->terminals();
			std::stable_sort(terminals.begin(), terminals.end(), [](Terminal *a, Terminal *b) {
				const QPointF pa = a->dockConductor(), pb = b->dockConductor();
				if (std::abs(pa.y() - pb.y()) > 0.5) return pa.y() < pb.y();
				return pa.x() < pb.x();
			});
			for (int i = 0 ; i < terminals.size() ; ++i) {
				const QString name = terminals.at(i)->name();
				if (name.trimmed().isEmpty()) {
					continue;
				}
				values.insert(QETInformation::summaryInfoKey(QETInformation::ELMT_SUMMARY_PIN, i + 1), name);
				show(values, label, name,
					 QETInformation::summaryInfoKey(QETInformation::ELMT_SUMMARY_REFERENCE, i + 1),
					 QETInformation::summaryInfoKey(QETInformation::ELMT_SUMMARY_FUNCTION, i + 1));
					//And by its name, wherever it stands
				values.insert(QETInformation::summaryInfoKeyFor(QETInformation::ELMT_SUMMARY_PIN, name), name);
				values.insert(QETInformation::summaryInfoKeyFor(QETInformation::ELMT_SUMMARY_REFERENCE, name),
							  values.value(QETInformation::summaryInfoKey(QETInformation::ELMT_SUMMARY_REFERENCE, i + 1)));
				values.insert(QETInformation::summaryInfoKeyFor(QETInformation::ELMT_SUMMARY_FUNCTION, name),
							  values.value(QETInformation::summaryInfoKey(QETInformation::ELMT_SUMMARY_FUNCTION, i + 1)));
			}
		}
	}

	for (auto it = computed.constBegin() ; it != computed.constEnd() ; ++it) {
		it.key()->setComputedInfo(it.value());
	}
}

QStringList SummarySymbols::expandPattern(const QString &pattern)
{
	QStringList names;
	static const QRegularExpression number(QStringLiteral("(\\d+)"));
	const QStringList items = pattern.split(QLatin1Char(','), Qt::SkipEmptyParts);
	for (QString item : items) {
		item = item.trimmed();
			//A range: "first-last", the two alike but for their numbers
		const QStringList ends = item.split(QLatin1Char('-'));
		if (ends.size() != 2) {
			if (!item.isEmpty()) names << item;
			continue;
		}
		const QString first = ends.at(0).trimmed(), last = ends.at(1).trimmed();
		QStringList first_parts, last_parts;
		QList<int> first_numbers, last_numbers;
		auto split = [](const QString &text, QStringList &parts, QList<int> &numbers) {
			int at = 0;
			QRegularExpressionMatchIterator it = number.globalMatch(text);
			while (it.hasNext()) {
				const QRegularExpressionMatch match = it.next();
				parts << text.mid(at, match.capturedStart() - at);
				numbers << match.captured(1).toInt();
				at = match.capturedEnd();
			}
			parts << text.mid(at);
		};
		split(first, first_parts, first_numbers);
		split(last, last_parts, last_numbers);
		if (first_parts != last_parts || first_numbers.isEmpty()) {
			names << first << last;
			continue;
		}
			//Each number from its first value to its last one, the last
			//number turning fastest: I0.0-I3.7 is I0.0..I0.7, I1.0..I3.7
		QList<int> current = first_numbers;
		const int fields = int(current.size());
		for (int guard = 0 ; guard < 10000 ; ++guard) {
			QString name;
			for (int f = 0 ; f < fields ; ++f) {
				name += first_parts.at(f) + QString::number(current.at(f));
			}
			name += first_parts.last();
			names << name;
			int f = fields - 1;
			while (f >= 0) {
				if (current.at(f) < last_numbers.at(f)) {
					++current[f];
					for (int g = f + 1 ; g < fields ; ++g) current[g] = first_numbers.at(g);
					break;
				}
				--f;
			}
			if (f < 0) break;
		}
	}
	return names;
}

QStringList SummarySymbols::deviceLabels(QETProject *project)
{
	QStringList labels;
	if (!project || !project->dataBase()) {
		return labels;
	}
	QSqlQuery query = project->dataBase()->newQuery(QStringLiteral(
		"SELECT DISTINCT TRIM(ei.label) FROM element e "
		"JOIN element_info ei ON ei.element_uuid = e.uuid "
		"WHERE e.type IN ('simple', 'master', 'terminal') AND TRIM(ei.label) <> ''"));
	if (query.exec()) {
		while (query.next()) labels << query.value(0).toString();
	}
	QCollator collator;
	collator.setNumericMode(true);
	std::sort(labels.begin(), labels.end(), [&collator](const QString &a, const QString &b) {
		return collator.compare(a, b) < 0;
	});
	return labels;
}

QStringList SummarySymbols::deviceTerminals(QETProject *project, const QString &label)
{
	QStringList names;
	if (!project || !project->dataBase()) {
		return names;
	}
	QSqlQuery query = project->dataBase()->newQuery();
	query.prepare(QStringLiteral(
		"SELECT e.definition FROM element e "
		"JOIN element_info ei ON ei.element_uuid = e.uuid "
		"WHERE TRIM(ei.label) = :label "
		"AND e.type NOT IN ('plc_summary', 'network_summary', 'next_report', 'previous_report')"));
	query.bindValue(QStringLiteral(":label"), label.trimmed());
	QHash<QString, QStringList> cache;
	if (query.exec()) {
		while (query.next()) {
			for (const QString &name : terminalNames(project, query.value(0).toString(), cache)) {
				if (!names.contains(name)) names << name;
			}
		}
	}
	QCollator collator;
	collator.setNumericMode(true);
	std::sort(names.begin(), names.end(), [&collator](const QString &a, const QString &b) {
		return collator.compare(a, b) < 0;
	});
	return names;
}

QDomDocument SummarySymbols::plcSummaryDefinition(const QStringList &pins, bool two_columns)
{
		//A box for the device, its label over it, a terminal for each pin
		//on its left (and, in two columns, the second half on its right);
		//beside each, its place and its function text, as a PLC summary
		//draws them. The texts follow their terminal by its name.
	const QString style = QStringLiteral("line-style:normal;line-weight:normal;filling:none;color:black");
	const QString font = QStringLiteral("Liberation Sans,8,-1,5,50,0,0,0,0,0,Regular");
	auto text = [&](const QString &info, qreal x, qreal y, qreal width, const QString &align) {
		return QStringLiteral(
			"<dynamic_text Halignment=\"%1\" Valignment=\"AlignTop\" font=\"%2\" frame=\"false\" "
			"keep_visual_rotation=\"false\" rotation=\"0\" rotation_point_center=\"false\" "
			"text_from=\"ElementInfo\" text_width=\"%3\" uuid=\"%4\" x=\"%5\" y=\"%6\" z=\"5\">"
			"<text></text><info_name>%7</info_name></dynamic_text>")
				.arg(align, font).arg(width).arg(QUuid::createUuid().toString())
				.arg(x).arg(y).arg(info.toHtmlEscaped());
	};
	const int left_count = two_columns ? int((pins.size() + 1) / 2) : int(pins.size());
	const int rows = qMax(1, left_count);
	const qreal row = 20, box_width = two_columns ? 80 : 60;
	const qreal box_height = row * (rows + 1);
	QString parts = QStringLiteral(
		"<rect x=\"0\" y=\"0\" width=\"%1\" height=\"%2\" rx=\"0\" ry=\"0\" style=\"%3\" antialias=\"false\"/>")
			.arg(box_width).arg(box_height).arg(style);
	parts += text(QStringLiteral("label"), 0, -20, -1, QStringLiteral("AlignLeft"));
	for (int i = 0 ; i < pins.size() ; ++i) {
		const QString &pin = pins.at(i);
		const bool right = two_columns && i >= left_count;
		const qreal y = row * ((right ? i - left_count : i) + 1);
		const QString pin_key = QETInformation::summaryInfoKeyFor(QETInformation::ELMT_SUMMARY_PIN, pin);
		const QString reference_key = QETInformation::summaryInfoKeyFor(QETInformation::ELMT_SUMMARY_REFERENCE, pin);
		const QString function_key = QETInformation::summaryInfoKeyFor(QETInformation::ELMT_SUMMARY_FUNCTION, pin);
		parts += QStringLiteral(
			"<terminal uuid=\"%1\" name=\"%2\" x=\"%3\" y=\"%4\" orientation=\"%5\" type=\"Generic\"/>")
				.arg(QUuid::createUuid().toString(), pin.toHtmlEscaped())
				.arg(right ? box_width + 3 : -3).arg(y)
				.arg(right ? QStringLiteral("e") : QStringLiteral("w"));
		if (right) {
			parts += text(pin_key, box_width - 38, y - 7, 35, QStringLiteral("AlignRight"));
			parts += text(reference_key, box_width + 8, y - 7, 52, QStringLiteral("AlignLeft"));
			parts += text(function_key, box_width + 65, y - 7, 140, QStringLiteral("AlignLeft"));
		} else {
			parts += text(pin_key, 3, y - 7, -1, QStringLiteral("AlignLeft"));
			parts += text(reference_key, -60, y - 7, 52, QStringLiteral("AlignRight"));
			parts += text(function_key, -205, y - 7, 140, QStringLiteral("AlignRight"));
		}
	}
	const qreal right_extent = two_columns ? box_width + 210 : box_width + 5;
	const QString xml = QStringLiteral(
		"<definition type=\"element\" link_type=\"plc_summary\" width=\"%1\" height=\"%2\" "
		"hotspot_x=\"210\" hotspot_y=\"30\" version=\"0.200.1\">"
		"<uuid uuid=\"%3\"/>"
		"<names><name lang=\"en\">PLC summary</name><name lang=\"es\">Resumen PLC</name></names>"
		"<kindInformations/><elementInformations/>"
		"<informations>PLC summary symbol: each terminal shows the terminal of the same name of "
		"the device of its label.</informations>"
		"<description>%4</description></definition>")
			.arg(int(210 + right_extent)).arg(int(box_height + 40))
			.arg(QUuid::createUuid().toString(), parts);
	QDomDocument document;
	document.setContent(xml);
	return document;
}

QDomDocument SummarySymbols::plcExampleDefinition()
{
	return plcSummaryDefinition({QStringLiteral("X1:1"), QStringLiteral("X1:2"),
								 QStringLiteral("X1:3"), QStringLiteral("X1:4")}, false);
}
