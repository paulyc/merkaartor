#include "LayerDock.h"
#include "LayerWidget.h"

#include "MainWindow.h"
#ifndef _MOBILE
#include "ui_MainWindow.h"
#endif
#include "MapView.h"
#include "Document.h"
#include "Layer.h"
#include "PropertiesDock.h"
#include "Command.h"
#include "InfoDock.h"

#include <QPushButton>
#include <QDragEnterEvent>
#include <QMenu>

#define LINEHEIGHT 25

#define CHILD_WIDGETS (p->Content->children())
#define CHILD_WIDGET(x) (dynamic_cast<LayerWidget*>(p->Content->children().at(x)))
#define CHILD_LAYER(x) (dynamic_cast<LayerWidget*>(p->Content->children().at(x))->getLayer())

class LayerDockPrivate
{
    public:
        LayerDockPrivate(MainWindow* aMain) :
          Main(aMain), Scroller(0), Content(0), Layout(0), theDropWidget(0),
          lastSelWidget(0)
          {}
    public:
        MainWindow* Main;
        QScrollArea* Scroller;
        QWidget* Content;
        QVBoxLayout* Layout;
        QVBoxLayout* frameLayout;
        QTabBar* tab;
        LayerWidget* theDropWidget;
        LayerWidget* lastSelWidget;
        QMenu* ctxMenu;
        QList<LayerWidget*> selWidgets;
};

LayerDock::LayerDock(MainWindow* aMain)
: MDockAncestor(aMain)
{
    p = new LayerDockPrivate(aMain);
    setMinimumSize(1,1);
    setObjectName("layersDock");
    setAcceptDrops(true);

    createContent();

    retranslateUi();
}

LayerDock::~LayerDock()
{
    delete p;
}

void LayerDock::dragEnterEvent(QDragEnterEvent *event)
{
    p->theDropWidget = nullptr;
    if (event->mimeData()->hasFormat("application/x-layer"))
        if ((p->theDropWidget = dynamic_cast<LayerWidget*>(event->source())))
            event->acceptProposedAction();
}

void LayerDock::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-layer"))
        event->accept();
    else {
        event->ignore();
        return;
    }

    LayerWidget* aW = dynamic_cast<LayerWidget*>(childAt(event->pos()));
    if (aW != p->theDropWidget) {
        if (!aW) {
            p->Layout->removeWidget(p->theDropWidget);
            p->Layout->addWidget(p->theDropWidget);
        } else {
            p->Layout->removeWidget(p->theDropWidget);
            p->Layout->insertWidget(p->Layout->indexOf(aW), p->theDropWidget);
        }
        update();
    }
}

void LayerDock::dragLeaveEvent(QDragLeaveEvent * /*event*/)
{
//	if (p->theDropWidget) {
//		p->Layout->removeWidget(p->theDropWidget);
//		SAFE_DELETE(p->theDropWidget);
//	}
}

void LayerDock::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-layer"))
        event->accept();
    else {
        event->ignore();
        return;
    }

    p->Main->document()->moveLayer(p->theDropWidget->getLayer(), p->Layout->indexOf(p->theDropWidget));
    emit(layersChanged(false));
    update();
}

void LayerDock::clearLayers()
{
    for (int i=CHILD_WIDGETS.size()-1; i >= 0; i--) {
        if (!CHILD_WIDGET(i))
            continue;
        CHILD_WIDGET(i)->deleteLater();
    }
}

void LayerDock::addLayer(Layer* aLayer)
{
    LayerWidget* w = aLayer->newWidget();
    if (w) {
        p->Layout->addWidget(w);

        connect(w, SIGNAL(layerChanged(LayerWidget*,bool)), this, SLOT(layerChanged(LayerWidget*,bool)));
        connect(w, SIGNAL(layerClosed(Layer*)), this, SLOT(layerClosed(Layer*)));
        connect(w, SIGNAL(layerCleared(Layer*)), this, SLOT(layerCleared(Layer*)));
        connect(w, SIGNAL(layerZoom(Layer*)), this, SLOT(layerZoom(Layer*)));
        connect(w, SIGNAL(layerProjection(const QString&)), this, SLOT(layerProjection(const QString&)));

#ifndef _MOBILE
        p->Main->ui->menuLayers->addMenu(w->getAssociatedMenu());
#endif

        //w->setChecked(aLayer->isSelected());
        w->setVisible(aLayer->isEnabled());
        w->setEnabled(aLayer->isEnabled());
        w->setChecked(aLayer->isSelected());
        w->getAssociatedMenu()->menuAction()->setVisible(aLayer->isEnabled());

        update();
    }
}

void LayerDock::deleteLayer(Layer* aLayer)
{
    for (int i=CHILD_WIDGETS.size()-1; i >= 0; i--) {
        if (!CHILD_WIDGET(i))
            continue;
        if (CHILD_LAYER(i) == aLayer) {
#ifndef _MOBILE
            p->Main->ui->menuLayers->removeAction(CHILD_WIDGET(i)->getAssociatedMenu()->menuAction());
#endif
            LayerWidget* curW = CHILD_WIDGET(i);
            curW->deleteLater();
            break;
        }
    }

    update();
}

Layer* LayerDock::getSelectedLayer()
{
    for (int i=CHILD_WIDGETS.size()-1; i >= 0; i--) {
        if (!CHILD_WIDGET(i))
            continue;
        if (CHILD_WIDGET(i)->isChecked()) {
            return CHILD_LAYER(i);
        }
    }

    return nullptr;
}

void LayerDock::createContent()
{
    delete p->Scroller;

    QWidget* frame = new QWidget();
    p->frameLayout = new QVBoxLayout(frame);
    p->frameLayout->setMargin(0);
    p->frameLayout->setSpacing(0);

    p->tab = new QTabBar(frame);
    p->tab->setShape(QTabBar::RoundedNorth);
    p->tab->setContextMenuPolicy(Qt::CustomContextMenu);
    p->tab->setUsesScrollButtons(true);
    p->tab->setElideMode(Qt::ElideRight);
    int t;
    t = p->tab->addTab(nullptr);
    p->tab->setTabData(t, Layer::All);
    t = p->tab->addTab(nullptr);
    p->tab->setTabData(t, Layer::Map);
    t = p->tab->addTab(nullptr);
    p->tab->setTabData(t, Layer::Draw);
    t = p->tab->addTab(nullptr);
    p->tab->setTabData(t, Layer::Tracks);
    retranslateTabBar();
    connect(p->tab, SIGNAL(currentChanged (int)), this, SLOT(tabChanged(int)));
    connect(p->tab, SIGNAL(customContextMenuRequested (const QPoint&)), this, SLOT(tabContextMenuRequested(const QPoint&)));

    QVBoxLayout* tabLayout = new QVBoxLayout();
    tabLayout->addWidget(p->tab);
//    QSpacerItem* tabSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
//    tabLayout->addItem(tabSpacer);

    p->frameLayout->addLayout(tabLayout);

    p->Scroller = new QScrollArea(frame);
    p->Scroller->setBackgroundRole(QPalette::Base);
    p->Scroller->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    p->Scroller->setWidgetResizable(true);

    QWidget* aWidget = new QWidget();
    QVBoxLayout* aLayout = new QVBoxLayout(aWidget);
    aLayout->setSpacing(0);
    aLayout->setMargin(0);

    p->Content = new QWidget();
    p->Layout = new QVBoxLayout(p->Content);
    p->Layout->setSpacing(0);
    p->Layout->setMargin(0);

    aLayout->addWidget(p->Content);
    aLayout->addStretch();

    p->Scroller->setWidget(aWidget);

    p->frameLayout->addWidget(p->Scroller);

    setWidget(frame);
    update();

    //Contextual Menu
    p->ctxMenu = new QMenu(this);

    QAction* actShowAll = new QAction(tr("Show All"), p->ctxMenu);
    actShowAll->setCheckable(false);
    p->ctxMenu->addAction(actShowAll);
    connect(actShowAll, SIGNAL(triggered(bool)), this, SLOT(showAllLayers(bool)));

    QAction* actHideAll = new QAction(tr("Hide All"), p->ctxMenu);
    actHideAll->setCheckable(false);
    p->ctxMenu->addAction(actHideAll);
    connect(actHideAll, SIGNAL(triggered(bool)), this, SLOT(hideAllLayers(bool)));

    p->ctxMenu->addSeparator();

    QAction* actReadonlyAll = new QAction(tr("Readonly All"), p->ctxMenu);
    actReadonlyAll->setCheckable(false);
    p->ctxMenu->addAction(actReadonlyAll);
    connect(actReadonlyAll, SIGNAL(triggered(bool)), this, SLOT(readonlyAllLayers(bool)));

    QAction* actReadonlyNone = new QAction(tr("Readonly None"), p->ctxMenu);
    actReadonlyNone->setCheckable(false);
    p->ctxMenu->addAction(actReadonlyNone);
    connect(actReadonlyNone, SIGNAL(triggered(bool)), this, SLOT(readonlyNoneLayers(bool)));

    p->ctxMenu->addSeparator();

    QAction* actClose = new QAction(tr("Close"), p->ctxMenu);
    actClose->setCheckable(false);
    p->ctxMenu->addAction(actClose);
    connect(actClose, SIGNAL(triggered(bool)), this, SLOT(closeLayers(bool)));
}

void LayerDock::resizeEvent(QResizeEvent* )
{
}

void LayerDock::layerChanged(LayerWidget* l, bool adjustViewport)
{
    l->getAssociatedMenu()->setTitle(l->getLayer()->name());
    emit(layersChanged(adjustViewport));
}

void LayerDock::layerClosed(Layer* l)
{
//	Main->document()->getUploadedLayer()->clear();
    //Main->document()->remove(l);
    //delete l;
    l->deleteAll();
    l->setEnabled(false);
    l->setVisible(false);
    l->getWidget()->setVisible(false);
    l->getWidget()->getAssociatedMenu()->setVisible(false);
#ifndef _MOBILE
    p->Main->on_editPropertiesAction_triggered();
#endif
    p->Main->document()->removeDownloadBox(l);
    if (p->Main->document()->getLastDownloadLayer() == l)
        p->Main->document()->setLastDownloadLayer(nullptr);

    emit layersClosed();
    update();
}

void LayerDock::layerCleared(Layer* l)
{
    l->clear();
#ifndef _MOBILE
    p->Main->on_editPropertiesAction_triggered();
#endif

    emit layersCleared();
}

void LayerDock::layerZoom(Layer * l)
{
    CoordBox bb = l->boundingBox();
    qDebug() << "layerZoom: " << bb.topLeft().y() << ";" << bb.topLeft().x() << ";" << bb.bottomRight().y() << ";" << bb.bottomRight().x();
    if (bb.isNull())
        return;

    CoordBox mini(bb.center()-COORD_ENLARGE, bb.center()+COORD_ENLARGE);
    bb.merge(mini);
//    bb = bb.zoomed(1.1);
    p->Main->view()->setViewport(bb, p->Main->view()->rect());
    emit(layersChanged(false));
}

void LayerDock::layerProjection(const QString &prj)
{
    emit layersProjection(prj);
}

void LayerDock::tabChanged(int idx)
{
    for (int i=CHILD_WIDGETS.size()-1; i >= 0; i--) {
        if (!CHILD_WIDGET(i))
            continue;
        if ((CHILD_LAYER(i)->isEnabled()) && (CHILD_LAYER(i)->classGroups() & (p->tab->tabData(idx).toUInt())))
            CHILD_WIDGET(i)->setVisible(true);
        else
            CHILD_WIDGET(i)->setVisible(false);
    }
}

void LayerDock::tabContextMenuRequested(const QPoint& pos)
{
    int idx = p->tab->tabAt(pos);
    p->tab->setCurrentIndex(idx);

    QMenu* ctxMenu = new QMenu(this);

    QAction* actTabShow = new QAction(tr("Show All"), ctxMenu);
    ctxMenu->addAction(actTabShow);
    connect(actTabShow, SIGNAL(triggered(bool)), this, SLOT(TabShowAll(bool)));

    QAction* actTabHide = new QAction(tr("Hide All"), ctxMenu);
    ctxMenu->addAction(actTabHide);
    connect(actTabHide, SIGNAL(triggered(bool)), this, SLOT(TabHideAll(bool)));

    ctxMenu->exec(mapToGlobal(pos));

}

void LayerDock::TabShowAll(bool)
{
    for (int i=CHILD_WIDGETS.size()-1; i >= 0; i--) {
        if (!CHILD_WIDGET(i))
            continue;
        if (CHILD_LAYER(i)->classGroups() & p->tab->tabData(p->tab->currentIndex()).toInt()) {
            CHILD_WIDGET(i)->setLayerVisible(true);
        }
    }
}

void LayerDock::TabHideAll(bool)
{
    for (int i=CHILD_WIDGETS.size()-1; i >= 0; i--) {
        if (!CHILD_WIDGET(i))
            continue;
        if (CHILD_LAYER(i)->classGroups() & p->tab->tabData(p->tab->currentIndex()).toInt()) {
            CHILD_WIDGET(i)->setLayerVisible(false);
        }
    }
}

void LayerDock::changeEvent(QEvent * event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    MDockAncestor::changeEvent(event);
}

void LayerDock::retranslateUi()
{
    setWindowTitle(tr("Layers"));
    retranslateTabBar();
}

void LayerDock::retranslateTabBar()
{
    p->tab->setTabText(0, tr("All"));
    p->tab->setTabText(1, tr("Map"));
    p->tab->setTabText(2, tr("Draw"));
    p->tab->setTabText(3, tr("Tracks"));
}

void LayerDock::showAllLayers(bool)
{
    for (int i=0; i<p->selWidgets.size(); ++i) {
        p->selWidgets[i]->setLayerVisible(true);
    }
}

void LayerDock::hideAllLayers(bool)
{
    for (int i=0; i<p->selWidgets.size(); ++i) {
        p->selWidgets[i]->setLayerVisible(false);
    }
}

void LayerDock::readonlyAllLayers(bool)
{
    for (int i=0; i<p->selWidgets.size(); ++i) {
        p->selWidgets[i]->setLayerReadonly(true);
    }
}

void LayerDock::readonlyNoneLayers(bool)
{
    for (int i=0; i<p->selWidgets.size(); ++i) {
        p->selWidgets[i]->setLayerReadonly(false);
    }
}

void LayerDock::closeLayers(bool)
{
    for (int i=0; i<p->selWidgets.size(); ++i) {
        if (p->selWidgets[i]->getLayer()->canDelete())
            layerClosed(p->selWidgets[i]->getLayer());
    }
}

void LayerDock::resetLayers()
{
    QList<Layer*> toDelete;
    for (int i=0; i < CHILD_WIDGETS.size(); ++i) {
        if (CHILD_WIDGET(i)) {
            if (CHILD_LAYER(i)->classType() == Layer::FilterLayerType)
                toDelete << CHILD_LAYER(i);
            else if ((CHILD_LAYER(i)->classType() == Layer::DirtyLayerType || CHILD_LAYER(i)->classType() == Layer::UploadedLayerType) && CHILD_LAYER(i)->size() == 0)
                toDelete << CHILD_LAYER(i);
            else {
                CHILD_WIDGET(i)->setLayerVisible(true);
                CHILD_LAYER(i)->setReadonly(false);
                CHILD_LAYER(i)->setAlpha(1.0);
            }
        }
    }
    foreach (Layer* f, toDelete)
        layerClosed(f);
    p->Main->document()->addFilterLayers();
}

void LayerDock::contextMenuEvent(QContextMenuEvent* anEvent)
{
    LayerWidget* aWidget = dynamic_cast<LayerWidget*>(childAt(anEvent->pos()));

    if (aWidget) {
        p->selWidgets.clear();
        for (int i=0; i < CHILD_WIDGETS.size(); ++i) {
            if (CHILD_WIDGET(i) && CHILD_WIDGET(i)->isChecked())
                p->selWidgets.push_back(CHILD_WIDGET(i));
        }

        if (p->selWidgets.size() == 0 || p->selWidgets.size() == 1) {
            for (int i=0; i < CHILD_WIDGETS.size(); ++i) {
                if (CHILD_WIDGET(i))
                    CHILD_WIDGET(i)->setChecked(false);
            }
            aWidget->setChecked(true);
            p->lastSelWidget = aWidget;

            aWidget->showContextMenu(anEvent);
        } else if (p->selWidgets.size()) {
            p->ctxMenu->exec(anEvent->globalPos());
        }
    } else {
        //Contextual Menu
        QMenu* ctxMenu = new QMenu(this);

        QAction* actResetLayers = new QAction(tr("Reset Layers to default"), ctxMenu);
        ctxMenu->addAction(actResetLayers);
        connect(actResetLayers, SIGNAL(triggered(bool)), this, SLOT(resetLayers()));

        ctxMenu->exec(anEvent->globalPos());
    }
}

#if QT_VERSION < 0x040500
bool LayerDock::event (QEvent* ev)
{
    switch (ev->type()) {
    case QEvent::MouseButtonPress:
        mousePressEvent(static_cast<QMouseEvent *>(ev));
        break;
    default:
        break;
    }
    return MDockAncestor::event(ev);
}
#endif

void LayerDock::mousePressEvent ( QMouseEvent * ev )
{
    if (ev->button() != Qt::LeftButton) {
        ev->ignore();
        return;
    }

    LayerWidget* aWidget = dynamic_cast<LayerWidget*>(childAt(ev->pos()));

    if (!aWidget) {
        for (int i=0; i < CHILD_WIDGETS.size(); ++i) {
            if (CHILD_WIDGET(i))
                CHILD_WIDGET(i)->setChecked(false);
        }
        p->lastSelWidget = nullptr;
        ev->ignore();
        return;
    }

    if (ev->modifiers() & Qt::ControlModifier) {
        bool toSelect = !aWidget->isChecked();
        aWidget->setChecked(toSelect);
        if (toSelect)
            p->lastSelWidget = aWidget;
        else
            p->lastSelWidget = nullptr;
    } else
    if (ev->modifiers() & Qt::ShiftModifier) {
        bool toSelect = false;
        for (int i=0; i < CHILD_WIDGETS.size(); ++i) {
            if (CHILD_WIDGET(i)) {
                if (CHILD_WIDGET(i) == aWidget || CHILD_WIDGET(i) == p->lastSelWidget)
                    toSelect = !toSelect;

                if (toSelect)
                    CHILD_WIDGET(i)->setChecked(true);
            }
        }
        aWidget->setChecked(true);
    } else {
        for (int i=0; i < CHILD_WIDGETS.size(); ++i) {
            if (CHILD_WIDGET(i))
                CHILD_WIDGET(i)->setChecked(false);
        }
        aWidget->setChecked(true);
        p->lastSelWidget = aWidget;

        if (p->Main->info())
            p->Main->info()->setHtml(aWidget->getLayer()->toHtml());
    }
    ev->accept();
}
