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
#ifndef TERMINALSETTINGSWIDGET_H
#define TERMINALSETTINGSWIDGET_H

#include "faseterminal.h"

#include <QPointer>
#include <QWidget>

class QComboBox;
class QLineEdit;
class QETProject;

/**
	@brief The TerminalSettingsWidget class is the form of a terminal: its
	strip (one of the project or a new name), its number, its colour, its
	type and its cross-section. Used to place terminals and in the
	"Terminal" tab of their properties.
*/
class TerminalSettingsWidget : public QWidget
{
	Q_OBJECT

	public:
		TerminalSettingsWidget(QETProject *project, QWidget *parent = nullptr);

		FaseTerminal::Settings settings() const;
		void setSettings(const FaseTerminal::Settings &settings);
			/// Propose the next free number whenever the strip changes
		void setProposeNumbers(bool propose) {m_propose = propose;}

	signals:
		void changed();

	private:
		void selectData(QComboBox *combo, const QVariant &data,
						const QString &other_text);
		void stripChanged();
		void emitChanged();

		QPointer<QETProject> m_project;
		QComboBox *m_strip = nullptr;
		QLineEdit *m_number = nullptr;
		QComboBox *m_color = nullptr;
		QComboBox *m_type = nullptr;
		QComboBox *m_section = nullptr;
		QComboBox *m_level = nullptr;
		bool m_propose = false;
		bool m_filling = false;
};

#endif // TERMINALSETTINGSWIDGET_H
