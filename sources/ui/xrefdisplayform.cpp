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
#include "xrefdisplayform.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>

XRefDisplayForm::XRefDisplayForm(QWidget *parent) :
	QWidget(parent)
{
	auto *form = new QFormLayout(this);
	form->setContentsMargins(0, 0, 0, 0);

	m_appearance = new QComboBox(this);
	m_appearance->setObjectName(QStringLiteral("m_appearance"));
	m_appearance->addItem(tr("Cross"), false);
	m_appearance->addItem(tr("Contact symbols"), true);
	form->addRow(tr("Appearance"), m_appearance);

	m_position = new QComboBox(this);
	m_position->setObjectName(QStringLiteral("m_position"));
	m_position->addItem(tr("Under device label"), ElementData::XRefDisplay::UnderLabel);
	m_position->addItem(tr("Below symbol"), ElementData::XRefDisplay::BelowSymbol);
	m_position->addItem(tr("Folio footer"), ElementData::XRefDisplay::Footer);
	m_position->addItem(tr("Manual position"), ElementData::XRefDisplay::Manual);
	form->addRow(tr("Position"), m_position);

	m_anchor = new QComboBox(this);
	m_anchor->setObjectName(QStringLiteral("m_anchor"));
	m_anchor->addItem(tr("Relative to the symbol"), false);
	m_anchor->addItem(tr("Absolute on the folio"), true);
	m_anchor->setToolTip(tr("Relative: the display follows the symbol when it "
							"moves. Absolute: it stays where it is on the folio."));
	form->addRow(tr("Manual position"), m_anchor);

	const auto spin = [this](const QString &name, qreal low, qreal high) {
		auto *box = new QDoubleSpinBox(this);
		box->setObjectName(name);
		box->setRange(low, high);
		box->setDecimals(2);
		return box;
	};
	m_x = spin(QStringLiteral("m_x"), -10000, 10000);
	m_y = spin(QStringLiteral("m_y"), -10000, 10000);
	m_x_label = new QLabel(this);
	m_y_label = new QLabel(this);
	form->addRow(m_x_label, m_x);
	form->addRow(m_y_label, m_y);

	m_angle = new QComboBox(this);
	m_angle->setObjectName(QStringLiteral("m_angle"));
	for (int angle : {0, 90, 180, 270}) {
		m_angle->addItem(QString::number(angle) + QChar(0x00B0), angle);
	}
	m_angle->setToolTip(tr("Turns the contact symbols; their numbers stay upright."));
	form->addRow(tr("Contact symbol angle"), m_angle);

	m_width = spin(QStringLiteral("m_width"), 0.1, 10);
	form->addRow(tr("Line thickness"), m_width);
	m_gap = spin(QStringLiteral("m_gap"), 0, 500);
	form->addRow(tr("Gap below symbol"), m_gap);

	m_show_unlinked = new QCheckBox(tr("Show defined contacts even when not linked"), this);
	m_show_unlinked->setObjectName(QStringLiteral("m_show_unlinked"));
	form->addRow(m_show_unlinked);

	setDisplay(ElementData::XRefDisplay(), QPointF(), QPointF());

	for (QComboBox *box : {m_appearance, m_position, m_angle}) {
		connect(box, &QComboBox::currentIndexChanged, this, [this]() {
			updateEnabled();
			emitChanged();
		});
	}
	connect(m_anchor, &QComboBox::currentIndexChanged, this, [this]() {
		anchorChanged();
		emitChanged();
	});
	for (QDoubleSpinBox *box : {m_x, m_y, m_width, m_gap}) {
		connect(box, &QDoubleSpinBox::valueChanged, this, &XRefDisplayForm::emitChanged);
	}
	connect(m_show_unlinked, &QCheckBox::toggled, this, &XRefDisplayForm::emitChanged);
}

void XRefDisplayForm::setDisplay(const ElementData::XRefDisplay &display,
								 const QPointF &symbol_pos,
								 const QPointF &suggested_manual)
{
	m_updating = true;
	m_symbol_pos = symbol_pos;
	m_appearance->setCurrentIndex(display.contacts ? 1 : 0);
	m_position->setCurrentIndex(m_position->findData(display.position));

	const bool manual = display.enabled
			&& display.position == ElementData::XRefDisplay::Manual;
	m_absolute = manual && display.absolute;
	m_anchor->setCurrentIndex(m_absolute ? 1 : 0);
	const QPointF point = manual ? display.manual : suggested_manual;
	m_x->setValue(point.x());
	m_y->setValue(point.y());

	m_angle->setCurrentIndex(qMax(0, m_angle->findData(display.angle)));
	m_width->setValue(display.width);
	m_gap->setValue(display.gap);
	m_show_unlinked->setChecked(display.showUnlinked);
	updateEnabled();
	m_updating = false;
}

ElementData::XRefDisplay XRefDisplayForm::display() const
{
	ElementData::XRefDisplay display;
	display.enabled = true;
	display.contacts = m_appearance->currentData().toBool();
	display.position = static_cast<ElementData::XRefDisplay::Position>(
				m_position->currentData().toInt());
	display.manual = QPointF(m_x->value(), m_y->value());
	display.absolute = m_absolute;
	display.angle = m_angle->currentData().toInt();
	display.width = m_width->value();
	display.gap = m_gap->value();
	display.showUnlinked = m_show_unlinked->isChecked();
	return display;
}

/**
	@brief XRefDisplayForm::updateEnabled
	The gap and the coordinates only matter for their own position, the
	angle for contact symbols only; the coordinates say what they are
	measured from.
*/
void XRefDisplayForm::updateEnabled()
{
	const int position = m_position->currentData().toInt();
	const bool manual = position == ElementData::XRefDisplay::Manual;
	m_gap->setEnabled(position == ElementData::XRefDisplay::BelowSymbol);
	m_anchor->setEnabled(manual);
	m_x->setEnabled(manual);
	m_y->setEnabled(manual);
	m_angle->setEnabled(m_appearance->currentData().toBool());
	m_x_label->setText(m_absolute ? tr("Folio position X") : tr("Manual offset X"));
	m_y_label->setText(m_absolute ? tr("Folio position Y") : tr("Manual offset Y"));
}

/**
	@brief XRefDisplayForm::anchorChanged
	From the symbol to the folio or back: the same place, measured from
	the other origin.
*/
void XRefDisplayForm::anchorChanged()
{
	const bool absolute = m_anchor->currentData().toBool();
	if (absolute == m_absolute) return;

	const QPointF shift = absolute ? m_symbol_pos : -m_symbol_pos;
	const bool updating = m_updating;
	m_updating = true;
	m_x->setValue(m_x->value() + shift.x());
	m_y->setValue(m_y->value() + shift.y());
	m_updating = updating;
	m_absolute = absolute;
	updateEnabled();
}

void XRefDisplayForm::emitChanged()
{
	if (!m_updating) {
		emit changed();
	}
}
