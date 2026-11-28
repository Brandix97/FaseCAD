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
#include "summarygeneratordialog.h"

#include "qetapp.h"
#include "qetproject.h"
#include "summarysymbols.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QSettings>
#include <QVBoxLayout>

SummaryGeneratorDialog::SummaryGeneratorDialog(QWidget *parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Generate the PLC summary symbol"));
	auto *layout = new QVBoxLayout(this);
	layout->addWidget(new QLabel(tr("A terminal is drawn for each terminal of the card, with its "
									"function text and place beside it. Each terminal shows the "
									"terminal of the same name of the device of its label."), this));
	qobject_cast<QLabel *>(layout->itemAt(0)->widget())->setWordWrap(true);

	m_from_device = new QRadioButton(tr("The terminals of a device of an open project:"), this);
	m_device = new QComboBox(this);
	for (QETProject *project : QETApp::registeredProjects()) {
		const QString title = project->title().isEmpty() ? project->filePath() : project->title();
		for (const QString &label : SummarySymbols::deviceLabels(project)) {
			const QStringList pins = SummarySymbols::deviceTerminals(project, label);
			if (pins.isEmpty()) {
				continue;
			}
			m_device->addItem(tr("%1 (%n terminal(s)) - %2", nullptr, int(pins.size()))
							  .arg(label, title), pins);
		}
	}
	m_from_pattern = new QRadioButton(tr("Terminals named after a pattern:"), this);
	m_pattern = new QLineEdit(this);
	m_pattern->setPlaceholderText(QStringLiteral(".0-.7, X1:1-X1:32, I0.0-I3.7"));
	m_pattern->setText(QSettings().value(QStringLiteral("summary_generator/pattern"),
										 QStringLiteral("I0.0-I3.7")).toString());
	m_example = new QRadioButton(tr("The example: four terminals, X1:1 to X1:4"), this);

	layout->addWidget(m_from_device);
	layout->addWidget(m_device);
	layout->addWidget(m_from_pattern);
	layout->addWidget(m_pattern);
	layout->addWidget(m_example);

	auto *form = new QFormLayout();
	m_columns = new QComboBox(this);
	m_columns->addItem(tr("One column, the texts on the left"), false);
	m_columns->addItem(tr("Two columns, half on each side"), true);
	m_columns->setCurrentIndex(QSettings().value(QStringLiteral("summary_generator/two_columns"),
												 false).toBool() ? 1 : 0);
	form->addRow(tr("Layout:"), m_columns);
	layout->addLayout(form);

	m_preview = new QLabel(this);
	m_preview->setWordWrap(true);
	layout->addWidget(m_preview);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
		QSettings settings;
		settings.setValue(QStringLiteral("summary_generator/pattern"), m_pattern->text());
		settings.setValue(QStringLiteral("summary_generator/two_columns"), twoColumns());
		accept();
	});
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);

	if (m_device->count()) {
		m_from_device->setChecked(true);
	} else {
		m_from_device->setEnabled(false);
		m_device->setEnabled(false);
		m_from_pattern->setChecked(true);
	}
	for (QRadioButton *radio : {m_from_device, m_from_pattern, m_example}) {
		connect(radio, &QRadioButton::toggled, this, &SummaryGeneratorDialog::updatePreview);
	}
	connect(m_device, &QComboBox::currentIndexChanged, this, &SummaryGeneratorDialog::updatePreview);
	connect(m_pattern, &QLineEdit::textChanged, this, [this]() {
		m_from_pattern->setChecked(true);
		updatePreview();
	});
	updatePreview();
	resize(520, sizeHint().height());
}

QStringList SummaryGeneratorDialog::pins() const
{
	if (m_from_device->isChecked()) {
		return m_device->currentData().toStringList();
	}
	if (m_from_pattern->isChecked()) {
		return SummarySymbols::expandPattern(m_pattern->text());
	}
	return {QStringLiteral("X1:1"), QStringLiteral("X1:2"),
			QStringLiteral("X1:3"), QStringLiteral("X1:4")};
}

bool SummaryGeneratorDialog::twoColumns() const
{
	return m_columns->currentData().toBool();
}

void SummaryGeneratorDialog::updatePreview()
{
	const QStringList names = pins();
	QStringList shown = names.mid(0, 12);
	if (names.size() > 12) {
		shown << QStringLiteral("…") << names.last();
	}
	m_preview->setText(tr("%n terminal(s): %1", nullptr, int(names.size()))
					   .arg(shown.join(QStringLiteral(", "))));
}
