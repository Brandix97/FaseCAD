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
#include "projectversions.h"

#include "diagram.h"
#include "diagramcontext.h"
#include "qetapp.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/independenttextitem.h"
#include "qetgraphicsitem/terminal.h"
#include "qetinformation.h"
#include "qetproject.h"
#include "titleblockproperties.h"

#include <QDateTime>
#include <QSet>

namespace {
	QString tr(const char *text, const char *comment = nullptr, int n = -1)
	{
		return QCoreApplication::translate("ProjectVersions", text, comment, n);
	}

	bool isReport(const Element *element)
	{
		return element->linkType() == Element::NextReport
				|| element->linkType() == Element::PreviousReport;
	}

	QString nameOf(const Terminal *terminal)
	{
		const QString name = terminal->name().trimmed();
		return name.isEmpty() ? QStringLiteral("?") : name;
	}

		/// Lines a folio of the changes holds
	const int lines_per_sheet = 46;
}

bool ProjectVersions::History::operator==(const History &other) const
{
	if (versions.size() != other.versions.size() || last != other.last) {
		return false;
	}
	for (int i = 0 ; i < versions.size() ; ++i) {
		if (versions.at(i).number != other.versions.at(i).number
			|| versions.at(i).date != other.versions.at(i).date) {
			return false;
		}
	}
	return true;
}

ProjectVersions::Snapshot ProjectVersions::snapshot(QETProject *project)
{
	Snapshot devices;
	if (!project) {
		return devices;
	}
	for (Diagram *folio : project->diagrams())
	{
		if (folio->isCabinetFolio() || folio->isRoutingFolio()) {
			continue;
		}
		folio->ensureLoaded();
		for (Element *element : folio->elements())
		{
			if (isReport(element)) {
				continue;
			}
			const QString label = element->actualLabel().trimmed();
			if (label.isEmpty()) {
				continue;
			}
			Device &device = devices[label];
			if (device.description.isEmpty()) {
				device.description = element->elementInformations()
						.value(QETInformation::ELMT_DESCRIPTION).toString().trimmed();
			}
			for (Terminal *terminal : element->terminals())
			{
				QSet<QString> joined;
				for (Conductor *conductor : terminal->conductors())
				{
					QSet<Conductor *> potential = conductor->relatedPotentialConductors(true);
					potential.insert(conductor);
					for (Conductor *wire : std::as_const(potential))
					{
						for (Terminal *end : {wire->terminal1, wire->terminal2})
						{
							if (!end || end == terminal) {
								continue;
							}
							Element *other = end->parentElement();
							if (!other || isReport(other)) {
								continue;
							}
							const QString other_label = other->actualLabel().trimmed();
							if (other_label.isEmpty()) {
								continue;
							}
							joined.insert(QStringLiteral("%1 → %2:%3")
										  .arg(nameOf(terminal), other_label, nameOf(end)));
						}
					}
				}
				device.wiring += QStringList(joined.cbegin(), joined.cend());
			}
		}
	}
	for (Device &device : devices) {
		device.wiring.removeDuplicates();
		device.wiring.sort();
	}
	return devices;
}

ProjectVersions::Changes ProjectVersions::compare(const Snapshot &before, const Snapshot &after)
{
	Changes changes;
	for (auto it = after.cbegin() ; it != after.cend() ; ++it) {
		if (!before.contains(it.key())) {
			changes.added << it.key();
			continue;
		}
		const QStringList old_wiring = before.value(it.key()).wiring;
		const QSet<QString> was(old_wiring.cbegin(), old_wiring.cend());
		const QSet<QString> now(it->wiring.cbegin(), it->wiring.cend());
		QStringList lines;
		for (const QString &line : old_wiring) {
			if (!now.contains(line)) lines << QStringLiteral("− ") + line;
		}
		for (const QString &line : it->wiring) {
			if (!was.contains(line)) lines << QStringLiteral("+ ") + line;
		}
		if (!lines.isEmpty()) {
			changes.rewired.insert(it.key(), lines);
		}
	}
	for (auto it = before.cbegin() ; it != before.cend() ; ++it) {
		if (!after.contains(it.key())) {
			changes.removed << it.key();
		}
	}
	return changes;
}

QStringList ProjectVersions::report(const Changes &changes, const Snapshot &before,
									const Snapshot &after, int from, int to)
{
	QStringList lines;
	lines << tr("Changes from version %1 to version %2").arg(from).arg(to)
		  << QLocale().toString(QDateTime::currentDateTime(), QLocale::ShortFormat)
		  << QString();
	if (changes.isEmpty()) {
		lines << tr("No device was added, removed or rewired.");
		return lines;
	}
	auto device = [](const QString &label, const Snapshot &devices) {
		const QString description = devices.value(label).description;
		return description.isEmpty() ? QStringLiteral("  ") + label
									 : QStringLiteral("  %1   %2").arg(label, description);
	};
	if (!changes.added.isEmpty()) {
		lines << tr("Devices added (%n)", nullptr, int(changes.added.size()));
		for (const QString &label : changes.added) lines << device(label, after);
		lines << QString();
	}
	if (!changes.removed.isEmpty()) {
		lines << tr("Devices removed (%n)", nullptr, int(changes.removed.size()));
		for (const QString &label : changes.removed) lines << device(label, before);
		lines << QString();
	}
	if (!changes.rewired.isEmpty()) {
		lines << tr("Wiring changed (%n)", nullptr, int(changes.rewired.size()));
		for (auto it = changes.rewired.cbegin() ; it != changes.rewired.cend() ; ++it) {
			lines << device(it.key(), after);
			for (const QString &line : it.value()) lines << QStringLiteral("      ") + line;
		}
	}
	while (!lines.isEmpty() && lines.last().isEmpty()) lines.removeLast();
	return lines;
}

QDomElement ProjectVersions::toXml(const History &history, QDomDocument &document)
{
	QDomElement root = document.createElement(QStringLiteral("project_versions"));
	for (const Version &version : history.versions) {
		QDomElement each = document.createElement(QStringLiteral("version"));
		each.setAttribute(QStringLiteral("number"), version.number);
		each.setAttribute(QStringLiteral("date"), version.date);
		root.appendChild(each);
	}
	QDomElement last = document.createElement(QStringLiteral("devices"));
	for (auto it = history.last.cbegin() ; it != history.last.cend() ; ++it) {
		QDomElement device = document.createElement(QStringLiteral("device"));
		device.setAttribute(QStringLiteral("label"), it.key());
		if (!it->description.isEmpty()) {
			device.setAttribute(QStringLiteral("description"), it->description);
		}
		for (const QString &line : it->wiring) {
			QDomElement wire = document.createElement(QStringLiteral("wire"));
			wire.appendChild(document.createTextNode(line));
			device.appendChild(wire);
		}
		last.appendChild(device);
	}
	root.appendChild(last);
	return root;
}

ProjectVersions::History ProjectVersions::fromXml(const QDomElement &element)
{
	History history;
	if (element.isNull()) {
		return history;
	}
	for (QDomElement each = element.firstChildElement(QStringLiteral("version")) ; !each.isNull() ;
		 each = each.nextSiblingElement(QStringLiteral("version"))) {
		Version version;
		version.number = each.attribute(QStringLiteral("number")).toInt();
		version.date = each.attribute(QStringLiteral("date"));
		if (version.number > 0) {
			history.versions << version;
		}
	}
	const QDomElement last = element.firstChildElement(QStringLiteral("devices"));
	for (QDomElement device = last.firstChildElement(QStringLiteral("device")) ; !device.isNull() ;
		 device = device.nextSiblingElement(QStringLiteral("device"))) {
		Device each;
		each.description = device.attribute(QStringLiteral("description"));
		for (QDomElement wire = device.firstChildElement(QStringLiteral("wire")) ; !wire.isNull() ;
			 wire = wire.nextSiblingElement(QStringLiteral("wire"))) {
			each.wiring << wire.text();
		}
		history.last.insert(device.attribute(QStringLiteral("label")), each);
	}
	return history;
}

QList<Diagram *> ProjectVersions::setVersion(QETProject *project)
{
	QList<Diagram *> sheets;
	if (!project) {
		return sheets;
	}
	History history = project->versionHistory();
	const Snapshot now = snapshot(project);
		//The version is the revision index of the title block (Rev index,
		//%indexrev; its "version" is the program's): 1 when it has none
		//or no number, else one more (Brandon, 2026-11-27)
	TitleBlockProperties defaults = project->defaultTitleBlockProperties();
	bool number = false;
	const int written = defaults.indexrev.trimmed().toInt(&number);
	const int to = number && written >= 0 ? written + 1 : 1;
	const int from = history.current();
		//The changes since the version kept last: none the first time
	if (!history.versions.isEmpty()) {
		const QStringList lines = report(compare(history.last, now), history.last, now, from, to);
		const int count = int((lines.size() + lines_per_sheet - 1) / lines_per_sheet);
		for (int index = 0 ; index < count ; ++index) {
			Diagram *sheet = project->addNewDiagram();
			if (!sheet) {
				break;
			}
			TitleBlockProperties properties = sheet->border_and_titleblock.exportTitleBlock();
			properties.title = count > 1
					? tr("Changes of version %1 (%2/%3)").arg(to).arg(index + 1).arg(count)
					: tr("Changes of version %1").arg(to);
			sheet->border_and_titleblock.importTitleBlock(properties);
			auto *text = new IndependentTextItem();
			text->setFont(QETApp::diagramTextsFont(9));
			text->setPlainText(lines.mid(index * lines_per_sheet, lines_per_sheet).join(QLatin1Char('\n')));
			sheet->addItem(text);
			text->setPos(sheet->border_and_titleblock.insideBorderRect().topLeft() + QPointF(20, 20));
			sheets << sheet;
		}
	}
		//The new version in the title block of the project and of every
		//folio
	defaults.indexrev = QString::number(to);
	project->setDefaultTitleBlockProperties(defaults);
	for (Diagram *folio : project->diagrams()) {
		folio->ensureLoaded();
		TitleBlockProperties properties = folio->border_and_titleblock.exportTitleBlock();
		if (properties.indexrev != defaults.indexrev) {
			properties.indexrev = defaults.indexrev;
			folio->border_and_titleblock.importTitleBlock(properties);
		}
	}
	Version version;
	version.number = to;
	version.date = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
	history.versions << version;
	history.last = now;
	project->setVersionHistory(history);
		//Title blocks show it as %{project_version} too
	DiagramContext properties = project->projectProperties();
	properties.addValue(QStringLiteral("project_version"), QString::number(to));
	project->setProjectProperties(properties);
	return sheets;
}
