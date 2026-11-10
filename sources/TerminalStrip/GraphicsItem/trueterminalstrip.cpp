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
#include "trueterminalstrip.h"
#include "../physicalterminal.h"
#include "../realterminal.h"
#include "../terminalstrip.h"
#include "../terminalstripbridge.h"
#include "../../autoNum/assignvariables.h"
#include "../../cable/cable.h"
#include "../../conductorproperties.h"
#include "../../diagram.h"
#include "../../factory/elementpicturefactory.h"
#include "../../qetgraphicsitem/conductor.h"
#include "../../qetgraphicsitem/element.h"
#include "../../qetgraphicsitem/terminal.h"
#include "../../qetproject.h"

#include "terminalstripdrawer.h"

namespace TerminalStripDrawer
{
	/**
	 * @brief TrueTerminalStrip::TrueTerminalStrip
	 * Constructor, this class don't take ownership of @a strip
	 * @param strip
	 */
	TrueTerminalStrip::TrueTerminalStrip(TerminalStrip *strip) :
		m_strip { strip }
	{}


	QString TrueTerminalStrip::installation() const
	{
		if (m_strip) {
			return m_strip->installation();
		} else {
			return QString();
		}
	}

	QString TrueTerminalStrip::location() const
	{
		if (m_strip) {
			return m_strip->location();
		} else {
			return QString();
		}
	}

	QString TrueTerminalStrip::name() const
	{
		if (m_strip) {
			return m_strip->name();
		} else {
			return QString();
		}
	}

	QVector<QSharedPointer<AbstractPhysicalTerminalInterface>> TrueTerminalStrip::physicalTerminal() const
	{
		QVector<QSharedPointer<AbstractPhysicalTerminalInterface>> vector_;
		if (m_strip) {
			for (const auto &phy : m_strip->physicalTerminal()) {
				vector_.append(QSharedPointer<AbstractPhysicalTerminalInterface>{ new TruePhysicalTerminal(phy) });
			}
		}

		return vector_;
	}

	TruePhysicalTerminal::TruePhysicalTerminal(QSharedPointer<PhysicalTerminal> physical) :
		m_physical { physical }
	{}

	QVector<QSharedPointer<AbstractRealTerminalInterface>> TruePhysicalTerminal::realTerminals() const
	{
		QVector<QSharedPointer<AbstractRealTerminalInterface>> vector_;
		if (m_physical) {
			for (const auto &real_ : m_physical->realTerminals()) {
				vector_.append(QSharedPointer<AbstractRealTerminalInterface> { new TrueRealTerminal{ real_ }});
			}
		}

		return vector_;
	}

	TrueRealTerminal::TrueRealTerminal(QSharedPointer<RealTerminal> real) :
		m_real { real }
	{}

	QString TrueRealTerminal::label() const
	{
		if (m_real) {
			return m_real->label();
		} else {
			return QString();
		}
	}

	bool TrueRealTerminal::isBridged() const
	{
		if (m_real) {
			return m_real->isBridged();
		} else {
			return false;
		}
	}

		//Return a raw pointer, the pointer is not managed by this class
	AbstractBridgeInterface* TrueRealTerminal::bridge() const
	{
		return new TrueBridge(m_real->bridge());
	}

	QString TrueRealTerminal::xref() const
	{
		if (m_real && m_real->isElement()) {
			return autonum::AssignVariables::genericXref(m_real->element());
		} else {
			return QString{};
		}
	}

	ElementData::TerminalType TrueRealTerminal::type() const
	{
		return m_real ? m_real->type() : ElementData::TTGeneric;
	}

	bool TrueRealTerminal::isLed() const
	{
		return m_real ? m_real->isLed() : false;
	}

	QString TrueRealTerminal::cable() const
	{
		return m_real ? m_real->cable() : QString();
	}

	QString TrueRealTerminal::cableWire() const
	{
		return m_real ? m_real->cableWire() : QString();
	}

	bool TrueRealTerminal::isShield() const
	{
		return m_real ? m_real->isShield() : false;
	}

	QVector<QString> TrueRealTerminal::cableTarget() const
	{
		return m_real ? m_real->cableTarget() : QVector<QString>();
	}

	QColor TrueRealTerminal::color() const
	{
		return m_real ? m_real->color() : QColor();
	}

	QColor TrueRealTerminal::stripeColor() const
	{
		return m_real ? m_real->stripeColor() : QColor();
	}

	qreal TrueRealTerminal::widthMm() const
	{
		return m_real ? m_real->widthMm() : 0;
	}

	QString TrueRealTerminal::catalogEntry() const
	{
		return m_real ? m_real->catalogEntry() : QString();
	}

	bool TrueRealTerminal::hasSeparatorAfter() const
	{
		return m_real ? m_real->hasSeparatorAfter() : false;
	}

	/**
	 * @brief TrueRealTerminal::diagramColumn
	 * What the terminal diagram style draws for this terminal, read from
	 * the wires on its two sides: the outer side is the one whose wire is
	 * a core of a cable, else the lower one.
	 */
	DiagramColumn TrueRealTerminal::diagramColumn() const
	{
		DiagramColumn column;
		if (m_real && m_real->section() > 0) {
			column.section = QString::number(m_real->section(), 'g', 6);
		}
		Element *terminal_element = m_real && m_real->isElement()
				? m_real->element() : nullptr;
		if (!terminal_element) {
			return column;
		}
		QETProject *project = terminal_element->diagram()
				? terminal_element->diagram()->project() : nullptr;

			//The core of a cable a wire is, -1 when it is none
		auto coreOf = [project](Conductor *conductor, Cable **cable) {
			if (!project) {
				return -1;
			}
			for (Cable *candidate : project->cables()) {
				for (const CableCore &core : candidate->usedCores()) {
					if (core.conductor == conductor->uuid()) {
						*cable = candidate;
						return core.core;
					}
				}
			}
			return -1;
		};

		struct Side {
			Terminal *terminal = nullptr;
			Conductor *conductor = nullptr;
			Terminal *far = nullptr;
			Cable *cable = nullptr;
			int core = -1;
		};
		QVector<Side> sides;
		for (Terminal *terminal : terminal_element->terminals()) {
			Side side;
			side.terminal = terminal;
			const QList<Conductor *> conductors = terminal->conductors();
			if (!conductors.isEmpty()) {
				side.conductor = conductors.first();
				side.far = side.conductor->terminal1 == terminal
						? side.conductor->terminal2 : side.conductor->terminal1;
				side.core = coreOf(side.conductor, &side.cable);
			}
			sides << side;
		}
		if (sides.isEmpty()) {
			return column;
		}

		int outer = -1;
		for (int i = 0; i < sides.size(); ++i) {
			if (sides.at(i).core >= 0) {
				outer = i;
				break;
			}
		}
		if (outer < 0) {
			outer = 0;
			for (int i = 1; i < sides.size(); ++i) {
				if (sides.at(i).terminal->sceneBoundingRect().center().y()
						> sides.at(outer).terminal->sceneBoundingRect().center().y()) {
					outer = i;
				}
			}
		}

		const Side &out = sides.at(outer);
		if (out.conductor) {
			if (out.cable) {
				column.outer_wire = out.cable->colorOfCore(out.core);
				if (column.outer_wire.isEmpty()) {
					column.outer_wire = QString::number(out.core + 1);
				}
			} else {
				const ConductorProperties properties = out.conductor->properties();
				column.outer_wire = properties.m_wire_color.isEmpty()
						? properties.text : properties.m_wire_color;
			}
		}
		if (out.far && out.far->parentElement()) {
			Element *device = out.far->parentElement();
			const DiagramContext information = device->elementInformations();
			column.function_text = information.value(QStringLiteral("function")).toString();
			if (column.function_text.isEmpty()) {
				column.function_text = information.value(QStringLiteral("description")).toString();
			}
			column.device_label = device->actualLabel();
			column.device_pin = out.far->name();
			column.device_key = device->uuid().toString();
			QPicture low;
			ElementPictureFactory::instance()->getPictures(
						device->location(), column.device_symbol, low);
		}

		for (int i = 0; i < sides.size(); ++i) {
			if (i == outer || !sides.at(i).far) {
				continue;
			}
			Terminal *inner = sides.at(i).far;
			column.inner_connection = inner->name();
			if (Element *device = inner->parentElement()) {
				column.plc_designation = device->elementInformations()
						.value(QStringLiteral("plc_address")).toString();
			}
			break;
		}
		return column;
	}

	QVector<AbstractBridgeInterface *> TrueRealTerminal::bridges() const
	{
		QVector<AbstractBridgeInterface *> list;
		const auto real_t = m_real;
		TerminalStrip *strip = real_t ? real_t->parentStrip() : nullptr;
		if (!strip) {
			return list;
		}
		for (const auto &bridge : strip->bridgesOf(real_t)) {
			list << new TrueBridge(bridge);
		}
		return list;
	}

    TrueBridge::TrueBridge(QSharedPointer<TerminalStripBridge> bridge) :
        m_bridge { bridge }
    {}

	QUuid TrueBridge::uuid() const
	{
		if (m_bridge) {
			return m_bridge->uuid();
		} else {
			return QUuid();
		}
	}

	int TrueBridge::track() const
	{
		return m_bridge ? m_bridge->track() : 1;
	}
}
