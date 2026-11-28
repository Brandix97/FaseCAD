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
#include "scriptdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include <limits>

ScriptDialog::ScriptDialog(const QVariantMap &spec, QWidget *parent) :
	QDialog(parent)
{
	setWindowTitle(spec.value(QStringLiteral("title"), tr("FaseCAD")).toString());
	auto *layout = new QVBoxLayout(this);

	const QString text = spec.value(QStringLiteral("text")).toString();
	if (!text.isEmpty()) {
		auto *label = new QLabel(text, this);
		label->setWordWrap(true);
		layout->addWidget(label);
	}

	auto *form = new QFormLayout;
	layout->addLayout(form);
	bool stretches = false;

	for (const QVariant &item : spec.value(QStringLiteral("fields")).toList()) {
		const QVariantMap f = item.toMap();
		const QString id = f.value(QStringLiteral("id")).toString();
		const QString type = f.value(QStringLiteral("type"), QStringLiteral("text")).toString();
		const QString label = f.value(QStringLiteral("label"), id).toString();
		const QVariant value = f.value(QStringLiteral("value"));
		if (id.isEmpty() || m_fields.contains(id)) {
			m_errors << tr("A field has no id, or the id %1 is repeated").arg(id);
			continue;
		}

		QWidget *widget = nullptr;
		if (type == QLatin1String("text")) {
			auto *edit = new QLineEdit(value.toString(), this);
			edit->setPlaceholderText(f.value(QStringLiteral("placeholder")).toString());
			widget = edit;
		} else if (type == QLatin1String("multiline")) {
			auto *edit = new QPlainTextEdit(value.toString(), this);
			widget = edit;
			stretches = true;
		} else if (type == QLatin1String("number")) {
			auto *spin = new QSpinBox(this);
			spin->setRange(f.value(QStringLiteral("min"), std::numeric_limits<int>::min()).toInt(),
						   f.value(QStringLiteral("max"), std::numeric_limits<int>::max()).toInt());
			spin->setValue(value.toInt());
			spin->setSuffix(f.value(QStringLiteral("suffix")).toString());
			widget = spin;
		} else if (type == QLatin1String("decimal")) {
			auto *spin = new QDoubleSpinBox(this);
			spin->setDecimals(f.value(QStringLiteral("decimals"), 2).toInt());
			spin->setRange(f.value(QStringLiteral("min"), -1e12).toDouble(),
						   f.value(QStringLiteral("max"), 1e12).toDouble());
			spin->setValue(value.toDouble());
			spin->setSuffix(f.value(QStringLiteral("suffix")).toString());
			widget = spin;
		} else if (type == QLatin1String("choice")) {
			auto *combo = new QComboBox(this);
			combo->addItems(f.value(QStringLiteral("items")).toStringList());
			combo->setEditable(f.value(QStringLiteral("editable")).toBool());
			const int index = combo->findText(value.toString());
			if (index >= 0) combo->setCurrentIndex(index);
			else if (combo->isEditable()) combo->setEditText(value.toString());
			widget = combo;
		} else if (type == QLatin1String("check")) {
			auto *check = new QCheckBox(label, this);
			check->setChecked(value.toBool());
			widget = check;
		} else if (type == QLatin1String("table")) {
			const QStringList columns = f.value(QStringLiteral("columns")).toStringList();
			const QVariantList rows = value.toList();
			auto *table = new QTableWidget(int(rows.size()), int(columns.size()), this);
			table->setHorizontalHeaderLabels(columns);
			table->horizontalHeader()->setStretchLastSection(true);
			for (int r = 0 ; r < rows.size() ; ++r) {
				const QVariantList cells = rows.at(r).toList();
				for (int c = 0 ; c < columns.size() && c < cells.size() ; ++c)
					table->setItem(r, c, new QTableWidgetItem(cells.at(c).toString()));
			}
			if (f.value(QStringLiteral("readOnly")).toBool())
				table->setEditTriggers(QAbstractItemView::NoEditTriggers);
			widget = table;
			stretches = true;
		} else {
			m_errors << tr("The field %1 has a type FaseCAD does not know: %2").arg(id, type);
			continue;
		}
		widget->setObjectName(id);
		widget->setToolTip(f.value(QStringLiteral("tooltip")).toString());
		m_fields.insert(id, widget);
		m_types.insert(id, type);
		m_order << id;
		if (type == QLatin1String("check")) form->addRow(widget);
		else form->addRow(label, widget);
	}

	m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	const QString ok = spec.value(QStringLiteral("ok")).toString();
	if (!ok.isEmpty()) m_buttons->button(QDialogButtonBox::Ok)->setText(ok);
	connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(m_buttons);
	if (stretches) resize(560, 420);
}

QVariantMap ScriptDialog::values() const
{
	QVariantMap result;
	for (const QString &id : m_order) {
		QWidget *w = m_fields.value(id);
		const QString type = m_types.value(id);
		if (auto *edit = qobject_cast<QLineEdit *>(w)) result.insert(id, edit->text());
		else if (auto *text = qobject_cast<QPlainTextEdit *>(w)) result.insert(id, text->toPlainText());
		else if (auto *spin = qobject_cast<QSpinBox *>(w)) result.insert(id, spin->value());
		else if (auto *dspin = qobject_cast<QDoubleSpinBox *>(w)) result.insert(id, dspin->value());
		else if (auto *combo = qobject_cast<QComboBox *>(w)) result.insert(id, combo->currentText());
		else if (auto *check = qobject_cast<QCheckBox *>(w)) result.insert(id, check->isChecked());
		else if (auto *table = qobject_cast<QTableWidget *>(w)) {
			QVariantList rows;
			for (int r = 0 ; r < table->rowCount() ; ++r) {
				QVariantList cells;
				for (int c = 0 ; c < table->columnCount() ; ++c) {
					QTableWidgetItem *cell = table->item(r, c);
					cells << (cell ? cell->text() : QString());
				}
				rows << QVariant(cells);
			}
			result.insert(id, rows);
		}
		Q_UNUSED(type)
	}
	return result;
}

QVariantMap ScriptDialog::ask(const QVariantMap &spec, QWidget *parent)
{
	ScriptDialog dialog(spec, parent);
	if (dialog.exec() != QDialog::Accepted) return QVariantMap();
	return dialog.values();
}
