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
#ifndef FASETERMINAL_H
#define FASETERMINAL_H

#include "../ElementsCollection/elementslocation.h"
#include "../properties/elementdata.h"

#include <QList>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUndoCommand>
#include <QVector>

class Element;
class QETProject;

/**
	@brief The FaseTerminal namespace: the terminal device of the toolbar.

	A terminal is placed with its strip, its number, its colour, its
	type and its cross-section chosen in one form (TerminalSettingsWidget),
	and the same form edits them afterwards in the "Terminal" tab of its
	properties. The values are kept where the terminal manager keeps
	them -- the label, the information terminal_color and
	terminal_section, the terminal type of the element data -- so the
	terminal manager, the strip drawings and the terminal diagram read
	them as they read any terminal.

	The label is written "-STRIP:NUMBER" ("-XPERS:9"), and the terminal is
	put into the strip named STRIP, made when the project has none.
*/
namespace FaseTerminal
{
		/// What the form edits
	struct Settings
	{
		QString strip;
		QString number;
			/// TerminalColor spec, empty for none
		QString color;
		ElementData::TerminalType type = ElementData::TTGeneric;
		bool led = false;
			/// Cross-section in mm2 written with a dot, empty for none
		QString section;
			/// Level of a terminal of several levels, from 1 at the bottom:
			/// the terminals of the strip with the same number are one
		int level = 1;

		bool operator==(const Settings &other) const;
		bool operator!=(const Settings &other) const {return !(*this == other);}
	};

		/// A colour of the form: its name, its short code, its spec
	struct ColorChoice
	{
		QString name;
		QString code;
		QString spec;
	};
	QVector<ColorChoice> colorChoices();
		/// The short code of a colour spec ("RD"), empty if unknown
	QString colorCode(const QString &spec);

		/// A type of the form: its name, the type and the led it stands for
	struct TypeChoice
	{
		QString name;
		ElementData::TerminalType type;
		bool led;
	};
	QVector<TypeChoice> typeChoices();
		/// The name of a type in the form ("Fuse"), "Normal" for the generic
	QString typeName(ElementData::TerminalType type, bool led);

	QStringList sections();

	QString label(const QString &strip, const QString &number);
		/// The strip and the number a label names ("-XPERS:9")
	void parseLabel(const QString &label, QString *strip, QString *number);

		/// The settings @p element has now
	Settings settingsOf(const Element *element);
		/// The strip of the nearest terminal on the left of @p element on
		/// its folio (the terminal before it, left to right, then top to
		/// bottom), the one a terminal without strip name belongs to
	QString inheritedStrip(const Element *element);
		/// The strips of @p project, by name
	QStringList stripNames(QETProject *project);
		/// The number after the highest one used in @p strip of @p project,
		/// the terminals of @p skip left out
	QString nextNumber(QETProject *project, const QString &strip,
					   const QSet<const Element *> &skip = {});

		/// A pasted terminal and the settings of the one it is a copy of
	struct Pasted
	{
		QPointer<Element> element;
		Settings settings;
	};
		/// The terminals among @p elements which name a strip, read before
		/// the paste erases their labels
	QList<Pasted> pastedTerminals(const QList<Element *> &elements);
		/// Give the pasted terminals the next free numbers of their strips,
		/// in the order of the numbers they were copied from, and put them
		/// into their strips: commands made children of @p parent
	void numberPasted(QETProject *project, const QList<Pasted> &pasted,
					  QUndoCommand *parent);

		/// The terminal symbol in the collection of @p project, put there
		/// from the program's resources the first time
	ElementsLocation symbol(QETProject *project);

	/**
		@brief The ChangeCommand class gives a terminal its settings: the
		fields they are kept in, on top of what the element has when the
		command is done (an edit of the Information tab made in the same
		step is kept), and its place in the strip they name, moving it out
		of the strip it was in and making the strip when needed.
	*/
	class ChangeCommand : public QUndoCommand
	{
		public:
			ChangeCommand(Element *element, const Settings &settings,
						  QUndoCommand *parent = nullptr);
			~ChangeCommand() override;
			void redo() override;
			void undo() override;

		private:
			void build();
			void step(QUndoCommand *command);
			void scheduleLabels();

			QList<QUndoCommand *> m_steps;

			QPointer<Element> m_element;
			Settings m_settings;
			bool m_built = false;
	};
}

#endif // FASETERMINAL_H
