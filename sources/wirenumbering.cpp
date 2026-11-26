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
#include "wirenumbering.h"

#include "QPropertyUndoCommand/qpropertyundocommand.h"
#include "conductorproperties.h"
#include "diagram.h"
#include "diagramposition.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminal.h"
#include "qetproject.h"

#include <QHash>
#include <QSet>
#include <QSettings>
#include <QUndoCommand>

#include <algorithm>

namespace
{
	QString tr(const char *text, const char *comment = nullptr, int n = -1)
	{
		return QCoreApplication::translate("WireNumbering", text, comment, n);
	}

		/// One wire, where it starts on its sheet
	struct Wire
	{
		Conductor *conductor = nullptr;
		Diagram *sheet = nullptr;
		int sheet_index = 0;
		QPointF start;
		Terminal *from = nullptr;
		Terminal *to = nullptr;
	};

		/// The name a potential type is numbered with
	QString prefixOf(const QString &type)
	{
		if (type == QLatin1String("24V")) return QStringLiteral("P");
		if (type == QLatin1String("0V")) return QStringLiteral("M");
		return type;
	}

		/// A potential type which, alone in the project, needs no number
	bool aloneUnnumbered(const QString &type)
	{
		return type == QLatin1String("N") || type == QLatin1String("PE");
	}

	bool before(const QPointF &a, const QPointF &b, WireNumbering::Order order)
	{
		if (order == WireNumbering::Order::RowsFirst) {
			return !qFuzzyCompare(a.y(), b.y()) ? a.y() < b.y() : a.x() < b.x();
		}
		return !qFuzzyCompare(a.x(), b.x()) ? a.x() < b.x() : a.y() < b.y();
	}

	QString endOf(const Terminal *terminal)
	{
		if (!terminal || !terminal->parentElement()) {
			return QString();
		}
		const QString label = terminal->parentElement()->actualLabel();
		const QString name = terminal->name();
		return name.isEmpty() ? label : label + QLatin1Char(':') + name;
	}

	const QString settings_key = QStringLiteral("wirenumbering/own");
}

QList<WireNumbering::Scheme> WireNumbering::examples()
{
	return {
		{tr("By potential: 1, 2, 3…"), QStringLiteral("%N"), false},
		{tr("Sheet and number: 3.1, 3.2…"), QStringLiteral("%F.%n"), false},
		{tr("Sheet and number: 301, 302…"), QStringLiteral("%F%2n"), false},
		{tr("Sheet and column: 3-12"), QStringLiteral("%F-%C"), false},
		{tr("From and to, IEC 62491: -K1:13/-X1:4"), QStringLiteral("%S:%T/%D:%E"), true},
		{tr("By potential type: L1, L2, N, PE, P1, M1"), QStringLiteral("%P"), false}
	};
}

QList<WireNumbering::Scheme> WireNumbering::ownSchemes()
{
	QList<Scheme> schemes;
	QSettings settings;
	const int count = settings.beginReadArray(settings_key);
	for (int i = 0 ; i < count ; ++i) {
		settings.setArrayIndex(i);
		Scheme scheme;
		scheme.name = settings.value(QStringLiteral("name")).toString();
		scheme.formula = settings.value(QStringLiteral("formula")).toString();
		scheme.per_wire = settings.value(QStringLiteral("per_wire")).toBool();
		if (!scheme.name.isEmpty() && !scheme.formula.isEmpty()) {
			schemes << scheme;
		}
	}
	settings.endArray();
	return schemes;
}

void WireNumbering::setOwnSchemes(const QList<Scheme> &schemes)
{
	QSettings settings;
	settings.remove(settings_key);
	settings.beginWriteArray(settings_key, schemes.size());
	for (int i = 0 ; i < schemes.size() ; ++i) {
		settings.setArrayIndex(i);
		settings.setValue(QStringLiteral("name"), schemes.at(i).name);
		settings.setValue(QStringLiteral("formula"), schemes.at(i).formula);
		settings.setValue(QStringLiteral("per_wire"), schemes.at(i).per_wire);
	}
	settings.endArray();
}

QString WireNumbering::evaluate(const QString &formula, const QHash<QChar, QString> &values,
								int number, int sheet_number)
{
	QString result;
	for (int i = 0 ; i < formula.size() ; ++i)
	{
		const QChar c = formula.at(i);
		if (c != QLatin1Char('%')) {
			result += c;
			continue;
		}
		int j = i + 1;
		int width = 0;
		while (j < formula.size() && formula.at(j).isDigit()) {
			width = width * 10 + formula.at(j).digitValue();
			++j;
		}
		if (j >= formula.size()) {
			result += formula.mid(i);
			break;
		}
		const QChar variable = formula.at(j);
		QString text;
		if (variable == QLatin1Char('%')) {
			text = QStringLiteral("%");
		} else if (variable == QLatin1Char('N')) {
			text = QString::number(number);
		} else if (variable == QLatin1Char('n')) {
			text = QString::number(sheet_number);
		} else if (values.contains(variable)) {
			text = values.value(variable);
		} else {
				//Not a variable: as it is written
			result += formula.mid(i, j - i + 1);
			i = j;
			continue;
		}
		if (width > 0 && (variable == QLatin1Char('N') || variable == QLatin1Char('n')
						  || variable == QLatin1Char('C'))) {
			text = text.rightJustified(width, QLatin1Char('0'));
		}
		result += text;
		i = j;
	}
	return result;
}

/**
	@brief WireNumbering::plan
	@return the names numbering the wires of @p project with @p options
	would give, one per potential (or per wire), in the order the wires
	stand on the sheets; only the names which change
*/
QList<WireNumbering::Change> WireNumbering::plan(QETProject *project, const Options &options)
{
	QList<Change> changes;
	if (!project) {
		return changes;
	}

		//The sheets
	QList<Diagram *> sheets;
	if (options.scope == Scope::Sheet) {
		if (options.sheet) {
			sheets << options.sheet;
		}
	} else if (options.scope == Scope::Selection) {
		for (Conductor *conductor : options.selection) {
			if (conductor && conductor->diagram() && !sheets.contains(conductor->diagram())) {
				sheets << conductor->diagram();
			}
		}
	} else {
		for (Diagram *sheet : project->diagrams()) {
			if (!sheet->isCabinetFolio() && !sheet->isRoutingFolio()) {
				sheets << sheet;
			}
		}
	}
	for (Diagram *sheet : std::as_const(sheets)) {
		sheet->ensureLoaded();
	}

		//The wires, in the order they stand
	QList<Wire> wires;
	for (Diagram *sheet : std::as_const(sheets))
	{
		const int index = project->folioIndex(sheet);
		for (Conductor *conductor : sheet->conductors())
		{
			if (options.scope == Scope::Selection && !options.selection.contains(conductor)) {
				continue;
			}
			if (!conductor->terminal1 || !conductor->terminal2) {
				continue;
			}
			Wire wire;
			wire.conductor = conductor;
			wire.sheet = sheet;
			wire.sheet_index = index;
			const QPointF a = conductor->terminal1->dockConductor();
			const QPointF b = conductor->terminal2->dockConductor();
			const bool a_first = before(a, b, options.order) || a == b;
			wire.start = a_first ? a : b;
			wire.from = a_first ? conductor->terminal1 : conductor->terminal2;
			wire.to = a_first ? conductor->terminal2 : conductor->terminal1;
			wires << wire;
		}
	}
	std::stable_sort(wires.begin(), wires.end(), [&options](const Wire &a, const Wire &b) {
		if (a.sheet_index != b.sheet_index) {
			return a.sheet_index < b.sheet_index;
		}
		return before(a.start, b.start, options.order);
	});

		//What gets one name: a potential, or a wire
	struct Group
	{
		QList<Conductor *> conductors;
		Wire first;
		bool fixed = false;
	};
	QList<Group> groups;
	QSet<Conductor *> grouped;
	for (const Wire &wire : std::as_const(wires))
	{
		if (grouped.contains(wire.conductor)) {
			continue;
		}
		Group group;
		group.first = wire;
		group.conductors << wire.conductor;
		if (!options.per_wire) {
			for (Conductor *other : wire.conductor->relatedPotentialConductors(true)) {
				if (!group.conductors.contains(other)) {
					group.conductors << other;
				}
			}
		}
		for (Conductor *conductor : std::as_const(group.conductors)) {
			grouped.insert(conductor);
			if (options.keep_fixed && conductor->isFreezeLabel()) {
				group.fixed = true;
			}
		}
		groups << group;
	}

		//The names kept, and how many potentials of each type are named
	QSet<QString> used;
	QHash<QString, int> type_total;
	for (const Group &group : std::as_const(groups)) {
		if (group.fixed) {
			used.insert(group.first.conductor->properties().text);
		} else {
			++type_total[group.first.conductor->properties().m_potential_type];
		}
	}

	int count = 0;
	QHash<Diagram *, int> sheet_count;
	QHash<QString, int> type_count;
	for (const Group &group : std::as_const(groups))
	{
		if (group.fixed) {
			continue;
		}
		const Wire &wire = group.first;
		const int number = options.start + count++;
		const int sheet_number = options.start + sheet_count[wire.sheet]++;

		QHash<QChar, QString> values;
		QString folio = wire.sheet->border_and_titleblock.finalfolio();
		if (folio.isEmpty()) {
			folio = QString::number(wire.sheet_index + 1);
		}
		values.insert(QLatin1Char('F'), folio);
		DiagramPosition position = wire.sheet->border_and_titleblock.convertPosition(wire.start);
		values.insert(QLatin1Char('C'), QString::number(position.number()));
		values.insert(QLatin1Char('R'), position.letter());
		const QString type = wire.conductor->properties().m_potential_type;
		const int of_type = ++type_count[type];
		if (type.isEmpty()) {
			values.insert(QLatin1Char('P'), QString::number(of_type));
		} else if (aloneUnnumbered(type) && type_total.value(type) == 1) {
			values.insert(QLatin1Char('P'), prefixOf(type));
		} else {
			values.insert(QLatin1Char('P'), prefixOf(type) + QString::number(of_type));
		}
		auto device = [](const Terminal *terminal) {
			return terminal && terminal->parentElement()
					? terminal->parentElement()->actualLabel() : QString();
		};
		values.insert(QLatin1Char('S'), device(wire.from));
		values.insert(QLatin1Char('T'), wire.from ? wire.from->name() : QString());
		values.insert(QLatin1Char('D'), device(wire.to));
		values.insert(QLatin1Char('E'), wire.to ? wire.to->name() : QString());

		const QString base = evaluate(options.formula, values, number, sheet_number);
		if (base.trimmed().isEmpty()) {
			continue;
		}
			//Two potentials never get the same name
		QString name = base;
		for (int k = 2 ; used.contains(name) ; ++k) {
			name = base + QLatin1Char('.') + QString::number(k);
		}
		used.insert(name);

		bool differs = false;
		for (Conductor *conductor : group.conductors) {
			if (conductor->properties().text != name) {
				differs = true;
				break;
			}
		}
		if (!differs) {
			continue;
		}
		Change change;
		for (Conductor *conductor : group.conductors) {
			change.conductors << conductor;
		}
		change.before = wire.conductor->properties().text;
		change.after = name;
		change.sheet = wire.sheet;
		change.from = endOf(wire.from);
		change.to = endOf(wire.to);
		changes << change;
	}
	return changes;
}

QUndoCommand *WireNumbering::command(const QList<Change> &changes)
{
	int wires = 0;
	for (const Change &change : changes) {
		for (const QPointer<Conductor> &conductor : change.conductors) {
			if (conductor && conductor->properties().text != change.after) {
				++wires;
			}
		}
	}
	if (!wires) {
		return nullptr;
	}
	auto *undo = new QUndoCommand(tr("Number %n wire(s)", "", wires));
	for (const Change &change : changes) {
		for (const QPointer<Conductor> &conductor : change.conductors)
		{
			if (!conductor || conductor->properties().text == change.after) {
				continue;
			}
			ConductorProperties properties = conductor->properties();
			QVariant old_value;
			old_value.setValue(properties);
			properties.text = change.after;
			QVariant new_value;
			new_value.setValue(properties);
			new QPropertyUndoCommand(conductor.data(), "properties", old_value, new_value, undo);
		}
	}
	return undo;
}
