/*
  quickscenepreviewwidget.cpp

  This file is part of GammaRay, the Qt application inspection and
  manipulation tool.

  Copyright (C) 2014-2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Anton Kreuzkamp <anton.kreuzkamp@kdab.com>

  Licensees holding valid commercial KDAB GammaRay licenses may use this file in
  accordance with GammaRay Commercial License Agreement provided with the Software.

  Contact info@kdab.com if any conditions of this licensing are not clear to you.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "quickscenepreviewwidget.h"
#include "quickinspectorinterface.h"

#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QAction>
#include <QComboBox>
#include <QLabel>
#include <QToolBar>

#include <cmath>

using namespace GammaRay;

QuickScenePreviewWidget::QuickScenePreviewWidget(QuickInspectorInterface *inspector, QWidget *parent)
  : RemoteViewWidget(parent),
    m_inspectorInterface(inspector)
{
  // Toolbar
  m_toolBar.toolbarWidget = new QToolBar(this);
  m_toolBar.toolbarWidget->setAutoFillBackground(true);

  m_toolBar.visualizeGroup = new QActionGroup(this);
  m_toolBar.visualizeGroup->setExclusive(false); // we need 0 or 1 selected, not exactly 1
  m_toolBar.visualizeClipping = new QAction(QIcon(QStringLiteral(":/gammaray/plugins/quickinspector/visualize-clipping.png")), tr("Visualize Clipping"), this);
  m_toolBar.visualizeClipping->setActionGroup(m_toolBar.visualizeGroup);
  m_toolBar.visualizeClipping->setCheckable(true);
  m_toolBar.visualizeClipping->setToolTip(tr("<b>Visualize Clipping</b><br/>"
      "Items with the property <i>clip</i> set to true, will cut off their and their "
      "children's rendering at the items' bounds. While this is a handy feature it "
      "comes with quite some cost, like disabling some performance optimizations.<br/>"
      "With this tool enabled the QtQuick renderer highlights items, that have clipping "
      "enabled, so you can check for items, that have clipping enabled unnecessarily. "));
  m_toolBar.toolbarWidget->addAction(m_toolBar.visualizeClipping);
  connect(m_toolBar.visualizeClipping, SIGNAL(triggered(bool)), this, SLOT(visualizeActionTriggered(bool)));

  m_toolBar.visualizeOverdraw = new QAction(QIcon(QStringLiteral(":/gammaray/plugins/quickinspector/visualize-overdraw.png")), tr("Visualize Overdraw"), this);
  m_toolBar.visualizeOverdraw->setActionGroup(m_toolBar.visualizeGroup);
  m_toolBar.visualizeOverdraw->setCheckable(true);
  m_toolBar.visualizeOverdraw->setToolTip(tr("<b>Visualize Overdraw</b><br/>"
      "The QtQuick renderer doesn't detect if an item is obscured by another "
      "opaque item, is completely outside the scene or outside a clipped ancestor and "
      "thus doesn't need to be rendered. You thus need to take care of setting "
      "<i>visible: false</i> for hidden items, yourself.<br/>"
      "With this tool enabled the QtQuick renderer draws a 3D-Box visualizing the "
      "layers of items that are drawn."));
  m_toolBar.toolbarWidget->addAction(m_toolBar.visualizeOverdraw);
  connect(m_toolBar.visualizeOverdraw, SIGNAL(triggered(bool)), this, SLOT(visualizeActionTriggered(bool)));

  m_toolBar.visualizeBatches = new QAction(QIcon(QStringLiteral(":/gammaray/plugins/quickinspector/visualize-batches.png")), tr("Visualize Batches"), this);
  m_toolBar.visualizeBatches->setActionGroup(m_toolBar.visualizeGroup);
  m_toolBar.visualizeBatches->setCheckable(true);
  m_toolBar.visualizeBatches->setToolTip(tr("<b>Visualize Batches</b><br/>"
      "Where a traditional 2D API, such as QPainter, Cairo or Context2D, is written to "
      "handle thousands of individual draw calls per frame, OpenGL is a pure hardware "
      "API and performs best when the number of draw calls is very low and state "
      "changes are kept to a minimum. Therefore the QtQuick renderer combines the "
      "rendering of similar items into single batches.<br/>"
      "Some settings (like <i>clip: true</i>) will cause the batching to fail, though, "
      "causing items to be rendered separately. With this tool enabled the QtQuick "
      "renderer visualizes those batches, by drawing all items that are batched using "
      "the same color. The fewer colors you see in this mode the better."));
  m_toolBar.toolbarWidget->addAction(m_toolBar.visualizeBatches);
  connect(m_toolBar.visualizeBatches, SIGNAL(triggered(bool)), this, SLOT(visualizeActionTriggered(bool)));

  m_toolBar.visualizeChanges = new QAction(QIcon(QStringLiteral(":/gammaray/plugins/quickinspector/visualize-changes.png")), tr("Visualize Changes"), this);
  m_toolBar.visualizeChanges->setActionGroup(m_toolBar.visualizeGroup);
  m_toolBar.visualizeChanges->setCheckable(true);
  m_toolBar.visualizeChanges->setToolTip(tr("<b>Visualize Changes</b><br>"
      "The QtQuick scene is only repainted, if some item changes in a visual manner. "
      "Unnecessary repaints can have a bad impact on the performance. With this tool "
      "enabled, the QtQuick renderer will thus on each repaint highlight the item(s), "
      "that caused the repaint."));
  m_toolBar.toolbarWidget->addAction(m_toolBar.visualizeChanges);
  connect(m_toolBar.visualizeChanges, SIGNAL(triggered(bool)), this, SLOT(visualizeActionTriggered(bool)));

  m_toolBar.toolbarWidget->addSeparator();


  QActionGroup *mouseToolGroup = new QActionGroup(this);
  mouseToolGroup->setExclusive(true);
  connect(mouseToolGroup, SIGNAL(triggered(QAction*)), this, SLOT(setMouseTool(QAction*)));

  m_toolBar.movePreview = new QAction(QIcon(QStringLiteral(":/gammaray/plugins/quickinspector/move-preview.png")), tr("Move Preview"), this);
  m_toolBar.movePreview->setActionGroup(mouseToolGroup);
  m_toolBar.movePreview->setToolTip(tr("<b>Move preview</b><br>"
      "Default mode. Click and drag to move the preview. Won't impact the original "
      "application in any way. "));
  m_toolBar.movePreview->setCheckable(true);
  m_toolBar.movePreview->setChecked(true);
  m_toolBar.toolbarWidget->addAction(m_toolBar.movePreview);

  m_toolBar.measurePixels = new QAction(QIcon(QStringLiteral(":/gammaray/plugins/quickinspector/measure-pixels.png")), tr("Measure Pixel Sizes"), this);
  m_toolBar.measurePixels->setActionGroup(mouseToolGroup);
  m_toolBar.measurePixels->setToolTip(tr("<b>Measure pixel-sizes</b><br>"
      "Choose this mode, click somewhere and drag to measure the distance between the "
      "point you clicked and the point where your mouse pointer is. (Measured in scene "
      "coordinates)."));
  m_toolBar.measurePixels->setCheckable(true);
  m_toolBar.toolbarWidget->addAction(m_toolBar.measurePixels);

  m_toolBar.redirectInput = new QAction(QIcon(QStringLiteral(":/gammaray/plugins/quickinspector/redirect-input.png")), tr("Redirect Input"), this);
  m_toolBar.redirectInput->setActionGroup(mouseToolGroup);
  m_toolBar.redirectInput->setToolTip(tr("<b>Redirect Input</b><br>"
      "In this mode all mouse input is redirected directly to the original application,"
      "so you can control the application directly from within GammaRay."));
  m_toolBar.redirectInput->setCheckable(true);
  m_toolBar.toolbarWidget->addAction(m_toolBar.redirectInput);

  m_toolBar.toolbarWidget->addSeparator();


  m_toolBar.zoomCombobox = new QComboBox(this);
  m_toolBar.zoomCombobox->setModel(zoomLevelModel());
  connect(m_toolBar.zoomCombobox, SIGNAL(currentIndexChanged(int)), this, SLOT(setZoomFromCombobox(int)));
  connect(this, &RemoteViewWidget::zoomChanged, this, &QuickScenePreviewWidget::updateEffectiveGeometry);
  connect(this, &RemoteViewWidget::zoomLevelChanged, m_toolBar.zoomCombobox, &QComboBox::setCurrentIndex);
  m_toolBar.zoomCombobox->setCurrentIndex(zoomLevelIndex());

  m_toolBar.toolbarWidget->addWidget(m_toolBar.zoomCombobox);

  m_toolBar.measureLabel = new QLabel(this);
  m_toolBar.toolbarWidget->addWidget(m_toolBar.measureLabel);
  connect(this, &RemoteViewWidget::measurementChanged, this, [this](const QRectF &r) {
      m_toolBar.measureLabel->setText(QStringLiteral(" %1px").arg(std::sqrt((qreal)(r.width()*r.width() + r.height()*r.height())), 0, 'f', 2));
  });

  setUnavailableText(tr("No remote view available.\n(This happens e.g. when the window is minimized or the scene is hidden)"));
}

QuickScenePreviewWidget::~QuickScenePreviewWidget()
{
}

void QuickScenePreviewWidget::resizeEvent(QResizeEvent *e)
{
    m_toolBar.toolbarWidget->setGeometry(0, 0, width(), m_toolBar.toolbarWidget->sizeHint().height());
    RemoteViewWidget::resizeEvent(e);
}

void QuickScenePreviewWidget::drawDecoration(QPainter* p)
{
  p->save();

  // bounding box
  p->setPen(QColor(232, 87, 82, 170));
  p->setBrush(QBrush(QColor(232, 87, 82, 95)));
  p->drawRect(m_effectiveGeometry.boundingRect);

  // original geometry
  if (m_effectiveGeometry.itemRect != m_effectiveGeometry.boundingRect) {
    p->setPen(Qt::gray);
    p->setBrush(QBrush(Qt::gray, Qt::BDiagPattern));
    p->drawRect(m_effectiveGeometry.itemRect);
  }

  // children rect
  if (m_effectiveGeometry.itemRect != m_effectiveGeometry.boundingRect && m_effectiveGeometry.transform.isIdentity()) {
    // If this item is transformed the children rect will be painted wrongly,
    // so for now skip painting it.
    p->setPen(QColor(0, 99, 193, 170));
    p->setBrush(QBrush(QColor(0, 99, 193, 95)));
    p->drawRect(m_effectiveGeometry.childrenRect);
  }

  // transform origin
  if (m_effectiveGeometry.itemRect != m_effectiveGeometry.boundingRect) {
    p->setPen(QColor(156, 15, 86, 170));
    p->drawEllipse(m_effectiveGeometry.transformOriginPoint, 2.5, 2.5);
    p->drawLine(m_effectiveGeometry.transformOriginPoint - QPointF(0, 6), m_effectiveGeometry.transformOriginPoint + QPointF(0, 6));
    p->drawLine(m_effectiveGeometry.transformOriginPoint - QPointF(6, 0), m_effectiveGeometry.transformOriginPoint + QPointF(6, 0));
  }

  // x and y values
  p->setPen(QColor(136, 136, 136));
  if (!m_effectiveGeometry.left &&
      !m_effectiveGeometry.horizontalCenter &&
      !m_effectiveGeometry.right &&
      m_effectiveGeometry.x != 0) {
    QPointF parentEnd = (QPointF(m_effectiveGeometry.itemRect.x() - m_effectiveGeometry.x, m_effectiveGeometry.itemRect.y()));
    QPointF itemEnd = m_effectiveGeometry.itemRect.topLeft();
    drawArrow(p, parentEnd, itemEnd);
    p->drawText(QRectF(parentEnd.x(), parentEnd.y() + 10, itemEnd.x() - parentEnd.x(), 50),
                Qt::AlignHCenter | Qt::TextDontClip,
                QStringLiteral("x: %1px").arg(m_effectiveGeometry.x / zoom()));
  }
  if (!m_effectiveGeometry.top &&
      !m_effectiveGeometry.verticalCenter &&
      !m_effectiveGeometry.bottom &&
      !m_effectiveGeometry.baseline &&
      m_effectiveGeometry.y != 0) {
    QPointF parentEnd = (QPointF(m_effectiveGeometry.itemRect.x(), m_effectiveGeometry.itemRect.y() - m_effectiveGeometry.y));
    QPointF itemEnd = m_effectiveGeometry.itemRect.topLeft();
    drawArrow(p, parentEnd, itemEnd);
    p->drawText(QRectF(parentEnd.x() + 10, parentEnd.y(), 100, itemEnd.y() - parentEnd.y()),
                Qt::AlignVCenter | Qt::TextDontClip,
                QStringLiteral("y: %1px").arg(m_effectiveGeometry.y / zoom()));
  }

  // anchors
  if (m_effectiveGeometry.left) {
    drawAnchor(p, Qt::Horizontal,
               m_effectiveGeometry.itemRect.left(), m_effectiveGeometry.leftMargin,
               QStringLiteral("margin: %1px").arg(m_effectiveGeometry.leftMargin / zoom()));
  }

  if (m_effectiveGeometry.horizontalCenter) {
    drawAnchor(p, Qt::Horizontal,
               (m_effectiveGeometry.itemRect.left() + m_effectiveGeometry.itemRect.right()) / 2, m_effectiveGeometry.horizontalCenterOffset,
               QStringLiteral("offset: %1px").arg(m_effectiveGeometry.horizontalCenterOffset / zoom()));
  }

  if (m_effectiveGeometry.right) {
    drawAnchor(p, Qt::Horizontal,
               m_effectiveGeometry.itemRect.right(), -m_effectiveGeometry.rightMargin,
               QStringLiteral("margin: %1px").arg(m_effectiveGeometry.rightMargin / zoom()));
  }

  if (m_effectiveGeometry.top) {
    drawAnchor(p, Qt::Vertical,
               m_effectiveGeometry.itemRect.top(), m_effectiveGeometry.topMargin,
               QStringLiteral("margin: %1px").arg(m_effectiveGeometry.topMargin / zoom()));
  }

  if (m_effectiveGeometry.verticalCenter) {
    drawAnchor(p, Qt::Vertical,
               (m_effectiveGeometry.itemRect.top() + m_effectiveGeometry.itemRect.bottom()) / 2, m_effectiveGeometry.verticalCenterOffset,
               QStringLiteral("offset: %1px").arg(m_effectiveGeometry.verticalCenterOffset / zoom()));
  }

  if (m_effectiveGeometry.bottom) {
    drawAnchor(p, Qt::Vertical,
               m_effectiveGeometry.itemRect.bottom(), -m_effectiveGeometry.bottomMargin,
               QStringLiteral("margin: %1px").arg(m_effectiveGeometry.bottomMargin / zoom()));
  }

  if (m_effectiveGeometry.baseline) {
    drawAnchor(p, Qt::Vertical,
               m_effectiveGeometry.itemRect.top(), m_effectiveGeometry.baselineOffset,
               QStringLiteral("offset: %1px").arg(m_effectiveGeometry.baselineOffset / zoom()));
  }

  p->restore();
}

void QuickScenePreviewWidget::drawArrow(QPainter *p, QPointF first, QPointF second)
{
  p->drawLine(first, second);
  QPointF vector(second - first);
  QMatrix m;
  m.rotate(30);
  QVector2D v1 = QVector2D(m.map(vector)).normalized() * 10;
  m.rotate(-60);
  QVector2D v2 = QVector2D(m.map(vector)).normalized() * 10;
  p->drawLine(first, first + v1.toPointF());
  p->drawLine(first, first + v2.toPointF());
  p->drawLine(second, second - v1.toPointF());
  p->drawLine(second, second - v2.toPointF());
}

void QuickScenePreviewWidget::drawAnchor(QPainter *p, Qt::Orientation orientation,
                                       qreal ownAnchorLine, qreal offset,
                                       const QString &label)
{
  qreal foreignAnchorLine = ownAnchorLine - offset;
  QPen pen(QColor(139, 179, 0));

  // Margin arrow
  if (offset) {
    p->setPen(pen);
    if (orientation == Qt::Horizontal) {
      drawArrow(p,
                QPointF(foreignAnchorLine, (m_effectiveGeometry.itemRect.top() + m_effectiveGeometry.itemRect.bottom()) / 2),
                QPointF(ownAnchorLine, (m_effectiveGeometry.itemRect.top() + m_effectiveGeometry.itemRect.bottom()) / 2));
    } else {
      drawArrow(p,
                QPointF((m_effectiveGeometry.itemRect.left() + m_effectiveGeometry.itemRect.right()) / 2, foreignAnchorLine),
                QPointF((m_effectiveGeometry.itemRect.left() + m_effectiveGeometry.itemRect.right()) / 2, ownAnchorLine));
    }

    // Margin text
    if (orientation == Qt::Horizontal) {
      p->drawText(
        QRectF(foreignAnchorLine,
               (m_effectiveGeometry.itemRect.top() + m_effectiveGeometry.itemRect.bottom()) / 2 + 10, offset, 50),
        Qt::AlignHCenter | Qt::TextDontClip,
        label);
    } else {
      p->drawText(
        QRectF((m_effectiveGeometry.itemRect.left() + m_effectiveGeometry.itemRect.right()) / 2 + 10,
               foreignAnchorLine, 100, offset),
        Qt::AlignVCenter | Qt::TextDontClip,
        label);
    }
  }

  // Own Anchor line
  pen.setWidth(2);
  p->setPen(pen);
  if (orientation == Qt::Horizontal) {
    p->drawLine(ownAnchorLine, m_effectiveGeometry.itemRect.top(), ownAnchorLine, m_effectiveGeometry.itemRect.bottom());
  } else {
    p->drawLine(m_effectiveGeometry.itemRect.left(), ownAnchorLine, m_effectiveGeometry.itemRect.right(), ownAnchorLine);
  }

  // Foreign Anchor line
  pen.setStyle(Qt::DotLine);
  p->setPen(pen);
  if (orientation == Qt::Horizontal) {
    p->drawLine(foreignAnchorLine, 0, foreignAnchorLine, image().height() * zoom());
  } else {
    p->drawLine(0, foreignAnchorLine, image().width() * zoom(), foreignAnchorLine);
  }
}

void QuickScenePreviewWidget::mouseMoveEvent(QMouseEvent* e)
{
  if (interactionMode() == InputRedirection) {
    m_inspectorInterface->sendMouseEvent(e->type(), mapToSource(e->pos()), e->button(), e->buttons(), e->modifiers());
  }
  RemoteViewWidget::mouseMoveEvent(e);
}

void QuickScenePreviewWidget::mousePressEvent(QMouseEvent* e)
{
  if (interactionMode() == ViewInteraction && e->modifiers() & Qt::ControlModifier) { // TODO: make this easier to discover!
    m_inspectorInterface->pickItemAt(mapToSource(e->pos()));
  } else if (interactionMode() == InputRedirection) {
     m_inspectorInterface->sendMouseEvent(e->type(), mapToSource(e->pos()), e->button(), e->buttons(), e->modifiers());
  }
  RemoteViewWidget::mousePressEvent(e);
}

void QuickScenePreviewWidget::mouseReleaseEvent(QMouseEvent* e)
{
  if (interactionMode() == InputRedirection) {
    m_inspectorInterface->sendMouseEvent(e->type(), mapToSource(e->pos()), e->button(), e->buttons(), e->modifiers());
  }
  RemoteViewWidget::mouseReleaseEvent(e);
}

void QuickScenePreviewWidget::keyPressEvent(QKeyEvent* e)
{
  if (interactionMode() == InputRedirection) {
    m_inspectorInterface->sendKeyEvent(e->type(), e->key(), e->modifiers(), e->text(), e->isAutoRepeat(), e->count());
  }
  RemoteViewWidget::keyPressEvent(e);
}

void QuickScenePreviewWidget::keyReleaseEvent(QKeyEvent* e)
{
  if (interactionMode() == InputRedirection) {
    m_inspectorInterface->sendKeyEvent(e->type(), e->key(), e->modifiers(), e->text(), e->isAutoRepeat(), e->count());
  }
  RemoteViewWidget::keyReleaseEvent(e);
}

void QuickScenePreviewWidget::wheelEvent(QWheelEvent* e)
{
  if (interactionMode() == InputRedirection) {
    m_inspectorInterface->sendWheelEvent(mapToSource(e->pos()), e->pixelDelta(), e->angleDelta(), e->buttons(), e->modifiers());
  }
  RemoteViewWidget::wheelEvent(e);
}

void QuickScenePreviewWidget::setItemGeometry(const QuickItemGeometry &itemGeometry)
{
  m_itemGeometry = itemGeometry;

  updateEffectiveGeometry();
  update();
}

void QuickScenePreviewWidget::updateEffectiveGeometry()
{
  m_effectiveGeometry = m_itemGeometry;

  m_effectiveGeometry.itemRect               = QRectF(m_itemGeometry.itemRect.topLeft() * zoom(), m_itemGeometry.itemRect.bottomRight() * zoom());
  m_effectiveGeometry.boundingRect           = QRectF(m_itemGeometry.boundingRect.topLeft() * zoom(), m_itemGeometry.boundingRect.bottomRight() * zoom());
  m_effectiveGeometry.childrenRect           = QRectF(m_itemGeometry.childrenRect.topLeft() * zoom(), m_itemGeometry.childrenRect.bottomRight() * zoom());
  m_effectiveGeometry.transformOriginPoint   = m_itemGeometry.transformOriginPoint * zoom();
  m_effectiveGeometry.leftMargin             = m_itemGeometry.leftMargin * zoom();
  m_effectiveGeometry.horizontalCenterOffset = m_itemGeometry.horizontalCenterOffset * zoom();
  m_effectiveGeometry.rightMargin            = m_itemGeometry.rightMargin * zoom();
  m_effectiveGeometry.topMargin              = m_itemGeometry.topMargin * zoom();
  m_effectiveGeometry.verticalCenterOffset   = m_itemGeometry.verticalCenterOffset * zoom();
  m_effectiveGeometry.bottomMargin           = m_itemGeometry.bottomMargin * zoom();
  m_effectiveGeometry.baselineOffset         = m_itemGeometry.baselineOffset * zoom();
  m_effectiveGeometry.x                      = m_itemGeometry.x * zoom();
  m_effectiveGeometry.y                      = m_itemGeometry.y * zoom();
}

void QuickScenePreviewWidget::setZoomFromCombobox(int)
{
  setZoom(m_toolBar.zoomCombobox->currentData().toReal());
}

void QuickScenePreviewWidget::visualizeActionTriggered(bool checked)
{
  static bool recursionGuard = false;
  if (recursionGuard)
      return;

  if (!checked) {
    m_inspectorInterface->setCustomRenderMode(QuickInspectorInterface::NormalRendering);
  } else {
    // QActionGroup requires exactly one selected, but we need 0 or 1 selected
    const auto current = sender();
    recursionGuard = true;
    foreach (auto action, m_toolBar.visualizeGroup->actions()) {
      if (action != current)
        action->setChecked(false);
    }
    recursionGuard = false;
    m_inspectorInterface->setCustomRenderMode(current == m_toolBar.visualizeClipping ? QuickInspectorInterface::VisualizeClipping
                                            : current == m_toolBar.visualizeBatches ? QuickInspectorInterface::VisualizeBatches
                                            : current == m_toolBar.visualizeOverdraw ? QuickInspectorInterface::VisualizeOverdraw
                                            : current == m_toolBar.visualizeChanges ? QuickInspectorInterface::VisualizeChanges
                                            : QuickInspectorInterface::NormalRendering
    );
  }
}

void QuickScenePreviewWidget::setMouseTool(QAction *action)
{
  setInteractionMode(action == m_toolBar.measurePixels ? Measuring
              : action == m_toolBar.redirectInput ? InputRedirection
              : ViewInteraction);
}

void GammaRay::QuickScenePreviewWidget::setSupportsCustomRenderModes(QuickInspectorInterface::Features supportedCustomRenderModes)
{
  m_toolBar.visualizeClipping->setEnabled(supportedCustomRenderModes & QuickInspectorInterface::CustomRenderModeClipping);
  m_toolBar.visualizeBatches->setEnabled(supportedCustomRenderModes & QuickInspectorInterface::CustomRenderModeBatches);
  m_toolBar.visualizeOverdraw->setEnabled(supportedCustomRenderModes & QuickInspectorInterface::CustomRenderModeOverdraw);
  m_toolBar.visualizeChanges->setEnabled(supportedCustomRenderModes & QuickInspectorInterface::CustomRenderModeChanges);
}
