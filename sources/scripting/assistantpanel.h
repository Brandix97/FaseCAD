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
#ifndef ASSISTANTPANEL_H
#define ASSISTANTPANEL_H

#include <QFrame>

class AssistantSession;
class QVariantAnimation;
class QToolButton;
class QETDiagramEditor;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTextBrowser;

/**
	Rings going out of the round "IA" button and fading (Brandon,
	2026-11-26), to catch the eye; under the button, letting the mouse
	through to the drawing.
*/
class AssistantRipple : public QWidget
{
	Q_OBJECT

	public:
		explicit AssistantRipple(QWidget *parent);
			/// Room around the button for the rings, px
		static int reach() {return 26;}
		void setRunning(bool running);
		bool isRunning() const;

	protected:
		void paintEvent(QPaintEvent *event) override;

	private:
		QVariantAnimation *m_animation = nullptr;
		qreal m_phase = 0;
};

/**
	The "IA" panel of the electrical design window (Brandon, 2026-11-25):
	the user writes what to do, the AI does it on the open project
	(AssistantSession) and says what it did; each script it ran is listed,
	with the undo step it left.

	A round "IA" button floats at the bottom right of @p area, the drawing
	(Brandon, 2026-11-26); the panel opens over the right of it, above the
	button, and follows the area when it is resized.
*/
class AssistantPanel : public QFrame
{
	Q_OBJECT

	public:
		AssistantPanel(QETDiagramEditor *editor, QWidget *area);
		QToolButton *button() const {return m_button;}
		AssistantRipple *ripple() const {return m_ripple;}

		AssistantSession *session() const {return m_session;}
			/// Send what is written
		void send();

	protected:
		bool eventFilter(QObject *watched, QEvent *event) override;

	private:
		void add(const QString &html);
		void place();
		void editSettings();
		void setRunning(bool running);

		AssistantSession *m_session = nullptr;
		QWidget *m_area = nullptr;
		QToolButton *m_button = nullptr;
		AssistantRipple *m_ripple = nullptr;
		QTextBrowser *m_talk = nullptr;
		QPlainTextEdit *m_input = nullptr;
		QPushButton *m_send = nullptr;
		QPushButton *m_stop = nullptr;
		QLabel *m_status = nullptr;
};

#endif // ASSISTANTPANEL_H
