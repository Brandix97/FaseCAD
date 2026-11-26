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
#ifndef BRANDINGTRANSLATOR_H
#define BRANDINGTRANSLATOR_H

#include <QList>
#include <QPointer>
#include <QTranslator>

/**
	@brief The BrandingTranslator class shows the name of FaseCAD where
	the interface says QElectroTech, in every language.

	FaseCAD is a fork of QElectroTech : the sources, the translation
	files and the settings keep the name QElectroTech, so the fork can
	follow QElectroTech and offer its work back. Only the texts the user
	reads change, here, in one place : this translator is installed after
	the others, so Qt asks it first ; it takes the translation the other
	translators give (or the English source) and writes FaseCAD in place
	of QElectroTech. A text without the name is left to the others.

	The credits stay as they are: the About window, and "the QElectroTech
	team" wherever it is named. FaseCAD is based on QElectroTech and the
	GPL asks for its authors to be named.
*/
class BrandingTranslator : public QTranslator
{
	Q_OBJECT

	public:
		explicit BrandingTranslator(QObject *parent = nullptr);

		static QString productName();
		static QString rebrand(const QString &text);
		static bool mentionsBrand(const QString &text);

			/// The translators whose texts are rebranded, asked in order
		void setBases(const QList<QTranslator *> &bases);

		QString translate(const char *context, const char *source_text,
						  const char *disambiguation = nullptr,
						  int n = -1) const override;
		bool isEmpty() const override {return false;}

	private:
		QList<QPointer<QTranslator>> m_bases;
};

#endif // BRANDINGTRANSLATOR_H
