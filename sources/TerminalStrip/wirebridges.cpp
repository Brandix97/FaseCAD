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
#include "wirebridges.h"

#include "realterminal.h"
#include "terminalstrip.h"
#include "terminalstripbridge.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/terminal.h"
#include "../qetgraphicsitem/terminalelement.h"

#include <QHash>
#include <QPointer>
#include <QUndoCommand>

namespace {
		/// The level of a strip the pin @p pin is on, if its symbol is a
		/// terminal of a strip
	QSharedPointer<RealTerminal> realTerminalOf(const Terminal *pin)
	{
		auto *element = pin ? dynamic_cast<TerminalElement *>(pin->parentElement()) : nullptr;
		return element ? element->realTerminal() : QSharedPointer<RealTerminal>();
	}

		/// Whether @p terminal is still wired to another terminal of
		/// @p bridge by a wire not in @p going
	bool stillWiredTo(const QSharedPointer<RealTerminal> &terminal,
					  const QSharedPointer<TerminalStripBridge> &bridge,
					  const QSet<Conductor *> &going)
	{
		const QVector<QSharedPointer<RealTerminal>> members = bridge->realTerminals();
		auto *element = terminal ? terminal->element() : nullptr;
		if (!element) {
			return false;
		}
		for (Terminal *pin : element->terminals()) {
			for (Conductor *wire : pin->conductors()) {
				if (going.contains(wire)) {
					continue;
				}
				const Terminal *other = wire->terminal1 == pin ? wire->terminal2 : wire->terminal1;
				const QSharedPointer<RealTerminal> far = realTerminalOf(other);
				if (far && far != terminal && members.contains(far)) {
					return true;
				}
			}
		}
		return false;
	}
}

bool WireBridges::ends(const Conductor *wire, TerminalStrip **strip,
					   QVector<QSharedPointer<RealTerminal>> *terminals)
{
	if (!wire) {
		return false;
	}
	const QSharedPointer<RealTerminal> first = realTerminalOf(wire->terminal1);
	const QSharedPointer<RealTerminal> second = realTerminalOf(wire->terminal2);
	if (!first || !second || first == second) {
		return false;
	}
	TerminalStrip *own = first->parentStrip();
	if (!own || own != second->parentStrip()) {
		return false;
	}
	if (strip) *strip = own;
	if (terminals) *terminals = {first, second};
	return true;
}

namespace {
		/// The bar a wire makes or lengthens, put back as it was on undo
	class JoinCommand : public QUndoCommand
	{
		public:
			JoinCommand(TerminalStrip *strip, const QVector<QSharedPointer<RealTerminal>> &pair,
						QUndoCommand *parent) :
				QUndoCommand(QObject::tr("Bridging terminals together"), parent),
				m_strip(strip), m_pair(pair)
			{}

			void redo() override
			{
				if (!m_strip) return;
				if (!m_bar) {
					QVector<QSharedPointer<RealTerminal>> added;
					m_bar = m_strip->joinWithBar(m_pair, &added);
					if (!m_bar) return;
					m_after = m_bar->realTerminals();
					for (const auto &real_t : std::as_const(m_after))
						if (!added.contains(real_t)) m_before << real_t;
				} else {
					m_strip->restoreBar(m_bar, m_after);
				}
			}

			void undo() override
			{
				if (!m_strip || !m_bar) return;
				if (m_before.size() < 2) m_strip->leaveBar(m_bar, m_bar->realTerminals());
				else m_strip->restoreBar(m_bar, m_before);
			}

		private:
			QPointer<TerminalStrip> m_strip;
			QVector<QSharedPointer<RealTerminal>> m_pair;
			QSharedPointer<TerminalStripBridge> m_bar;
			QVector<QSharedPointer<RealTerminal>> m_before;
			QVector<QSharedPointer<RealTerminal>> m_after;
	};

		/// Terminals off a bar, the bar whole again on undo
	class LeaveCommand : public QUndoCommand
	{
		public:
			LeaveCommand(TerminalStrip *strip, const QSharedPointer<TerminalStripBridge> &bar,
						 const QVector<QSharedPointer<RealTerminal>> &leaving, QUndoCommand *parent) :
				QUndoCommand(QObject::tr("Delete terminal bridges"), parent),
				m_strip(strip), m_bar(bar), m_leaving(leaving), m_before(bar->realTerminals())
			{}

			void redo() override {if (m_strip) m_strip->leaveBar(m_bar, m_leaving);}
			void undo() override {if (m_strip) m_strip->restoreBar(m_bar, m_before);}

		private:
			QPointer<TerminalStrip> m_strip;
			QSharedPointer<TerminalStripBridge> m_bar;
			QVector<QSharedPointer<RealTerminal>> m_leaving;
			QVector<QSharedPointer<RealTerminal>> m_before;
	};
}

QUndoCommand *WireBridges::bridge(const Conductor *wire, QUndoCommand *parent)
{
	TerminalStrip *strip = nullptr;
	QVector<QSharedPointer<RealTerminal>> pair;
	if (!ends(wire, &strip, &pair)) {
		return nullptr;
	}
		//Side by side or with terminals between them (a comb with teeth
		//taken out): one bar
	if (!strip->canJoinWithBar(pair)) {
		return nullptr;
	}
	return new JoinCommand(strip, pair, parent);
}

int WireBridges::letGo(const QSet<Conductor *> &going, QUndoCommand *parent)
{
		//The terminals leaving each bar, in one step per bar
	QHash<TerminalStripBridge *, QVector<QSharedPointer<RealTerminal>>> leaving;
	QHash<TerminalStripBridge *, TerminalStrip *> strip_of;
	QHash<TerminalStripBridge *, QSharedPointer<TerminalStripBridge>> shared_of;
	for (Conductor *wire : going) {
		TerminalStrip *strip = nullptr;
		QVector<QSharedPointer<RealTerminal>> pair;
		if (!ends(wire, &strip, &pair)) {
			continue;
		}
		const QSharedPointer<TerminalStripBridge> bar = strip->commonBridge(pair);
		if (!bar || bar->automatic()) {
			continue;
		}
		for (const QSharedPointer<RealTerminal> &terminal : std::as_const(pair)) {
			QVector<QSharedPointer<RealTerminal>> &list = leaving[bar.data()];
			if (!list.contains(terminal) && !stillWiredTo(terminal, bar, going)) {
				list << terminal;
			}
		}
		strip_of.insert(bar.data(), strip);
		shared_of.insert(bar.data(), bar);
	}

	int steps = 0;
	for (auto it = leaving.constBegin() ; it != leaving.constEnd() ; ++it) {
		TerminalStrip *strip = strip_of.value(it.key());
		const QSharedPointer<TerminalStripBridge> bar = shared_of.value(it.key());
		if (!strip || !bar || it.value().isEmpty()) {
			continue;
		}
		new LeaveCommand(strip, bar, it.value(), parent);
		++steps;
	}
	return steps;
}
