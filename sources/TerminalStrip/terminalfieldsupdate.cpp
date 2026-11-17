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
#include "terminalfieldsupdate.h"

#include "../qetgraphicsitem/dynamicelementtextitem.h"
#include "../qetgraphicsitem/element.h"
#include "../qetinformation.h"

#include <QDomElement>
#include <QLineF>
#include <QUuid>

#include <memory>

namespace {

/**
	@return the first text field of @p definition showing the
	information @p info, a null element when it has none
*/
QDomElement fieldOf(const QDomElement &definition, const QString &info)
{
	const QDomElement description =
			definition.firstChildElement(QStringLiteral("description"));
	for (QDomElement text = description.firstChildElement(QStringLiteral("dynamic_text"));
		 !text.isNull();
		 text = text.nextSiblingElement(QStringLiteral("dynamic_text")))
	{
		if (text.attribute(QStringLiteral("text_from")) == QLatin1String("ElementInfo")
			&& text.firstChildElement(QStringLiteral("info_name")).text() == info) {
			return text;
		}
	}
	return QDomElement();
}

/**
	@return @p field as a placed text field reads it, with an identifier
	of its own (the symbol's would be shared by every placed copy)
*/
QDomElement placedFrom(const QDomElement &field)
{
	QDomElement dom = field.cloneNode(true).toElement();
	dom.setTagName(DynamicElementTextItem::xmlTagName());
	dom.setAttribute(QStringLiteral("uuid"), QUuid::createUuid().toString());
	return dom;
}

/**
	@return where @p field of a symbol has its anchor in @p element,
	which keeps it whatever it shows; @p alignment receives its
	alignment
*/
QPointF anchorOf(Element *element, const QDomElement &field, Qt::Alignment *alignment)
{
		//Measured on a field which shows nothing, as a field does the
		//moment its element is placed: a text of its own, empty, rather
		//than the information it shows. Made on the element (a field
		//asks its element for its rotation) but never added to its
		//fields, and gone at the end of this function.
	QDomElement dom = placedFrom(field);
	dom.setAttribute(QStringLiteral("text_from"), QStringLiteral("UserText"));
	dom.setAttribute(QStringLiteral("keep_visual_rotation"), QStringLiteral("false"));
	const QDomElement info = dom.firstChildElement(QStringLiteral("info_name"));
	if (!info.isNull()) {
		dom.removeChild(info);
	}
	QDomElement text_dom = dom.firstChildElement(QStringLiteral("text"));
	if (!text_dom.isNull()) {
		dom.removeChild(text_dom);
	}
	std::unique_ptr<DynamicElementTextItem> text(new DynamicElementTextItem(element));
	text->fromXml(dom);
	*alignment = text->alignment();
	return text->anchorPos();
}

QList<DynamicElementTextItem *> fieldsOf(Element *element, const QString &info)
{
	QList<DynamicElementTextItem *> found;
	for (DynamicElementTextItem *text : element->dynamicTextItems())
	{
		if (text->textFrom() == DynamicElementTextItem::ElementInfo
			&& text->infoName() == info) {
			found << text;
		}
	}
	return found;
}

} // namespace

int TerminalFieldsUpdate::adopt(Element *element, const QDomElement &old_definition,
								const QDomElement &new_definition)
{
	if (!element || element->elementData().m_type != ElementData::Terminal) {
		return 0;
	}

	int changed = 0;
	for (const QString &info : {QETInformation::ELMT_LABEL,
								QETInformation::ELMT_TERMINAL_NUMBER})
	{
		const QDomElement now = fieldOf(new_definition, info);
		if (now.isNull()) continue;
		const QDomElement before = fieldOf(old_definition, info);
		const QList<DynamicElementTextItem *> placed = fieldsOf(element, info);

			//New in the symbol: added. One the old symbol had and the
			//placed terminal has not was deleted by hand, and stays so.
		if (placed.isEmpty())
		{
			if (!before.isNull()) continue;
			auto *text = new DynamicElementTextItem(element);
			text->fromXml(placedFrom(now));
			element->addDynamicTextItem(text);
			++changed;
			continue;
		}

			//Where the old symbol put it, aligned as it was: it goes
			//where the new one puts it
		if (placed.size() != 1 || before.isNull()) continue;
		Qt::Alignment old_alignment;
		Qt::Alignment new_alignment;
		const QPointF old_anchor = anchorOf(element, before, &old_alignment);
		const QPointF new_anchor = anchorOf(element, now, &new_alignment);
		DynamicElementTextItem *text = placed.first();
		if (text->alignment() != old_alignment
			|| QLineF(text->anchorPos(), old_anchor).length() > 0.5) {
			continue;
		}
		if (text->alignment() == new_alignment
			&& QLineF(old_anchor, new_anchor).length() <= 0.5) {
			continue;
		}
		text->setAlignment(new_alignment);
		text->setAnchorPos(new_anchor);
		++changed;
	}

	if (changed) {
			//The new fields show what the labelling of the project says
		element->refreshLabelFields();
	}
	return changed;
}
