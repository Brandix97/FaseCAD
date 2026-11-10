/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "faseterminal.h"

#include "../ElementsCollection/xmlelementcollection.h"
#include "../NameList/nameslist.h"
#include "../TerminalStrip/UndoCommand/addterminalstripcommand.h"
#include "../TerminalStrip/UndoCommand/addterminaltostripcommand.h"
#include "../TerminalStrip/UndoCommand/changeterminallevel.h"
#include "../TerminalStrip/UndoCommand/groupterminalscommand.h"
#include "../TerminalStrip/physicalterminal.h"
#include "../TerminalStrip/realterminal.h"
#include "../TerminalStrip/terminalcolor.h"
#include "../TerminalStrip/terminalstrip.h"
#include "../TerminalStrip/terminalstriplabels.h"
#include "../diagram.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/terminalelement.h"
#include "../qetproject.h"
#include "../undocommand/changeelementdatacommand.h"
#include "../utils/qetutils.h"

#include <QCoreApplication>
#include <QDomDocument>
#include <QFile>
#include <QHash>

#include <algorithm>

namespace {

QString translated(const char *source)
{
	return QCoreApplication::translate("FaseTerminal", source);
}

const QString symbol_dir = QStringLiteral("import/FaseCAD");
const QString symbol_file = QStringLiteral("terminal.elmt");

QString colorSpec(const char *english)
{
	for (const auto &entry : TerminalColor::palette()) {
		if (QLatin1String(entry.english) == QLatin1String(english)) {
			return TerminalColor::specOf(entry);
		}
	}
	return QString();
}

} // namespace

bool FaseTerminal::Settings::operator==(const Settings &other) const
{
	return strip == other.strip && number == other.number
			&& color == other.color && type == other.type
			&& led == other.led && section == other.section
			&& level == other.level;
}

QVector<FaseTerminal::ColorChoice> FaseTerminal::colorChoices()
{
	return {
		{translated("Red"), QStringLiteral("RD"), colorSpec("Red")},
		{translated("Blue"), QStringLiteral("BU"), colorSpec("Blue")},
		{translated("Beige"), QStringLiteral("BG"), colorSpec("Beige")},
		{translated("Green/Yellow"), QStringLiteral("GNYE"), colorSpec("Green/Yellow")},
		{translated("Black"), QStringLiteral("BK"), colorSpec("Black")}};
}

QString FaseTerminal::colorCode(const QString &spec)
{
	for (const ColorChoice &choice : colorChoices()) {
		if (choice.spec.compare(spec, Qt::CaseInsensitive) == 0) {
			return choice.code;
		}
	}
	return QString();
}

QVector<FaseTerminal::TypeChoice> FaseTerminal::typeChoices()
{
	return {
		{translated("Normal"), ElementData::TTGeneric, false},
		{translated("Fuse"), ElementData::TTFuse, false},
		{translated("Sectional"), ElementData::TTSectional, false},
		{translated("Ground (PE)"), ElementData::TTGround, false},
		{translated("Diode"), ElementData::TTDiode, false},
		{translated("LED"), ElementData::TTGeneric, true}};
}

QString FaseTerminal::typeName(ElementData::TerminalType type, bool led)
{
	for (const TypeChoice &choice : typeChoices()) {
		if (choice.type == type && choice.led == led) {
			return choice.name;
		}
	}
	return led ? translated("LED") : translated("Normal");
}

QStringList FaseTerminal::sections()
{
	return {QStringLiteral("0.5"), QStringLiteral("0.75"), QStringLiteral("1"),
			QStringLiteral("1.5"), QStringLiteral("2.5"), QStringLiteral("4"),
			QStringLiteral("6"), QStringLiteral("10"), QStringLiteral("16"),
			QStringLiteral("25"), QStringLiteral("35"), QStringLiteral("50"),
			QStringLiteral("70"), QStringLiteral("95")};
}

QString FaseTerminal::label(const QString &strip, const QString &number)
{
	if (strip.isEmpty()) {
		return number;
	}
	return QStringLiteral("-") + strip + QLatin1Char(':') + number;
}

void FaseTerminal::parseLabel(const QString &label, QString *strip,
							  QString *number)
{
	QString value = label.trimmed();
	const int colon = value.lastIndexOf(QLatin1Char(':'));
	QString name = colon < 0 ? QString() : value.left(colon).trimmed();
	if (name.startsWith(QLatin1Char('-'))) {
		name.remove(0, 1);
	}
	if (strip) {
		*strip = name;
	}
	if (number) {
		*number = colon < 0 ? value : value.mid(colon + 1).trimmed();
	}
}

FaseTerminal::Settings FaseTerminal::settingsOf(const Element *element)
{
	Settings settings;
	if (!element) {
		return settings;
	}
	const ElementData data = element->elementData();
	const DiagramContext &info = data.m_informations;
	parseLabel(info.value(QStringLiteral("label")).toString(),
			   &settings.strip, &settings.number);
		//The strip the terminal is in tells its name better than its label
	if (auto *terminal = qobject_cast<const TerminalElement *>(element)) {
		if (const auto real = terminal->realTerminal()) {
			if (TerminalStrip *strip = real->parentStrip()) {
				settings.strip = strip->name();
			}
			if (real->physicalTerminal()
					&& real->physicalTerminal()->realTerminalCount() > 1) {
				settings.level = real->level() + 1;
			}
		}
	}
	settings.color = info.value(RealTerminal::colorInfoKey()).toString();
	settings.section = info.value(RealTerminal::sectionInfoKey()).toString();
	settings.type = data.terminalType();
	settings.led = data.terminalLed();
	return settings;
}

QString FaseTerminal::inheritedStrip(const Element *element)
{
	if (!element || !element->diagram()) {
		return QString();
	}
	const QPointF here = element->scenePos();
	auto before = [](const QPointF &a, const QPointF &b) {
		if (!qFuzzyCompare(a.x() + 1, b.x() + 1)) {
			return a.x() < b.x();
		}
		return a.y() < b.y();
	};
	QString strip;
	QPointF best;
	bool found = false;
	for (Element *other : element->diagram()->elements()) {
		if (other == element || other->linkType() != Element::Terminale) {
			continue;
		}
		const QPointF there = other->scenePos();
		if (!before(there, here) || (found && !before(best, there))) {
			continue;
		}
		QString name;
		parseLabel(other->actualLabel(), &name, nullptr);
		if (name.isEmpty()) {
			continue;
		}
		strip = name;
		best = there;
		found = true;
	}
	return strip;
}

QStringList FaseTerminal::stripNames(QETProject *project)
{
	QStringList names;
	if (!project) {
		return names;
	}
	for (TerminalStrip *strip : project->terminalStrip()) {
		if (strip && !strip->name().isEmpty()
				&& !names.contains(strip->name())) {
			names << strip->name();
		}
	}
	std::sort(names.begin(), names.end(), QETUtils::naturalLessThan);
	return names;
}

QString FaseTerminal::nextNumber(QETProject *project, const QString &strip,
								   const QSet<const Element *> &skip)
{
	int highest = 0;
	auto count = [&](const QString &label) {
		QString name, number;
		parseLabel(label, &name, &number);
		bool ok = false;
		const int value = number.toInt(&ok);
		if (ok && name == strip) {
			highest = qMax(highest, value);
		}
	};
	if (project) {
		for (Diagram *diagram : project->diagrams()) {
			for (Element *element : diagram->elements()) {
				if (element->linkType() == Element::Terminale
						&& !skip.contains(element)) {
					count(element->elementData().m_informations
						  .value(QStringLiteral("label")).toString());
				}
			}
		}
		for (TerminalStrip *strip_item : project->terminalStrip()) {
			if (!strip_item || strip_item->name() != strip) {
				continue;
			}
			for (const auto &real : strip_item->realTerminals()) {
				if (real && !skip.contains(real->element())) {
					QString number;
					parseLabel(real->label(), nullptr, &number);
					bool ok = false;
					const int value = number.toInt(&ok);
					if (ok) {
						highest = qMax(highest, value);
					}
				}
			}
		}
	}
	return QString::number(highest + 1);
}

QList<FaseTerminal::Pasted> FaseTerminal::pastedTerminals(
		const QList<Element *> &elements)
{
	QList<Pasted> list;
	for (Element *element : elements) {
		if (!element || element->linkType() != Element::Terminale) {
			continue;
		}
		Pasted pasted;
		pasted.element = element;
		pasted.settings = settingsOf(element);
		if (!pasted.settings.strip.isEmpty()) {
			list << pasted;
		}
	}
	return list;
}

void FaseTerminal::numberPasted(QETProject *project,
								const QList<Pasted> &pasted,
								QUndoCommand *parent)
{
	if (!project || pasted.isEmpty()) {
		return;
	}
	QSet<const Element *> skip;
	for (const Pasted &one : pasted) {
		skip.insert(one.element.data());
	}
	QList<Pasted> ordered = pasted;
	std::stable_sort(ordered.begin(), ordered.end(),
					 [](const Pasted &a, const Pasted &b) {
		if (a.settings.strip != b.settings.strip) {
			return QETUtils::naturalLessThan(a.settings.strip,
											 b.settings.strip);
		}
		return QETUtils::naturalLessThan(a.settings.number,
										 b.settings.number);
	});
	QHash<QString, int> next;
	for (const Pasted &one : std::as_const(ordered)) {
		if (!one.element) {
			continue;
		}
		const QString &strip = one.settings.strip;
		if (!next.contains(strip)) {
			next.insert(strip, nextNumber(project, strip, skip).toInt());
		}
		Settings settings = one.settings;
		settings.number = QString::number(next[strip]++);
		new ChangeCommand(one.element, settings, parent);
	}
}

ElementsLocation FaseTerminal::symbol(QETProject *project)
{
	if (!project || !project->embeddedElementCollection()) {
		return ElementsLocation();
	}
	XmlElementCollection *collection = project->embeddedElementCollection();
	const QString path = symbol_dir + QLatin1Char('/') + symbol_file;
	if (!collection->exist(path)) {
		if (!collection->exist(symbol_dir)) {
			NamesList names;
			names.addName(QStringLiteral("en"), QStringLiteral("FaseCAD"));
			if (!collection->createDir(QStringLiteral("import"),
									   QStringLiteral("FaseCAD"), names)) {
				return ElementsLocation();
			}
		}
		QFile file(QStringLiteral(":/terminals/fasecad_terminal.elmt"));
		QDomDocument document;
		if (!file.open(QIODevice::ReadOnly)
				|| !document.setContent(&file)) {
			return ElementsLocation();
		}
		if (!collection->addElementDefinition(symbol_dir, symbol_file,
											  document.documentElement())) {
			return ElementsLocation();
		}
		project->setModified(true);
	}
	return ElementsLocation(path, project);
}

FaseTerminal::ChangeCommand::ChangeCommand(Element *element,
										   const Settings &settings,
										   QUndoCommand *parent) :
	QUndoCommand(parent),
	m_element(element),
	m_settings(settings)
{
	setText(translated("Edit the terminal %1")
			.arg(label(settings.strip, settings.number)));
}

FaseTerminal::ChangeCommand::~ChangeCommand()
{
	qDeleteAll(m_steps);
}

/**
	@brief FaseTerminal::ChangeCommand::step
	Do @p command at once and keep it: each step is worked out from what
	the steps before it made (the strip the terminal is in, the terminals
	at its number).
*/
void FaseTerminal::ChangeCommand::step(QUndoCommand *command)
{
	command->redo();
	m_steps << command;
}

/**
	@brief FaseTerminal::ChangeCommand::build
	Do the steps the first time the command is done, from the element as
	it is then.
*/
void FaseTerminal::ChangeCommand::build()
{
	m_built = true;
	if (!m_element) {
		return;
	}
		//No strip named: the one of the terminal on its left
	if (m_settings.strip.isEmpty()) {
		m_settings.strip = inheritedStrip(m_element);
	}
	ElementData data = m_element->elementData();
	DiagramContext &info = data.m_informations;
	auto set = [&info](const QString &key, const QString &value) {
		if (value.isEmpty()) {
			info.remove(key);
		} else {
			info.addValue(key, value);
		}
	};
		//The label is the strip and the number: a numbering formula would
		//write another one over it
	info.remove(QStringLiteral("formula"));
	set(QStringLiteral("label"), label(m_settings.strip, m_settings.number));
	set(RealTerminal::colorInfoKey(), m_settings.color);
	set(RealTerminal::sectionInfoKey(), m_settings.section);
	data.setTerminalType(m_settings.type);
	data.setTerminalLED(m_settings.led);
	if (data != m_element->elementData()) {
		step(new ChangeElementDataCommand(m_element, data));
	}

	auto *terminal = qobject_cast<TerminalElement *>(m_element.data());
	QETProject *project = m_element->diagram()
			? m_element->diagram()->project() : nullptr;
	if (!terminal || !project || m_settings.strip.isEmpty()) {
		return;
	}
	const QSharedPointer<RealTerminal> real = terminal->realTerminal();
	if (!real) {
		return;
	}
	TerminalStrip *target = nullptr;
	for (TerminalStrip *strip : project->terminalStrip()) {
		if (strip && strip->name() == m_settings.strip) {
			target = strip;
			break;
		}
	}
	if (!target) {
		target = project->newTerminalStrip(QString(), QString(),
										   m_settings.strip);
		step(new AddTerminalStripCommand(target, project));
	}
	TerminalStrip *current = real->parentStrip();
	if (current != target) {
		if (current) {
				//Alone again before it goes: the levels of a terminal
				//belong to one strip
			if (real->physicalTerminal()
					&& real->physicalTerminal()->realTerminalCount() > 1) {
				step(new UnGroupTerminalsCommand(current, {real}));
			}
			step(new MoveTerminalCommand(real->physicalTerminal(), current,
										 target));
		} else {
			step(new AddTerminalToStripCommand(real, target));
		}
	}

		//The levels: the terminals of the strip with the same number are
		//one terminal, this one at its level
	QSharedPointer<PhysicalTerminal> partner;
	for (const auto &other : target->realTerminals()) {
		if (!other || other == real) {
			continue;
		}
		QString number;
		parseLabel(other->label(), nullptr, &number);
		if (number == m_settings.number) {
			partner = other->physicalTerminal();
			break;
		}
	}
	const QSharedPointer<PhysicalTerminal> own = real->physicalTerminal();
	if (partner && partner != own) {
		if (own && own->realTerminalCount() > 1) {
			step(new UnGroupTerminalsCommand(target, {real}));
		}
		step(new GroupTerminalsCommand(target, partner, {real}));
	} else if (!partner && own && own->realTerminalCount() > 1) {
		step(new UnGroupTerminalsCommand(target, {real}));
	}
	const int level = qMax(0, m_settings.level - 1);
	if (real->physicalTerminal()
			&& real->physicalTerminal()->realTerminalCount() > 1
			&& real->level() != level) {
		step(new ChangeTerminalLevel(target, real, level));
	}
}

void FaseTerminal::ChangeCommand::redo()
{
	if (!m_built) {
		build();
	} else {
		for (QUndoCommand *command : std::as_const(m_steps)) {
			command->redo();
		}
	}
	scheduleLabels();
}

void FaseTerminal::ChangeCommand::undo()
{
	for (int i = m_steps.size() - 1; i >= 0; --i) {
		m_steps.at(i)->undo();
	}
	scheduleLabels();
}

/**
	@brief FaseTerminal::ChangeCommand::scheduleLabels
	The folios of the terminal and of the other terminals of its strip
	draw their labels and level marks again: a level changed here changes
	the mark of every level of the terminal, wherever they stand.
*/
void FaseTerminal::ChangeCommand::scheduleLabels()
{
	if (!m_element) {
		return;
	}
	QSet<Diagram *> diagrams;
	if (m_element->diagram()) {
		diagrams.insert(m_element->diagram());
	}
	if (auto *terminal = qobject_cast<TerminalElement *>(m_element.data())) {
		if (const auto real = terminal->realTerminal()) {
			if (TerminalStrip *strip = real->parentStrip()) {
				for (const auto &other : strip->realTerminals()) {
					if (other && other->element() && other->element()->diagram()) {
						diagrams.insert(other->element()->diagram());
					}
				}
			}
		}
	}
	for (Diagram *diagram : std::as_const(diagrams)) {
		TerminalStripLabels::schedule(diagram);
	}
}
