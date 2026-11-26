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
#ifndef TITLEBLOCKDESIGNER_H
#define TITLEBLOCKDESIGNER_H

#include "titleblockdesign.h"

#include <QDialog>
#include <QPointer>
#include <QWidget>

#include <memory>

class Diagram;
class QCheckBox;
class QComboBox;
class QETProject;
class QFormLayout;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QToolButton;
class TitleBlockTemplate;

/**
	@brief The TitleBlockDesignPreview class shows a design as the folio
	will, with the folio's own data, and is where its blocks are handled:
	a click picks a block, a drag of the line between two blocks shares
	their width out again, a drag of a block puts it elsewhere in the row.
*/
class TitleBlockDesignPreview : public QWidget
{
	Q_OBJECT

	public:
		explicit TitleBlockDesignPreview(QWidget *parent = nullptr);
		~TitleBlockDesignPreview() override;

		void setDesign(const TitleBlockDesign::Design &design);
		void setContext(const DiagramContext &context);
			/// The width of the title block on the folio, pixels
		void setTemplateWidth(int width);
		void setSelected(int index);
		int selected() const {return m_selected;}

			/// Where block @p index lies in the widget, and the block at @p x
		QRectF blockRect(int index) const;
		int blockAt(qreal x) const;

		QSize sizeHint() const override;
		QSize minimumSizeHint() const override;

	signals:
		void blockSelected(int index);
		void widthsChanged(int index, int width, int next_width);
		void blockMoved(int from, int to);
		void removeRequested(int index);

	protected:
		void paintEvent(QPaintEvent *event) override;
		void mousePressEvent(QMouseEvent *event) override;
		void mouseMoveEvent(QMouseEvent *event) override;
		void mouseReleaseEvent(QMouseEvent *event) override;
		void keyPressEvent(QKeyEvent *event) override;

	private:
		qreal scale() const;
		QPointF origin() const;
			/// The boundary after block @p index the point is on, -1 for none
		int boundaryAt(qreal x) const;
		int dropIndex(qreal x) const;

		TitleBlockDesign::Design m_design;
		std::unique_ptr<TitleBlockTemplate> m_template;
		QList<QPair<int, int>> m_spans;
		DiagramContext m_context;
		int m_template_width = 1400;
		int m_selected = -1;

		enum class Drag {None, Resize, Move};
		Drag m_drag = Drag::None;
		int m_drag_index = -1;
		qreal m_press_x = 0;
		int m_press_widths[2] = {0, 0};
		bool m_moved = false;
		qreal m_drop_x = -1;
};

/**
	@brief The TitleBlockDesigner class makes a title block of blocks
	(Brandon, 2026-11-19, "the original one I do not like at all"): the
	blocks to add above, the title block as the folio shows it in the
	middle, what the picked block shows below, the style around. It
	keeps the design in the project as an ordinary title block template,
	and gives it to the folio or to every folio of the project in one
	step of undo.
*/
class TitleBlockDesigner : public QDialog
{
	Q_OBJECT

	public:
		TitleBlockDesigner(QETProject *project, Diagram *diagram, QWidget *parent = nullptr);

		TitleBlockDesign::Design design() const {return m_design;}
		void setDesign(const TitleBlockDesign::Design &design);
		void addBlock(TitleBlockDesign::Kind kind);
		void removeBlock(int index);
		void selectBlock(int index);
		int selectedBlock() const;

			/// Every folio of the project, not only the one the
			/// designer was opened on
		bool forEveryFolio() const;
		void setForEveryFolio(bool every);

			/// Keep the design in the project and give it to the folios;
			/// false, saying why, when it cannot
		bool apply();
			/// A name a template can have, from what the user wrote
		static QString templateName(const QString &text);

	private:
		void designChanged();
		void rebuildBlockPanel();
		void updatePalette();
		void chooseLogo();
		void accept() override;

		QPointer<QETProject> m_project;
		QPointer<Diagram> m_diagram;
		TitleBlockDesign::Design m_design;

		TitleBlockDesignPreview *m_preview = nullptr;
		QList<QPair<TitleBlockDesign::Kind, QPushButton *>> m_palette;
		QLineEdit *m_name = nullptr;
		QComboBox *m_style = nullptr;
		QSpinBox *m_height = nullptr;
		QToolButton *m_accent = nullptr;
		QCheckBox *m_accent_bar = nullptr;
		QWidget *m_block_panel = nullptr;
		QFormLayout *m_block_form = nullptr;
		QRadioButton *m_this_folio = nullptr;
		QRadioButton *m_every_folio = nullptr;
		bool m_updating = false;
};

#endif // TITLEBLOCKDESIGNER_H
