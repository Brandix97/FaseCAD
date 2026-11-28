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
#include "elementformatpattern.h"

#include "../QPropertyUndoCommand/qpropertyundocommand.h"

#include <QCoreApplication>
#include <QMetaProperty>
#include <QObject>
#include <QUndoCommand>

namespace
{
		/// The properties that make the look of a part
	const char *const kFormatProperties[] = {
		"line_style", "line_weight", "filling", "color", "antialias",
		"font", "frame",
		"label_font", "label_color", "label_frame"
	};

	QVariantHash &copiedPattern()
	{
		static QVariantHash pattern;
		return pattern;
	}

		/// The property @p name of @p object, when it can take @p value
	QMetaProperty property(const QObject *object, const QByteArray &name,
						   const QVariant &value)
	{
		const QMetaObject *meta = object->metaObject();
		const int index = meta->indexOfProperty(name.constData());
		if (index < 0) return QMetaProperty();
		QMetaProperty p = meta->property(index);
		if (!p.isWritable() || p.metaType() != value.metaType())
			return QMetaProperty();
		return p;
	}
}

QVariantHash ElementFormatPattern::take(const QObject *part)
{
	QVariantHash pattern;
	if (!part) return pattern;
	const QMetaObject *meta = part->metaObject();
	for (const char *name : kFormatProperties) {
		const int index = meta->indexOfProperty(name);
		if (index < 0) continue;
		const QVariant value = meta->property(index).read(part);
		if (value.isValid()) pattern.insert(QString::fromLatin1(name), value);
	}
	return pattern;
}

QUndoCommand *ElementFormatPattern::applyCommand(const QList<QObject *> &parts,
												 const QVariantHash &pattern)
{
	auto *command = new QUndoCommand(
				QCoreApplication::translate("ElementFormatPattern", "Apply format"));
	int changes = 0;
	for (QObject *part : parts) {
		if (!part) continue;
		for (auto it = pattern.constBegin(); it != pattern.constEnd(); ++it) {
			const QByteArray name = it.key().toLatin1();
			QMetaProperty p = property(part, name, it.value());
			if (!p.isValid()) continue;
			const QVariant old_value = p.read(part);
			if (old_value == it.value()) continue;
				//The command keeps the name: give it one that lives on
			const char *stored = nullptr;
			for (const char *known : kFormatProperties)
				if (name == known) stored = known;
			if (!stored) continue;
			new QPropertyUndoCommand(part, stored, old_value, it.value(), command);
			++changes;
		}
	}
	if (!changes) {
		delete command;
		return nullptr;
	}
	return command;
}

QVariantHash ElementFormatPattern::copied()
{
	return copiedPattern();
}

void ElementFormatPattern::setCopied(const QVariantHash &pattern)
{
	copiedPattern() = pattern;
}

bool ElementFormatPattern::hasCopied()
{
	return !copiedPattern().isEmpty();
}
