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
#include "titleblockdesigner.h"

#include "../diagram.h"
#include "../qetproject.h"
#include "../titleblockproperties.h"
#include "../titleblocktemplate.h"
#include "templatescollection.h"

#include <QBuffer>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QToolButton>
#include <QUndoCommand>
#include <QUndoStack>
#include <QVBoxLayout>

using namespace TitleBlockDesign;

namespace
{
		/// Pixels around the title block in the preview
	const int margin = 14;
		/// How near the line between two blocks a press takes it
	const int boundary_grip = 5;
		/// The narrowest a block may be, percent
	const int narrowest = 3;

	int mmToPx(int mm) {return qRound(mm * 96.0 / 25.4);}
	int pxToMm(int px) {return qRound(px * 25.4 / 96.0);}

		/// Take @p item out of the way: its widgets go once the event
		/// being handled is over, a button may be the one clicked
	void discard(QLayoutItem *item)
	{
		if (!item) return;
		if (QWidget *widget = item->widget()) {
			widget->hide();
			widget->setObjectName(QString());
			widget->deleteLater();
			delete item;
		} else if (QLayout *layout = item->layout()) {
			while (QLayoutItem *child = layout->takeAt(0)) discard(child);
			delete layout;
		} else {
			delete item;
		}
	}

	/**
		Gives folios another title block, and their old one back on undo
	*/
	class UseTitleBlockCommand : public QUndoCommand
	{
		public:
			struct Change
			{
				QPointer<Diagram> diagram;
				TitleBlockProperties before;
				TitleBlockProperties after;
			};

			explicit UseTitleBlockCommand(const QString &name)
			{
				setText(QCoreApplication::translate("TitleBlockDesigner", "Use the title block %1")
						.arg(name));
			}
			void add(const Change &change) {m_changes << change;}
			bool isEmpty() const {return m_changes.isEmpty();}
			void redo() override {apply(true);}
			void undo() override {apply(false);}

		private:
			void apply(bool after)
			{
				for (const Change &change : m_changes)
				{
					if (!change.diagram) continue;
					change.diagram->border_and_titleblock.importTitleBlock(
								after ? change.after : change.before);
					change.diagram->invalidate(
								change.diagram->border_and_titleblock.borderAndTitleBlockRect());
				}
			}

			QList<Change> m_changes;
	};
}

TitleBlockDesignPreview::TitleBlockDesignPreview(QWidget *parent) :
	QWidget(parent)
{
	setFocusPolicy(Qt::StrongFocus);
	setMouseTracking(true);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

TitleBlockDesignPreview::~TitleBlockDesignPreview() = default;

void TitleBlockDesignPreview::setDesign(const Design &design)
{
	m_design = design;
	m_template.reset(build(design));
	m_spans = blockSpans(design, m_template_width);
	if (m_selected >= m_design.blocks.size()) m_selected = m_design.blocks.size() - 1;
	updateGeometry();
	update();
}

void TitleBlockDesignPreview::setContext(const DiagramContext &context)
{
	m_context = context;
	update();
}

void TitleBlockDesignPreview::setTemplateWidth(int width)
{
	m_template_width = qMax(300, width);
	m_spans = blockSpans(m_design, m_template_width);
	update();
}

void TitleBlockDesignPreview::setSelected(int index)
{
	m_selected = index;
	update();
}

QSize TitleBlockDesignPreview::sizeHint() const
{
	return QSize(900, int(m_design.height * 900.0 / m_template_width) + 2 * margin + 18);
}

QSize TitleBlockDesignPreview::minimumSizeHint() const
{
	return QSize(400, 100);
}

qreal TitleBlockDesignPreview::scale() const
{
	return (width() - 2.0 * margin) / m_template_width;
}

QPointF TitleBlockDesignPreview::origin() const
{
	const qreal height = m_design.height * scale();
	return QPointF(margin, qMax<qreal>(margin + 14, (this->height() - height) / 2));
}

QRectF TitleBlockDesignPreview::blockRect(int index) const
{
	if (index < 0 || index >= m_spans.size()) return QRectF();
	const qreal s = scale();
	const QPointF o = origin();
	return QRectF(o.x() + m_spans.at(index).first * s, o.y(),
				  (m_spans.at(index).second - m_spans.at(index).first) * s,
				  m_design.height * s);
}

int TitleBlockDesignPreview::blockAt(qreal x) const
{
	for (int i = 0 ; i < m_spans.size() ; ++i)
	{
		const QRectF rect = blockRect(i);
		if (x >= rect.left() && x < rect.right()) return i;
	}
	return -1;
}

int TitleBlockDesignPreview::boundaryAt(qreal x) const
{
	for (int i = 0 ; i + 1 < m_spans.size() ; ++i)
	{
		if (qAbs(blockRect(i).right() - x) <= boundary_grip) return i;
	}
	return -1;
}

int TitleBlockDesignPreview::dropIndex(qreal x) const
{
	int index = 0;
	for (int i = 0 ; i < m_spans.size() ; ++i)
	{
		if (x > blockRect(i).center().x()) index = i + 1;
	}
	return index;
}

void TitleBlockDesignPreview::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event)
	QPainter painter(this);
	painter.fillRect(rect(), palette().window());
	const qreal s = scale();
	const QPointF o = origin();
	const QRectF paper(o, QSizeF(m_template_width * s, m_design.height * s));
	painter.fillRect(paper.adjusted(-6, -6, 6, 6), Diagram::background_color);

	if (m_template) {
		painter.save();
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setRenderHint(QPainter::TextAntialiasing);
		painter.translate(o);
		painter.scale(s, s);
		m_template->render(painter, m_context, m_template_width);
		painter.restore();
	}

	if (m_design.blocks.isEmpty()) {
		painter.setPen(palette().color(QPalette::PlaceholderText));
		painter.drawText(paper, Qt::AlignCenter, tr("Add the blocks of the title block with the buttons above"));
		return;
	}

	painter.setRenderHint(QPainter::Antialiasing);
	const QColor accent = m_design.accent.isValid() ? m_design.accent : QColor(0xFA, 0x13, 0x0C);
	if (m_selected >= 0 && m_selected < m_spans.size()) {
		const QRectF selected = blockRect(m_selected).adjusted(-1, -1, 1, 1);
		painter.setPen(QPen(accent, 2));
		painter.setBrush(Qt::NoBrush);
		painter.drawRect(selected);
			//Its name on a tab above it
		const QString name = kindName(m_design.blocks.at(m_selected).kind);
		QFont font = this->font();
		font.setBold(true);
		painter.setFont(font);
		const QFontMetrics metrics(font);
		const QRectF tab(selected.left(), selected.top() - metrics.height() - 2,
						 metrics.horizontalAdvance(name) + 10, metrics.height() + 2);
		painter.fillRect(tab, accent);
		painter.setPen(Qt::white);
		painter.drawText(tab, Qt::AlignCenter, name);
	}
	if (m_drag == Drag::Move && m_moved && m_drop_x >= 0) {
		painter.setPen(QPen(accent, 3));
		painter.drawLine(QPointF(m_drop_x, paper.top() - 4), QPointF(m_drop_x, paper.bottom() + 4));
	}
}

void TitleBlockDesignPreview::mousePressEvent(QMouseEvent *event)
{
	if (event->button() != Qt::LeftButton) {
		QWidget::mousePressEvent(event);
		return;
	}
	setFocus();
	const qreal x = event->position().x();
	m_press_x = x;
	m_moved = false;
	const int boundary = boundaryAt(x);
	if (boundary >= 0) {
		m_drag = Drag::Resize;
		m_drag_index = boundary;
		m_press_widths[0] = m_design.blocks.at(boundary).width;
		m_press_widths[1] = m_design.blocks.at(boundary + 1).width;
		return;
	}
	const int index = blockAt(x);
	if (index >= 0) {
		m_drag = Drag::Move;
		m_drag_index = index;
		emit blockSelected(index);
	}
}

void TitleBlockDesignPreview::mouseMoveEvent(QMouseEvent *event)
{
	const qreal x = event->position().x();
	if (m_drag == Drag::None) {
		setCursor(boundaryAt(x) >= 0 ? Qt::SizeHorCursor
									 : (blockAt(x) >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor));
		return;
	}
	if (m_drag == Drag::Resize) {
			//The two blocks share what they had
		int total = 0;
		for (const Block &block : m_design.blocks) total += block.width;
		const qreal percent_per_pixel = total / (m_template_width * scale());
		const int pair = m_press_widths[0] + m_press_widths[1];
		const int width = qBound(narrowest,
								 m_press_widths[0] + qRound((x - m_press_x) * percent_per_pixel),
								 pair - narrowest);
		if (width != m_design.blocks.at(m_drag_index).width) {
			emit widthsChanged(m_drag_index, width, pair - width);
		}
		return;
	}
	if (qAbs(x - m_press_x) > 6) m_moved = true;
	if (m_moved) {
		setCursor(Qt::ClosedHandCursor);
		const int drop = dropIndex(x);
		m_drop_x = drop < m_spans.size() ? blockRect(drop).left() : blockRect(m_spans.size() - 1).right();
		update();
	}
}

void TitleBlockDesignPreview::mouseReleaseEvent(QMouseEvent *event)
{
	if (m_drag == Drag::Move && m_moved) {
		int to = dropIndex(event->position().x());
			//Taken out of the row first, it leaves a gap before the drop
		if (to > m_drag_index) --to;
		if (to != m_drag_index) emit blockMoved(m_drag_index, to);
	}
	m_drag = Drag::None;
	m_drag_index = -1;
	m_moved = false;
	m_drop_x = -1;
	unsetCursor();
	update();
}

void TitleBlockDesignPreview::keyPressEvent(QKeyEvent *event)
{
	if (m_selected >= 0 && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)) {
		emit removeRequested(m_selected);
		return;
	}
	if (m_selected >= 0 && (event->modifiers() & Qt::ControlModifier)
		&& (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right)) {
		const int to = m_selected + (event->key() == Qt::Key_Left ? -1 : 1);
		if (to >= 0 && to < m_design.blocks.size()) emit blockMoved(m_selected, to);
		return;
	}
	if (!m_design.blocks.isEmpty() && (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right)) {
		const int to = qBound(0, m_selected + (event->key() == Qt::Key_Left ? -1 : 1),
							  m_design.blocks.size() - 1);
		emit blockSelected(to);
		return;
	}
	QWidget::keyPressEvent(event);
}

TitleBlockDesigner::TitleBlockDesigner(QETProject *project, Diagram *diagram, QWidget *parent) :
	QDialog(parent),
	m_project(project),
	m_diagram(diagram)
{
	setWindowTitle(tr("Title block designer"));
	resize(1100, 640);

		//The design the folio has, else one for its paper
	int template_width = 1400;
	bool found = false;
	if (m_diagram) {
		template_width = qRound(m_diagram->border_and_titleblock.titleBlockRect().width());
		if (template_width < 300) template_width = 1400;
		const QString current = m_diagram->border_and_titleblock.titleBlockTemplateName();
		if (m_project && !current.isEmpty()) {
			const QDomElement xml = m_project->embeddedTitleBlockTemplatesCollection()
					->getTemplateXmlDescription(current);
			found = !xml.isNull() && fromTemplateXml(xml, m_design);
		}
	}
	if (!found) m_design = template_width < 1300 ? compactDesign() : completeDesign();

	auto *layout = new QVBoxLayout(this);

		//The blocks to add
	auto *palette_row = new QHBoxLayout();
	palette_row->addWidget(new QLabel(tr("Add a block:"), this));
	for (Kind kind : kinds())
	{
		auto *button = new QPushButton(kindName(kind), this);
		button->setObjectName(QStringLiteral("add-") + kindToString(kind));
		button->setToolTip(tr("Adds the block \"%1\" at the end of the title block").arg(kindName(kind)));
		connect(button, &QPushButton::clicked, this, [this, kind]() {addBlock(kind);});
		palette_row->addWidget(button);
		m_palette << qMakePair(kind, button);
	}
	palette_row->addStretch();
	layout->addLayout(palette_row);

	m_preview = new TitleBlockDesignPreview(this);
	m_preview->setObjectName(QStringLiteral("m_preview"));
	m_preview->setTemplateWidth(template_width);
	if (m_diagram) {
		m_preview->setContext(m_diagram->border_and_titleblock.titleblockInformation());
	} else if (m_project) {
		m_preview->setContext(m_project->projectWideProperties());
	}
	layout->addWidget(m_preview, 1);
	auto *hint = new QLabel(tr("Click a block to change it. Drag the line between two blocks to share "
							   "their width, drag a block to move it, Delete takes it away."), this);
	hint->setWordWrap(true);
	hint->setEnabled(false);
	layout->addWidget(hint);

	auto *bottom = new QHBoxLayout();
	m_block_panel = new QGroupBox(this);
	m_block_form = new QFormLayout(m_block_panel);
	bottom->addWidget(m_block_panel, 3);

	auto *whole = new QGroupBox(tr("Title block"), this);
	auto *whole_form = new QFormLayout(whole);
	m_name = new QLineEdit(m_design.name, this);
	m_name->setObjectName(QStringLiteral("m_name"));
	whole_form->addRow(tr("Name:"), m_name);
	m_style = new QComboBox(this);
	m_style->addItem(tr("Modern: the label small above the value"), int(Style::Modern));
	m_style->addItem(tr("Classic: label and value on one line"), int(Style::Classic));
	whole_form->addRow(tr("Style:"), m_style);
	m_height = new QSpinBox(this);
	m_height->setRange(10, 80);
	m_height->setSuffix(QStringLiteral(" mm"));
	whole_form->addRow(tr("Height:"), m_height);
	auto *accent_row = new QHBoxLayout();
	m_accent = new QToolButton(this);
	m_accent->setToolTip(tr("Colour of the bar and of the headers"));
	m_accent_bar = new QCheckBox(tr("Bar along the left side"), this);
	accent_row->addWidget(m_accent);
	accent_row->addWidget(m_accent_bar);
	accent_row->addStretch();
	whole_form->addRow(tr("Accent:"), accent_row);
	auto *start = new QComboBox(this);
	start->addItem(tr("Start again from…"));
	start->addItem(tr("Complete, for A3"));
	start->addItem(tr("Compact, for A4"));
	whole_form->addRow(QString(), start);
	bottom->addWidget(whole, 2);
	layout->addLayout(bottom);

	auto *footer = new QHBoxLayout();
	footer->addWidget(new QLabel(tr("Give it to:"), this));
	m_this_folio = new QRadioButton(tr("this sheet"), this);
	m_every_folio = new QRadioButton(tr("every sheet of the project"), this);
	m_every_folio->setObjectName(QStringLiteral("m_every_folio"));
	m_this_folio->setChecked(true);
	m_this_folio->setEnabled(m_diagram);
	if (!m_diagram) m_every_folio->setChecked(true);
	footer->addWidget(m_this_folio);
	footer->addWidget(m_every_folio);
	footer->addStretch();
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	buttons->button(QDialogButtonBox::Ok)->setEnabled(m_project && !m_project->isReadOnly());
	connect(buttons, &QDialogButtonBox::accepted, this, &TitleBlockDesigner::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	footer->addWidget(buttons);
	layout->addLayout(footer);

	connect(m_preview, &TitleBlockDesignPreview::blockSelected, this, &TitleBlockDesigner::selectBlock);
	connect(m_preview, &TitleBlockDesignPreview::removeRequested, this, &TitleBlockDesigner::removeBlock);
	connect(m_preview, &TitleBlockDesignPreview::widthsChanged, this,
			[this](int index, int width, int next_width) {
		if (index < 0 || index + 1 >= m_design.blocks.size()) return;
		m_design.blocks[index].width = width;
		m_design.blocks[index + 1].width = next_width;
		designChanged();
	});
	connect(m_preview, &TitleBlockDesignPreview::blockMoved, this, [this](int from, int to) {
		if (from < 0 || from >= m_design.blocks.size() || to < 0 || to >= m_design.blocks.size()) return;
		m_design.blocks.move(from, to);
		designChanged();
		selectBlock(to);
	});
	connect(m_name, &QLineEdit::textEdited, this, [this](const QString &text) {m_design.name = text;});
	connect(m_style, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
		if (m_updating) return;
		m_design.style = Style(m_style->currentData().toInt());
		m_accent_bar->setEnabled(m_design.style == Style::Modern);
		designChanged();
	});
	connect(m_height, qOverload<int>(&QSpinBox::valueChanged), this, [this](int mm) {
		if (m_updating) return;
		m_design.height = mmToPx(mm);
		designChanged();
	});
	connect(m_accent, &QToolButton::clicked, this, [this]() {
		const QColor color = QColorDialog::getColor(m_design.accent, this, tr("Accent colour"));
		if (!color.isValid()) return;
		m_design.accent = color;
		setDesign(m_design);
	});
	connect(m_accent_bar, &QCheckBox::toggled, this, [this](bool on) {
		if (m_updating) return;
		m_design.accent_bar = on;
		designChanged();
	});
	connect(start, qOverload<int>(&QComboBox::activated), this, [this, start](int index) {
		if (index <= 0) return;
		Design design = index == 1 ? completeDesign() : compactDesign();
		design.name = m_design.name;
		design.logo = m_design.logo;
		design.logo_type = m_design.logo_type;
		design.accent = m_design.accent;
		setDesign(design);
		start->setCurrentIndex(0);
	});

	setDesign(m_design);
	selectBlock(m_design.blocks.isEmpty() ? -1 : 0);
}

void TitleBlockDesigner::setDesign(const Design &design)
{
	m_design = design;
	m_updating = true;
	m_name->setText(m_design.name);
	m_style->setCurrentIndex(qMax(0, m_style->findData(int(m_design.style))));
	m_height->setValue(pxToMm(m_design.height));
	m_accent_bar->setChecked(m_design.accent_bar);
	m_accent_bar->setEnabled(m_design.style == Style::Modern);
	QPixmap swatch(28, 16);
	swatch.fill(m_design.accent);
	m_accent->setIcon(QIcon(swatch));
	m_accent->setIconSize(swatch.size());
	m_updating = false;
	designChanged();
	rebuildBlockPanel();
}

void TitleBlockDesigner::designChanged()
{
	m_preview->setDesign(m_design);
	updatePalette();
	if (auto *width = m_block_panel->findChild<QSpinBox *>(QStringLiteral("width"))) {
		const int index = selectedBlock();
		if (index >= 0 && index < m_design.blocks.size()) {
			const QSignalBlocker blocker(width);
			width->setValue(m_design.blocks.at(index).width);
		}
	}
}

void TitleBlockDesigner::updatePalette()
{
	for (const auto &pair : m_palette)
	{
		bool present = false;
		for (const Block &block : m_design.blocks) present = present || block.kind == pair.first;
		pair.second->setEnabled(!present);
	}
}

void TitleBlockDesigner::addBlock(Kind kind)
{
	for (const Block &block : m_design.blocks)
	{
		if (block.kind == kind) return;
	}
	Block block;
	block.kind = kind;
	block.width = defaultWidth(kind);
	m_design.blocks << block;
	normalizeWidths(m_design);
	designChanged();
	selectBlock(m_design.blocks.size() - 1);
}

void TitleBlockDesigner::removeBlock(int index)
{
	if (index < 0 || index >= m_design.blocks.size()) return;
	m_design.blocks.removeAt(index);
	normalizeWidths(m_design);
	designChanged();
	selectBlock(qMin(index, m_design.blocks.size() - 1));
}

void TitleBlockDesigner::selectBlock(int index)
{
	m_preview->setSelected(index);
	rebuildBlockPanel();
}

int TitleBlockDesigner::selectedBlock() const
{
	return m_preview->selected();
}

void TitleBlockDesigner::rebuildBlockPanel()
{
	while (m_block_form->rowCount())
	{
		const QFormLayout::TakeRowResult row = m_block_form->takeRow(0);
		discard(row.labelItem);
		discard(row.fieldItem);
	}
	auto *box = static_cast<QGroupBox *>(m_block_panel);
	const int index = selectedBlock();
	if (index < 0 || index >= m_design.blocks.size()) {
		box->setTitle(tr("Block"));
		m_block_form->addRow(new QLabel(tr("Pick a block in the title block."), m_block_panel));
		return;
	}
	const Block &block = m_design.blocks.at(index);
	box->setTitle(tr("Block: %1").arg(kindName(block.kind)));

	auto *width = new QSpinBox(m_block_panel);
	width->setObjectName(QStringLiteral("width"));
	width->setRange(narrowest, 100 - narrowest * (m_design.blocks.size() - 1));
	width->setSuffix(QStringLiteral(" %"));
	width->setValue(block.width);
	connect(width, qOverload<int>(&QSpinBox::valueChanged), this, [this, index](int value) {
		if (index >= m_design.blocks.size() || m_design.blocks.size() < 2) return;
			//The block beside it gives or takes the difference
		const int neighbour = index + 1 < m_design.blocks.size() ? index + 1 : index - 1;
		const int pair = m_design.blocks.at(index).width + m_design.blocks.at(neighbour).width;
		const int bounded = qBound(narrowest, value, pair - narrowest);
		m_design.blocks[index].width = bounded;
		m_design.blocks[neighbour].width = pair - bounded;
		designChanged();
	});
	m_block_form->addRow(tr("Width:"), width);

	const QStringList names = fieldNames(block.kind);
	for (int field = 0 ; field < names.size() ; ++field)
	{
		auto *row = new QHBoxLayout();
		auto *edit = new QLineEdit(text(block, field), m_block_panel);
		edit->setObjectName(QStringLiteral("field%1").arg(field));
		edit->setToolTip(tr("Text, and data between %{ and }: %{projecttitle}"));
		auto *data = new QToolButton(m_block_panel);
		data->setText(tr("Data"));
		data->setPopupMode(QToolButton::InstantPopup);
		auto *menu = new QMenu(data);
		for (const auto &variable : variables())
		{
			menu->addAction(QStringLiteral("%1   %{%2}").arg(variable.second, variable.first),
							edit, [edit, variable]() {
				edit->insert(QStringLiteral("%{%1}").arg(variable.first));
			});
		}
		data->setMenu(menu);
		row->addWidget(edit, 1);
		row->addWidget(data);
		m_block_form->addRow(names.at(field) + QLatin1Char(':'), row);
		connect(edit, &QLineEdit::textChanged, this, [this, index, field](const QString &value) {
			if (index >= m_design.blocks.size()) return;
			Block &changed = m_design.blocks[index];
			while (changed.texts.size() <= field) changed.texts << text(changed, changed.texts.size());
			changed.texts[field] = value;
			designChanged();
		});
	}

	if (block.kind == Kind::Revisions) {
		auto *rows = new QComboBox(m_block_panel);
		for (int count : {2, 3, 5}) rows->addItem(QString::number(count), count);
		rows->setCurrentIndex(qMax(0, rows->findData(m_design.revision_rows)));
		connect(rows, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, rows]() {
			m_design.revision_rows = rows->currentData().toInt();
			designChanged();
		});
		m_block_form->addRow(tr("Revisions shown:"), rows);
		auto *where = new QLabel(tr("The revisions are those of the project: Project > Title block data."),
								 m_block_panel);
		where->setWordWrap(true);
		m_block_form->addRow(where);
	}
	if (block.kind == Kind::Logo) {
		auto *row = new QHBoxLayout();
		auto *choose = new QPushButton(tr("Choose an image…"), m_block_panel);
		auto *remove = new QPushButton(tr("No logo"), m_block_panel);
		remove->setEnabled(!m_design.logo.isEmpty());
		row->addWidget(choose);
		row->addWidget(remove);
		row->addStretch();
		m_block_form->addRow(tr("Image:"), row);
		connect(choose, &QPushButton::clicked, this, &TitleBlockDesigner::chooseLogo);
		connect(remove, &QPushButton::clicked, this, [this]() {
			m_design.logo.clear();
			m_design.logo_type.clear();
			designChanged();
			rebuildBlockPanel();
		});
	}

	auto *take_away = new QPushButton(tr("Take the block away"), m_block_panel);
	connect(take_away, &QPushButton::clicked, this, [this, index]() {removeBlock(index);});
	m_block_form->addRow(take_away);
}

void TitleBlockDesigner::chooseLogo()
{
	const QString path = QFileDialog::getOpenFileName(
				this, tr("Logo of the title block"), QString(),
				tr("Images (*.svg *.png *.jpg *.jpeg *.bmp)"));
	if (path.isEmpty()) return;
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) return;
	const QByteArray data = file.readAll();
	if (QFileInfo(path).suffix().compare(QLatin1String("svg"), Qt::CaseInsensitive) == 0) {
		m_design.logo = data;
		m_design.logo_type = QStringLiteral("svg");
	} else {
			//Any image Qt reads, kept as PNG
		QImage image;
		if (!image.loadFromData(data)) return;
		QBuffer buffer(&m_design.logo);
		m_design.logo.clear();
		buffer.open(QIODevice::WriteOnly);
		image.save(&buffer, "PNG");
		m_design.logo_type = QStringLiteral("png");
	}
	designChanged();
	rebuildBlockPanel();
}

bool TitleBlockDesigner::forEveryFolio() const
{
	return m_every_folio->isChecked();
}

void TitleBlockDesigner::setForEveryFolio(bool every)
{
	(every ? m_every_folio : m_this_folio)->setChecked(true);
}

QString TitleBlockDesigner::templateName(const QString &text)
{
	QString name = text.trimmed().toLower();
	name.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("-"));
	name.remove(QRegularExpression(QStringLiteral("[^a-z0-9_.-]")));
	return name;
}

bool TitleBlockDesigner::apply()
{
	if (!m_project || m_project->isReadOnly()) return false;
	const QString name = templateName(m_name->text());
	if (name.isEmpty()) {
		QMessageBox::warning(this, windowTitle(),
							 tr("Give the title block a name: letters, digits, \"-\" and \"_\"."));
		return false;
	}
	if (m_design.blocks.isEmpty()) {
		QMessageBox::warning(this, windowTitle(), tr("The title block has no block."));
		return false;
	}

	TitleBlockTemplatesProjectCollection *collection = m_project->embeddedTitleBlockTemplatesCollection();
	const QDomElement existing = collection->getTemplateXmlDescription(name);
	Design ignored;
	if (!existing.isNull() && !fromTemplateXml(existing, ignored)) {
		const auto answer = QMessageBox::question(
					this, windowTitle(),
					tr("The project already has a title block template called %1, not made "
					   "with the designer. Replace it?").arg(name));
		if (answer != QMessageBox::Yes) return false;
	}

	m_design.name = name;
	QDomDocument document;
	const QDomElement xml = templateXml(m_design, document);
	document.appendChild(xml);
	if (!collection->setTemplateXmlDescription(name, xml)) return false;

	QList<Diagram *> folios;
	if (forEveryFolio()) {
		folios = m_project->diagrams();
	} else if (m_diagram) {
		folios << m_diagram;
	}
	auto *command = new UseTitleBlockCommand(name);
	for (Diagram *folio : folios)
	{
		folio->ensureLoaded();
		const TitleBlockProperties before = folio->border_and_titleblock.exportTitleBlock();
		if (before.template_name == name && before.collection == QET::Embedded) continue;
		TitleBlockProperties after = before;
		after.template_name = name;
		after.collection = QET::Embedded;
		command->add({folio, before, after});
	}
	if (command->isEmpty()) {
		delete command;
	} else {
		m_project->undoStack()->push(command);
	}
	return true;
}

void TitleBlockDesigner::accept()
{
	if (apply()) QDialog::accept();
}
