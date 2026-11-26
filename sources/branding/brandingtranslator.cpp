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
#include "brandingtranslator.h"

#include <QRegularExpression>

BrandingTranslator::BrandingTranslator(QObject *parent) :
	QTranslator(parent)
{}

QString BrandingTranslator::productName()
{
	return QStringLiteral("FaseCAD");
}

/**
	@brief BrandingTranslator::rebrand
	@return @p text with FaseCAD in place of QElectroTech, except where
	the QElectroTech team is named, and in place of the short name QET
	inside a phrase ("QET Collection"). QET alone is a sample text, and
	names such as QET_ElementScaler are other programs: both stay. Web
	addresses and file names, written in lower case (qelectrotech.org,
	.qet), are not touched.
*/
QString BrandingTranslator::rebrand(const QString &text)
{
	static const QRegularExpression name(
				QStringLiteral("QElectroTech(?!\\s*[Tt]eam)"));
	static const QRegularExpression short_name(QStringLiteral("\\bQET\\b"));
	QString result = text;
	result.replace(name, productName());
	if (result.trimmed() != QLatin1String("QET")) {
		result.replace(short_name, productName());
	}
	return result;
}

/**
	@brief BrandingTranslator::mentionsBrand
	@return true when @p text names QElectroTech or QET
*/
bool BrandingTranslator::mentionsBrand(const QString &text)
{
	static const QRegularExpression brand(
				QStringLiteral("QElectroTech|\\bQET\\b"));
	return text.contains(brand);
}

void BrandingTranslator::setBases(const QList<QTranslator *> &bases)
{
	m_bases.clear();
	for (QTranslator *base : bases) {
		m_bases << base;
	}
}

/**
	@brief BrandingTranslator::translate
	@return the translation of the other translators, or the English
	source, rebranded ; an empty string, which hands the text over to the
	other translators, when there is nothing to change
*/
QString BrandingTranslator::translate(const char *context,
									  const char *source_text,
									  const char *disambiguation,
									  int n) const
{
	if (!source_text || !context
			|| qstrcmp(context, "AboutQETDialog") == 0) {
		return QString();
	}

	QString text;
	for (const QPointer<QTranslator> &base : m_bases) {
		if (base) {
			text = base->translate(context, source_text, disambiguation, n);
			if (!text.isEmpty()) {
				break;
			}
		}
	}
	if (text.isEmpty()) {
		text = QString::fromUtf8(source_text);
	}
	if (!mentionsBrand(text)) {
		return QString();
	}
	const QString rebranded = rebrand(text);
	return rebranded == text ? QString() : rebranded;
}
