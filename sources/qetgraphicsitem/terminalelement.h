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
#ifndef TERMINALELEMENT_H
#define TERMINALELEMENT_H


#include "element.h"
#include "../TerminalStrip/terminalstrip.h"

class QETProject;
class QGraphicsLineItem;
class QGraphicsPathItem;
class QGraphicsSimpleTextItem;
class RealTerminal;
/**
	@brief The TerminalElement class
*/
class TerminalElement : public Element
{
		Q_OBJECT
	public:
		TerminalElement(const ElementsLocation &,
				QGraphicsItem * = nullptr, int * = nullptr);
		~TerminalElement() override;
		void initLink(QETProject *project) override;

		QSharedPointer<RealTerminal> realTerminal() const;
		void setParentTerminalStrip(TerminalStrip *strip);
		TerminalStrip *parentTerminalStrip() const;

			/// The label of its terminal strip, shown beside this terminal
			/// when it is the strip's first on its folio (TerminalStripLabels);
			/// empty hides it
		void setStripLabel(const QString &text);
		QString stripLabel() const;
		QGraphicsSimpleTextItem *stripLabelItem() const { return m_strip_label; }

			/// A bridge of the terminal strip editor to the next terminal of
			/// the row, as a line in this terminal's coordinates, drawn
			/// behind it (TerminalStripLabels); a null line takes it away
		void setBridgeLine(const QLineF &line);
			/// Show the label as the terminal number alone
		void setNumberOnly(bool number_only);
		bool showsTerminalNumberOnly() const override {return m_number_only;}
			/// Beside the symbol, the levels of the terminal it is a level of,
			/// stepped from level 1 on the left, its own one filled; nothing
			/// for a terminal of one level
		void setLevelMark(int levels, int level);
			/// The levels the mark shows, 0 for none
		int levelMarkLevels() const {return m_level_mark ? m_mark_levels : 0;}
		QLineF bridgeLine() const;

	protected:
		QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

	private:
		void placeStripLabel();

		QSharedPointer<RealTerminal> m_real_terminal;
		QGraphicsSimpleTextItem *m_strip_label = nullptr;
		QGraphicsLineItem *m_bridge_line = nullptr;
		QGraphicsPathItem *m_level_mark = nullptr;
		bool m_number_only = false;
		int m_mark_levels = 0;
		int m_mark_level = -1;
};

#endif // TERMINALELEMENT_H
