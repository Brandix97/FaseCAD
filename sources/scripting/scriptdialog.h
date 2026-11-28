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
#ifndef SCRIPTDIALOG_H
#define SCRIPTDIALOG_H

#include <QDialog>
#include <QHash>
#include <QVariantMap>

class QDialogButtonBox;
class QWidget;

/**
	@brief The ScriptDialog class is the window a script or a plugin asks
	for with qet.dialog(): it describes it, FaseCAD draws it with its own
	widgets, and gets back what the user filled in.
	@code
	var answer = qet.dialog({
		title: "Export to the ERP",
		text: "Which folios?",
		ok: "Export",
		fields: [
			{id: "name", label: "Order", type: "text", value: "PO-1"},
			{id: "copies", label: "Copies", type: "number", value: 2, min: 1, max: 10},
			{id: "price", label: "Price", type: "decimal", value: 2.5, decimals: 2},
			{id: "format", label: "Format", type: "choice", items: ["CSV", "XML"], value: "XML"},
			{id: "spares", label: "With spare parts", type: "check", value: true},
			{id: "notes", label: "Notes", type: "multiline"},
			{id: "rows", label: "Devices", type: "table", columns: ["Label", "Qty"],
			 value: [["-K1", "1"], ["-K2", "2"]]}
		]
	});
	if (answer) qet.log(answer.name + " " + answer.copies);
	@endcode
	exec() gives the values by id, or an empty map when the user cancels.
*/
class ScriptDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit ScriptDialog(const QVariantMap &spec, QWidget *parent = nullptr);

			/// The fields of @p spec this window could not draw, and why
		QStringList errors() const {return m_errors;}
			/// What each field holds now, by its id
		QVariantMap values() const;
			/// The widget of the field @p id, for the tests
		QWidget *field(const QString &id) const {return m_fields.value(id);}

			/// Shows the window for @p spec: the values, or an empty
			/// map when the user cancels
		static QVariantMap ask(const QVariantMap &spec, QWidget *parent = nullptr);

	private:
		QHash<QString, QWidget *> m_fields;
		QStringList m_order;
		QHash<QString, QString> m_types;
		QStringList m_errors;
		QDialogButtonBox *m_buttons = nullptr;
};

#endif // SCRIPTDIALOG_H
