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
#ifndef PARTSMATERIAL_H
#define PARTSMATERIAL_H

#include "partsdatabase.h"
#include "../materiallist/materiallist.h"

#include <optional>

/**
	@brief A part of the parts database as an entry of the material file,
	so the element informations take it the way they take an article
	picked from that file (ElementInfoWidget::applyMaterialRecord()):
	the same columns, one block or another, one step of undo.
*/
namespace PartsMaterial
{
		/**
			@return the entry for @p part. The columns of the article
			(manufacturer, order number, description...) are always
			written, empty or not, so the article picked before never
			lingers; @p quantity, given for an accessory, fills the
			quantity too.
		*/
	MaterialRecord record(const Part &part,
						  std::optional<double> quantity = std::nullopt);
}

#endif // PARTSMATERIAL_H
