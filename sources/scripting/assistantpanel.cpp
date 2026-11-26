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
#include "assistantpanel.h"

#include "assistantsession.h"
#include "../qetdiagrameditor.h"
#include "../ui/accountbuttons.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPainter>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QToolButton>
#include <QTextBrowser>
#include <QVariantAnimation>
#include <QVBoxLayout>

AssistantRipple::AssistantRipple(QWidget *parent) :
	QWidget(parent),
	m_animation(new QVariantAnimation(this))
{
	setObjectName(QStringLiteral("assistant_ripple"));
	setAttribute(Qt::WA_TransparentForMouseEvents);
	setAttribute(Qt::WA_NoSystemBackground);
	m_animation->setStartValue(0.0);
	m_animation->setEndValue(1.0);
	m_animation->setDuration(2600);
	m_animation->setLoopCount(-1);
	connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
		m_phase = value.toReal();
		update();
	});
}

void AssistantRipple::setRunning(bool running)
{
	if (running && m_animation->state() != QAbstractAnimation::Running)
		m_animation->start();
	else if (!running)
		m_animation->stop();
	setVisible(running);
}

bool AssistantRipple::isRunning() const
{
	return m_animation->state() == QAbstractAnimation::Running;
}

void AssistantRipple::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event)
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	const QPointF centre = QRectF(rect()).center();
	const qreal inner = (width() - 2.0 * reach()) / 2.0;
		//Two rings half a period apart: each grows from the button out
		//to the edge, thinner and fainter as it goes
	for (int ring = 0 ; ring < 2 ; ++ring) {
		qreal t = m_phase + ring * 0.5;
		if (t >= 1) t -= 1;
		const qreal eased = 1 - (1 - t) * (1 - t);
		const qreal radius = inner + eased * (reach() - 2);
		QColor colour(0xFA, 0x13, 0x0C);
		colour.setAlphaF(0.55 * (1 - t));
		painter.setPen(QPen(colour, 3.0 * (1 - t) + 1.0));
		colour.setAlphaF(0.12 * (1 - t));
		painter.setBrush(colour);
		painter.drawEllipse(centre, radius, radius);
	}
}

AssistantPanel::AssistantPanel(QETDiagramEditor *editor, QWidget *area) :
	QFrame(area),
	m_session(new AssistantSession(editor, this)),
	m_area(area)
{
	setObjectName(QStringLiteral("assistant_panel"));
	setFrameShape(QFrame::StyledPanel);
	setAutoFillBackground(true);
	setStyleSheet(QStringLiteral("#assistant_panel {border: 1px solid palette(mid); border-radius: 8px; "
								 "background: palette(window);}"));
	QWidget *content = this;
	auto *layout = new QVBoxLayout(content);
	layout->setContentsMargins(8, 6, 8, 8);
	auto *head = new QHBoxLayout();
	auto *title = new QLabel(QStringLiteral("<b>%1</b>").arg(tr("AI")), content);
	auto *close = new QToolButton(content);
	close->setText(QStringLiteral("×"));
	close->setAutoRaise(true);
	close->setToolTip(tr("Close"));
	auto *settings = new QToolButton(content);
	settings->setObjectName(QStringLiteral("assistant_service"));
	settings->setText(QStringLiteral("⚙"));
	settings->setAutoRaise(true);
	settings->setToolTip(tr("Sign in..."));
	head->addWidget(title);
	head->addStretch();
	head->addWidget(settings);
	head->addWidget(close);
	layout->addLayout(head);

	m_talk = new QTextBrowser(content);
	m_talk->setObjectName(QStringLiteral("assistant_talk"));
	m_talk->setOpenLinks(false);
	m_talk->setPlaceholderText(tr("Tell the AI what to do on the open project: \"add a contactor "
								  "-KM1 with its coil and a stop and start button\", \"number the "
								  "wires of this folio\"... It works on the folio shown, and each "
								  "change it makes can be undone."));
	layout->addWidget(m_talk, 1);

	m_status = new QLabel(content);
	m_status->setObjectName(QStringLiteral("assistant_status"));
	layout->addWidget(m_status);

	m_input = new QPlainTextEdit(content);
	m_input->setObjectName(QStringLiteral("assistant_input"));
	m_input->setPlaceholderText(tr("Write an instruction... (Enter sends, Shift+Enter a new line)"));
	m_input->setMaximumHeight(90);
	m_input->installEventFilter(this);
	layout->addWidget(m_input);

	auto *buttons = new QHBoxLayout();
	m_send = new QPushButton(tr("Send"), content);
	m_send->setObjectName(QStringLiteral("assistant_send"));
	m_stop = new QPushButton(tr("Stop"), content);
	m_stop->setObjectName(QStringLiteral("assistant_stop"));
	m_stop->hide();
	auto *fresh = new QPushButton(tr("New conversation"), content);
	buttons->addWidget(m_send);
	buttons->addWidget(m_stop);
	buttons->addStretch();
	buttons->addWidget(fresh);
	layout->addLayout(buttons);

		//The round button at the bottom right of the drawing, the rings
		//under it
	m_ripple = new AssistantRipple(area);
	m_button = new QToolButton(area);
	m_button->setObjectName(QStringLiteral("assistant_button"));
	m_button->setText(tr("AI"));
	m_button->setToolTip(tr("Tell the AI what to do on the open project"));
	m_button->setCheckable(true);
	m_button->setFixedSize(52, 52);
	m_button->setStyleSheet(QStringLiteral(
		"QToolButton {border-radius: 26px; background: #FA130C; color: white; font-weight: bold; font-size: 15px;}"
		"QToolButton:hover {background: #ff3b33;}"
		"QToolButton:checked {background: #b80d08;}"));
	m_button->show();
	m_button->raise();
	m_ripple->setRunning(true);
	hide();
	connect(m_button, &QToolButton::toggled, this, [this](bool on) {
		setVisible(on);
			//Quiet while the panel is open
		m_ripple->setRunning(!on);
		if (on) {
			place();
			raise();
			m_button->raise();
			m_input->setFocus();
		}
	});
	connect(close, &QToolButton::clicked, m_button, [this]() {m_button->setChecked(false);});
	area->installEventFilter(this);
	place();

	connect(m_send, &QPushButton::clicked, this, &AssistantPanel::send);
	connect(m_stop, &QPushButton::clicked, m_session, &AssistantSession::stop);
	connect(fresh, &QPushButton::clicked, this, [this]() {
		m_session->clear();
		m_talk->clear();
	});
	connect(settings, &QToolButton::clicked, this, &AssistantPanel::editSettings);
	connect(m_session, &AssistantSession::said, this, [this](const QString &text) {
		add(QStringLiteral("<p><b>%1</b> %2</p>").arg(tr("AI:"), text.toHtmlEscaped()
													   .replace(QLatin1Char('\n'), QStringLiteral("<br>"))));
	});
	connect(m_session, &AssistantSession::scriptRan, this,
			[this](const QString &title, bool ok, const QString &undo, const QString &details) {
		const QString mark = ok ? QStringLiteral("<span style=\"color:#1a7f37\">✓</span>")
								: QStringLiteral("<span style=\"color:#b42318\">✗</span>");
		QString line = QStringLiteral("<p style=\"color:gray\">%1 %2").arg(mark, title.toHtmlEscaped());
		if (ok && undo.isEmpty()) line += QStringLiteral(" — ") + tr("nothing changed");
		if (!ok && !details.isEmpty())
			line += QStringLiteral("<br><small>%1</small>").arg(details.left(300).toHtmlEscaped());
		add(line + QStringLiteral("</p>"));
	});
	connect(m_session, &AssistantSession::statusChanged, m_status, &QLabel::setText);
	connect(m_session, &AssistantSession::finished, this,
			[this](const QString &reason, const QString &message) {
		setRunning(false);
		if (reason == QLatin1String("stopped"))
			add(QStringLiteral("<p style=\"color:gray\">%1</p>").arg(tr("Stopped.")));
		else if (!message.isEmpty())
			add(QStringLiteral("<p style=\"color:#b42318\">%1</p>").arg(message.toHtmlEscaped()));
	});
}

void AssistantPanel::send()
{
	const QString text = m_input->toPlainText().trimmed();
	if (text.isEmpty() || m_session->isRunning()) return;
	m_input->clear();
	add(QStringLiteral("<p><b>%1</b> %2</p>").arg(tr("You:"), text.toHtmlEscaped()
												   .replace(QLatin1Char('\n'), QStringLiteral("<br>"))));
	setRunning(true);
	m_session->ask(text);
}

void AssistantPanel::place()
{
	if (!m_area) return;
	const int margin = 16;
	const QRect inside = m_area->rect();
		//Room for the rings inside the drawing
	const int reach = AssistantRipple::reach();
	const int corner = reach + 2;
	m_button->move(inside.right() - corner - m_button->width(),
				   inside.bottom() - corner - m_button->height());
	m_ripple->setGeometry(m_button->geometry().adjusted(-reach, -reach, reach, reach));
	m_ripple->raise();
	m_button->raise();
	const int width = qMin(400, qMax(260, inside.width() / 3));
	const int height = qMax(200, inside.height() - 2 * margin - corner - m_button->height());
	setGeometry(inside.right() - margin - width, inside.top() + margin, width, height);
}

bool AssistantPanel::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == m_area && (event->type() == QEvent::Resize || event->type() == QEvent::Show
							  || event->type() == QEvent::ChildAdded)) {
		place();
		if (isVisible()) raise();
	}
	if (watched == m_input && event->type() == QEvent::KeyPress) {
		auto *key = static_cast<QKeyEvent *>(event);
		if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
			&& !(key->modifiers() & Qt::ShiftModifier)) {
			send();
			return true;
		}
	}
	return QFrame::eventFilter(watched, event);
}

void AssistantPanel::add(const QString &html)
{
	m_talk->append(html);
	m_talk->verticalScrollBar()->setValue(m_talk->verticalScrollBar()->maximum());
}

void AssistantPanel::setRunning(bool running)
{
	m_send->setVisible(!running);
	m_stop->setVisible(running);
	m_input->setEnabled(!running);
	if (!running) m_input->setFocus();
}

void AssistantPanel::editSettings()
{
	AccountButtons::signIn(this);
}
