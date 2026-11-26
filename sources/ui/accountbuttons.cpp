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
#include "accountbuttons.h"

#include "../utils/fasecadaccount.h"

#include <QDialogButtonBox>
#include <QEventLoop>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
		/// A person, head and shoulders, in @p color
	QIcon personIcon(const QColor &color)
	{
		QPixmap pixmap(64, 64);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setPen(Qt::NoPen);
		painter.setBrush(color);
		painter.drawEllipse(QRectF(20, 6, 24, 24));
		QPainterPath body;
		body.moveTo(8, 60);
		body.cubicTo(8, 38, 56, 38, 56, 60);
		body.closeSubpath();
		painter.drawPath(body);
		return QIcon(pixmap);
	}

	QToolButton *button(const QString &name, const QString &text, const QIcon &icon, QWidget *parent)
	{
		auto *each = new QToolButton(parent);
		each->setObjectName(name);
		each->setText(text);
		each->setIcon(icon);
		each->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		each->setAutoRaise(true);
		return each;
	}
}

AccountButtons::AccountButtons(QWidget *parent) :
	QWidget(parent)
{
	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(2);
	m_bug = button(QStringLiteral("account_report_bug"), tr("Report a bug"),
				   style()->standardIcon(QStyle::SP_MessageBoxWarning), this);
	m_sign_in = button(QStringLiteral("account_sign_in"), tr("Sign in"),
					   personIcon(palette().color(QPalette::ButtonText)), this);
	m_support = button(QStringLiteral("account_request_support"), tr("Request support"),
					   style()->standardIcon(QStyle::SP_DialogHelpButton), this);
	layout->addWidget(m_bug);
	layout->addWidget(m_sign_in);
	layout->addWidget(m_support);

	connect(m_sign_in, &QToolButton::clicked, this, [this]() {signIn(window());});
	connect(m_bug, &QToolButton::clicked, this, [this]() {write(FaseCadServices::Kind::Bug);});
	connect(m_support, &QToolButton::clicked, this, [this]() {write(FaseCadServices::Kind::Support);});
	connect(FaseCadServices::instance(), &FaseCadServices::changed, this, &AccountButtons::update);
		//Signed in on another computer: said, without stopping the work
	connect(FaseCadServices::instance(), &FaseCadServices::sessionLost, this, [this]() {
		auto *box = new QMessageBox(QMessageBox::Information, tr("Session closed"),
									tr("Your account was opened on another computer, so this "
									   "session was closed: one computer at a time for each "
									   "account. Sign in again to go on here."),
									QMessageBox::Ok, window());
		box->setObjectName(QStringLiteral("account_session_lost"));
		box->setAttribute(Qt::WA_DeleteOnClose);
		box->open();
	});
	update();
	FaseCadServices::instance()->refresh();
}

void AccountButtons::update()
{
	FaseCadServices *services = FaseCadServices::instance();
	m_sign_in->setEnabled(services->serviceOn());
	if (!services->serviceOn()) {
		m_sign_in->setText(tr("Sign in"));
		m_sign_in->setToolTip(tr("The services of fasecad.com are not open yet"));
	} else if (services->signedIn()) {
		m_sign_in->setText(FaseCadAccount::user());
		m_sign_in->setToolTip(services->subscribed() && services->subscribedUntil().isValid()
							  ? tr("Signed in as %1, subscription until %2. Click to change or sign out.")
								.arg(FaseCadAccount::user(),
									 QLocale().toString(services->subscribedUntil(), QLocale::ShortFormat))
							  : tr("Signed in as %1. Click to change or sign out.").arg(FaseCadAccount::user()));
	} else {
		m_sign_in->setText(tr("Sign in"));
		m_sign_in->setToolTip(tr("Sign in to your fasecad.com account"));
	}
	const bool open = services->serviceOn() && services->signedIn() && services->subscribed();
	const QString closed = !services->serviceOn() || !services->signedIn()
			? tr("Sign in with an account with a subscription to use it")
			: tr("Your account has no subscription running");
	m_bug->setEnabled(open);
	m_bug->setToolTip(open ? tr("Tell the FaseCAD team about something that does not work") : closed);
	m_support->setEnabled(open);
	m_support->setToolTip(open ? tr("Ask the FaseCAD team for help") : closed);
}

std::function<bool(QWidget *, const QString &, const QDateTime &)> AccountButtons::askTakeOver =
		[](QWidget *parent, const QString &computer, const QDateTime &since) {
	const QString where = computer.isEmpty() ? tr("another computer") : computer;
	const QString when = since.isValid()
			? QLocale().toString(since.toLocalTime(), QLocale::ShortFormat) : QString();
	const QString text = when.isEmpty()
			? tr("Your account is open on %1. One computer at a time can use an account: "
				 "close it there and go on here?").arg(where)
			: tr("Your account is open on %1 since %2. One computer at a time can use an "
				 "account: close it there and go on here?").arg(where, when);
	return QMessageBox::question(parent, tr("Account in use"), text,
								 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
};

bool AccountButtons::signIn(QWidget *parent)
{
	FaseCadServices *services = FaseCadServices::instance();
	const FaseCadAccount::Choice choice = FaseCadAccount::askSignIn(parent);
	if (choice == FaseCadAccount::Choice::Cancel) return false;
	if (choice == FaseCadAccount::Choice::SignOut) {
		services->signOut();
		return true;
	}
	bool take = false;
	for (;;) {
		QEventLoop wait;
		FaseCadServices::SignIn result = FaseCadServices::SignIn::Unreachable;
		QString computer;
		QDateTime since;
		const QMetaObject::Connection done = QObject::connect(
					services, &FaseCadServices::signInDone, &wait,
					[&](FaseCadServices::SignIn answer, const QString &where, const QDateTime &when) {
			result = answer;
			computer = where;
			since = when;
			wait.quit();
		});
		services->signIn(take);
		wait.exec();
		QObject::disconnect(done);
		if (result == FaseCadServices::SignIn::Done) return true;
		if (result == FaseCadServices::SignIn::Busy && !take) {
			if (!askTakeOver(parent, computer, since)) return true;
			take = true;
			continue;
		}
		QMessageBox::warning(parent, tr("Sign in"), services->accountError());
		return true;
	}
}

void AccountButtons::write(FaseCadServices::Kind kind)
{
	SupportDialog dialog(kind, window());
	dialog.exec();
}

SupportDialog::SupportDialog(FaseCadServices::Kind kind, QWidget *parent) :
	QDialog(parent),
	m_kind(kind)
{
	setObjectName(QStringLiteral("support_dialog"));
	const bool bug = kind == FaseCadServices::Kind::Bug;
	setWindowTitle(bug ? tr("Report a bug") : tr("Request support"));
	resize(560, 420);
	auto *layout = new QVBoxLayout(this);
	auto *intro = new QLabel(bug ? tr("Say what you did, what happened and what you expected. "
									  "The version of FaseCAD and of the system go with it.")
								 : tr("Say what you need help with. The version of FaseCAD and "
									  "of the system go with it."), this);
	intro->setWordWrap(true);
	layout->addWidget(intro);
	auto *form = new QFormLayout();
	m_subject = new QLineEdit(this);
	m_subject->setObjectName(QStringLiteral("support_subject"));
	m_subject->setMaxLength(200);
	form->addRow(tr("Subject:"), m_subject);
	layout->addLayout(form);
	m_text = new QPlainTextEdit(this);
	m_text->setObjectName(QStringLiteral("support_text"));
	layout->addWidget(m_text, 1);
	m_status = new QLabel(this);
	m_status->setWordWrap(true);
	layout->addWidget(m_status);
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
	m_send = buttons->addButton(tr("Send"), QDialogButtonBox::AcceptRole);
	m_send->setEnabled(false);
	layout->addWidget(buttons);
	auto enable = [this]() {
		m_send->setEnabled(!m_subject->text().trimmed().isEmpty()
						   && !m_text->toPlainText().trimmed().isEmpty());
	};
	connect(m_subject, &QLineEdit::textChanged, this, enable);
	connect(m_text, &QPlainTextEdit::textChanged, this, enable);
	connect(buttons, &QDialogButtonBox::accepted, this, &SupportDialog::send);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(FaseCadServices::instance(), &FaseCadServices::sent, this, [this](bool ok, const QString &message) {
		m_send->setEnabled(true);
		if (ok) {
			QMessageBox::information(this, windowTitle(), tr("Sent. The FaseCAD team will answer you by email."));
			accept();
		} else {
			m_status->setText(message);
		}
	});
}

void SupportDialog::send()
{
	if (m_subject->text().trimmed().isEmpty() || m_text->toPlainText().trimmed().isEmpty()) return;
	m_send->setEnabled(false);
	m_status->setText(tr("Sending..."));
	FaseCadServices::instance()->send(m_kind, m_subject->text(), m_text->toPlainText());
}
