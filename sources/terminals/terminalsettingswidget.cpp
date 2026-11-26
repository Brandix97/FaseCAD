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
#include "terminalsettingswidget.h"

#include "../TerminalStrip/terminalcolor.h"
#include "../qetproject.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>

TerminalSettingsWidget::TerminalSettingsWidget(QETProject *project,
											   QWidget *parent) :
	QWidget(parent),
	m_project(project)
{
	auto *layout = new QFormLayout(this);

	m_strip = new QComboBox(this);
	m_strip->setEditable(true);
	m_strip->addItems(FaseTerminal::stripNames(project));
	m_strip->setToolTip(tr("A terminal strip of the project, or the name"
						   " of a new one. Empty: the strip of the terminal"
						   " on its left on the folio"));
	m_strip->lineEdit()->setPlaceholderText(tr("Same as on the left"));
	layout->addRow(tr("Terminal strip:"), m_strip);

	m_number = new QLineEdit(this);
	layout->addRow(tr("Terminal number:"), m_number);

	m_color = new QComboBox(this);
	m_color->addItem(tr("None"), QString());
	for (const FaseTerminal::ColorChoice &choice : FaseTerminal::colorChoices()) {
		QColor main, stripe;
		TerminalColor::parse(choice.spec, &main, &stripe);
		m_color->addItem(QIcon(TerminalColor::swatch(main, stripe)),
						 choice.name, choice.spec);
	}
	layout->addRow(tr("Colour:"), m_color);

	m_type = new QComboBox(this);
	const auto types = FaseTerminal::typeChoices();
	for (int i = 0; i < types.size(); ++i) {
		m_type->addItem(types.at(i).name, i);
	}
	layout->addRow(tr("Type:"), m_type);

	m_section = new QComboBox(this);
	m_section->addItem(tr("None"), QString());
	for (const QString &section : FaseTerminal::sections()) {
		m_section->addItem(tr("%1 mm²").arg(section), section);
	}
	layout->addRow(tr("Cross-section:"), m_section);

	m_level = new QComboBox(this);
	for (int level = 1; level <= 4; ++level) {
		m_level->addItem(QString::number(level), level);
	}
	m_level->setToolTip(tr("The terminals of the strip with the same number"
						   " are one terminal of several levels, 1 at the"
						   " bottom"));
	layout->addRow(tr("Level:"), m_level);

		//A name or a number being typed is no change yet: it is one
		//once typed (Enter, or leaving the field) or picked in the list
	connect(m_strip, &QComboBox::currentTextChanged,
			this, &TerminalSettingsWidget::stripChanged);
	connect(m_strip, qOverload<int>(&QComboBox::activated),
			this, &TerminalSettingsWidget::emitChanged);
	connect(m_strip->lineEdit(), &QLineEdit::editingFinished,
			this, &TerminalSettingsWidget::emitChanged);
	connect(m_number, &QLineEdit::editingFinished,
			this, &TerminalSettingsWidget::emitChanged);
	for (QComboBox *combo : {m_color, m_type, m_section, m_level}) {
		connect(combo, qOverload<int>(&QComboBox::currentIndexChanged),
				this, [this]() {
			if (!m_filling) {
				emit changed();
			}
		});
	}
}

FaseTerminal::Settings TerminalSettingsWidget::settings() const
{
	FaseTerminal::Settings settings;
	settings.strip = m_strip->currentText().trimmed();
	if (settings.strip.startsWith(QLatin1Char('-'))) {
		settings.strip.remove(0, 1);
	}
	settings.number = m_number->text().trimmed();
	settings.color = m_color->currentData().toString();
	const auto types = FaseTerminal::typeChoices();
	const int type = m_type->currentData().toInt();
	if (type >= 0 && type < types.size()) {
		settings.type = types.at(type).type;
		settings.led = types.at(type).led;
	}
	settings.section = m_section->currentData().toString();
	settings.level = m_level->currentData().toInt();
	return settings;
}

void TerminalSettingsWidget::setSettings(const FaseTerminal::Settings &settings)
{
	m_filling = true;
	m_strip->setCurrentText(settings.strip);
	m_number->setText(settings.number);
	QColor main, stripe;
	TerminalColor::parse(settings.color, &main, &stripe);
	selectData(m_color, settings.color,
			   TerminalColor::displayName(main, stripe));
	int type = 0;
	const auto types = FaseTerminal::typeChoices();
	for (int i = 0; i < types.size(); ++i) {
		if (types.at(i).type == settings.type
				&& types.at(i).led == settings.led) {
			type = i;
		}
	}
	m_type->setCurrentIndex(m_type->findData(type));
	selectData(m_section, settings.section,
			   tr("%1 mm²").arg(settings.section));
	selectData(m_level, settings.level, QString::number(settings.level));
	m_filling = false;
}

/**
	@brief TerminalSettingsWidget::selectData
	Select the entry of @p combo holding @p data; a value the list does
	not offer (written by the terminal manager) gets an entry of its own,
	so it is kept rather than lost.
*/
void TerminalSettingsWidget::selectData(QComboBox *combo, const QVariant &data,
										const QString &other_text)
{
	int index = combo->findData(data);
	if (index < 0) {
		combo->addItem(other_text, data);
		index = combo->count() - 1;
	}
	combo->setCurrentIndex(index);
}

void TerminalSettingsWidget::stripChanged()
{
	if (m_filling) {
		return;
	}
	if (m_propose) {
		m_number->setText(FaseTerminal::nextNumber(m_project,
												   settings().strip));
	}
}

void TerminalSettingsWidget::emitChanged()
{
	if (!m_filling) {
		emit changed();
	}
}
