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
#ifndef ACCOUNTBUTTONS_H
#define ACCOUNTBUTTONS_H

#include "../utils/fasecadservices.h"

#include <QDialog>
#include <QWidget>

#include <functional>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QToolButton;

/**
	The buttons of the account at the right of the command tabs (Brandon,
	2026-11-26): "Report a bug", "Sign in" and "Request support". Signing
	in is offered once the web service of fasecad.com is on; the other two
	once the account signed in has a subscription running.
*/
class AccountButtons : public QWidget
{
	Q_OBJECT

	public:
		explicit AccountButtons(QWidget *parent = nullptr);
			/// Follow what FaseCadServices knows
		void update();
			/// The sign-in window, then the session of this computer on
			/// the server: when the account is open on another computer,
			/// asks whether to close it there. Signing out too. True when
			/// the account changed
		static bool signIn(QWidget *parent);
			/// The question asked when the account is open on @p computer
			/// since @p since; true to close it there. The tests change it
		static std::function<bool(QWidget *, const QString &, const QDateTime &)> askTakeOver;

	private:
		void write(FaseCadServices::Kind kind);

		QToolButton *m_bug = nullptr;
		QToolButton *m_sign_in = nullptr;
		QToolButton *m_support = nullptr;
};

/**
	The window to write a bug report or a request for support, sent to
	fasecad.com with the version of FaseCAD and the system.
*/
class SupportDialog : public QDialog
{
	Q_OBJECT

	public:
		SupportDialog(FaseCadServices::Kind kind, QWidget *parent = nullptr);
		void send();

	private:
		FaseCadServices::Kind m_kind;
		QLineEdit *m_subject = nullptr;
		QPlainTextEdit *m_text = nullptr;
		QLabel *m_status = nullptr;
		QPushButton *m_send = nullptr;
};

#endif // ACCOUNTBUTTONS_H
