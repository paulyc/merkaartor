#include "Global.h"
#include "MoveNodeInteraction.h"

#include "MainWindow.h"
#include "MapView.h"
#include "DocumentCommands.h"
#include "WayCommands.h"
#include "NodeCommands.h"
#include "Coord.h"
#include "Document.h"
#include "Projection.h"
#include "Node.h"
#include "LineF.h"
#include "MDiscardableDialog.h"
#include "PropertiesDock.h"

#include <QtGui/QCursor>
#include <QtGui/QMouseEvent>
#include <QtGui/QPixmap>
#include <QMessageBox>

#include <QList>

MoveNodeInteraction::MoveNodeInteraction(MainWindow* aMain)
    : FeatureSnapInteraction(aMain)
    , StartDragPosition(0,0)
    , theList(0)
{
    setDontSelectVirtual(true);
    if (M_PREFS->getSeparateMoveMode()) {
        setDontSelectVirtual(false);
    }
}

MoveNodeInteraction::~MoveNodeInteraction(void)
{
}

QString MoveNodeInteraction::toHtml()
{
    QString help;
    help = (MainWindow::tr("LEFT-CLICK to select;LEFT-DRAG to move"));

    QStringList helpList = help.split(";");

    QString desc;
    desc = QString("<big><b>%1</b></big>").arg(MainWindow::tr("Move Node interaction"));

    QString S =
    "<html><head/><body>"
    "<small><i>" + QString(metaObject()->className()) + "</i></small><br/>"
    + desc;
    S += "<hr/>";
    S += "<ul style=\"margin-left: 0px; padding-left: 0px;\">";
    for (int i=0; i<helpList.size(); ++i) {
        S+= "<li>" + helpList[i] + "</li>";
    }
    S += "</ul>";
    S += "</body></html>";

    return S;
}

#ifndef _MOBILE
QCursor MoveNodeInteraction::cursor() const
{
#ifdef Q_OS_MAC
    QPixmap pm(":/Icons/move.xpm");
    return QCursor(pm);
#else
    return QCursor(Qt::SizeAllCursor);
#endif
}
#endif

void MoveNodeInteraction::recurseAddNodes(Feature* F)
{
    if (Node* Pt = CAST_NODE(F))
    {
        Moving.push_back(Pt);
    }
    else if (Way* R = CAST_WAY(F)) {
        if (!g_Merk_Segment_Mode) {
            for (int j=0; j<R->size(); ++j)
                if (std::find(Moving.begin(),Moving.end(),R->get(j)) == Moving.end()) {
                    Moving.push_back(R->getNode(j));
                }
        } else {
            for (int j=R->bestSegment(); j<=R->bestSegment()+1; ++j)
                if (std::find(Moving.begin(),Moving.end(),R->get(j)) == Moving.end()) {
                    Moving.push_back(R->getNode(j));
                }
        }
        addToNoSnap(R);
    }
    else if (Relation* L = CAST_RELATION(F)) {
        for (int j=0; j<L->size(); ++j)
            recurseAddNodes(L->get(j));
        addToNoSnap(L);
    }
}

void MoveNodeInteraction::snapMousePressEvent(QMouseEvent * event, Feature* aLast)
{
    QList<Feature*> sel;
    if (view()->isSelectionLocked()) {
        if (theMain->properties()->selection(0))
            sel.append(theMain->properties()->selection(0));
        else
            sel.append(aLast);
    } else {
        if (aLast) {
            if (theMain->properties()->selection().size() && !M_PREFS->getSeparateMoveMode())
                sel = theMain->properties()->selection();
            else
                sel.append(aLast);
        }
    }
    HasMoved = false;
    clearNoSnap();
    Moving.clear();
    OriginalPosition.clear();
    StartDragPosition = XY_TO_COORD(event->pos());
    if (sel.size() == 1) {
        if (Node* Pt = CAST_NODE(sel[0])) {
            StartDragPosition = Pt->position();
            Moving.push_back(Pt);
        } else
            recurseAddNodes(sel[0]);
    } else {
        for (int i=0; i<sel.size(); i++) {
            recurseAddNodes(sel[i]);
        }
    }

    for (int i=0; i<Moving.size(); ++i)
    {
        OriginalPosition.push_back(Moving[i]->position());
        addToNoSnap(Moving[i]);
    }
    Virtual = false;
    theList = new CommandList;
}

void MoveNodeInteraction::snapMouseReleaseEvent(QMouseEvent * event, Feature* Closer)
{
    if (event->button() != Qt::LeftButton)
        return;

    if (Moving.size() && !panning() && HasMoved)
    {
        Coord Diff(calculateNewPosition(event,Closer, theList)-StartDragPosition);
        if (Moving.size() > 1) {
            theList->setDescription(MainWindow::tr("Move nodes"));
            theList->setFeature(Moving[0]);
        } else {
            if (!Virtual) {
                theList->setDescription(MainWindow::tr("Move node %1").arg(Moving[0]->id().numId));
                theList->setFeature(Moving[0]);
            }
        }
        QSet<Way*> WaysToUpdate;
        for (int i=0; i<Moving.size(); ++i)
        {
            Moving[i]->setPosition(OriginalPosition[i]);
            if (Moving[i]->layer()->isTrack())
                theList->add(new MoveNodeCommand(Moving[i],OriginalPosition[i]+Diff, Moving[i]->layer()));
            else
                theList->add(new MoveNodeCommand(Moving[i],OriginalPosition[i]+Diff, document()->getDirtyOrOriginLayer(Moving[i]->layer())));
            for (int j=0; j<Moving[i]->sizeParents(); ++j) {
                if (Way* aRoad = CAST_WAY(Moving[i]->getParent(j)))
                    WaysToUpdate << aRoad;
                else {
                    Feature* f = CAST_FEATURE(Moving[i]->getParent(j));
                    if (f)
                        g_backend.sync(f);
                }
            }
        }
        foreach (Way* w, WaysToUpdate) {
            g_backend.sync(w);
        }

        // If moving a single node (not a track node), see if it got dropped onto another node
        if (Moving.size() == 1 && !Moving[0]->layer()->isTrack())
        {
            Coord newPos = OriginalPosition[0] + Diff;
            QList<Node*> samePosPts;
            for (VisibleFeatureIterator it(document()); !it.isEnd(); ++it)
            {
                Node* visPt = CAST_NODE(it.get());
                if (visPt && visPt->layer()->classType() != Layer::TrackLayerType)
                {
                    if (visPt == Moving[0])
                        continue;

                    if (visPt->position() == newPos)
                    {
                        samePosPts.push_back(visPt);
                    }
                }
            }
            // Ensure the node being moved is at the end of the list.
            // (This is not the node that all nodes will be merged into,
            // they are always merged into a node that already was at that position.)
            samePosPts.push_back(Moving[0]);

            if (samePosPts.size() > 1)   // Ignore the node we're moving, see if there are more
            {
                MDiscardableMessage dlg(view(),
                    MainWindow::tr("Nodes at the same position found."),
                    MainWindow::tr("Do you want to merge all nodes at the drop position?"));
                if (dlg.check() == QDialog::Accepted)
                {
                    // Merge all nodes from the same position

                    // from MainWindow::on_nodeMergeAction_triggered()
                    // Merge all nodes into the first node that has been found (not the node being moved)
                    Node* merged = samePosPts[0];
                    // Change the command description to reflect the merge
                    theList->setDescription(MainWindow::tr("Merge nodes into %1").arg(merged->id().numId));
                    theList->setFeature(merged);

                    // from mergeNodes(theDocument, theList, theProperties);
                    QList<Feature*> alt;
                    alt.push_back(merged);
                    for (int i = 1; i < samePosPts.size(); ++i) {
                        Feature::mergeTags(document(), theList, merged, samePosPts[i]);
                        theList->add(new RemoveFeatureCommand(document(), samePosPts[i], alt));
                    }

                    theMain->properties()->setSelection(merged);
                }
            }
        }

        if (theList)
            document()->addHistory(theList);
        theList = nullptr;
        view()->setInteracting(false);
        view()->invalidate(true, true, false);
    } else
        SAFE_DELETE(theList);
    Moving.clear();
    OriginalPosition.clear();
    clearNoSnap();
}

void MoveNodeInteraction::snapMouseMoveEvent(QMouseEvent* event, Feature* Closer)
{
    if (Moving.size() && !panning())
    {
        HasMoved = true;
        view()->setInteracting(true);
        Coord Diff = calculateNewPosition(event,Closer,nullptr)-StartDragPosition;
        for (int i=0; i<Moving.size(); ++i) {
            if (Moving[i]->isVirtual()) {
                Virtual = true;
                Node* v = Moving[i];
                Way* aRoad = CAST_WAY(v->getParent(0));
                theList->setDescription(MainWindow::tr("Create node in way %1").arg(aRoad->id().numId));
                theList->setFeature(aRoad);
                int SnapIdx = aRoad->findVirtual(v)+1;
                Node* N = g_backend.allocNode(main()->document()->getDirtyOrOriginLayer(aRoad->layer()), *v);
                N->setVirtual(false);
                N->setPosition(OriginalPosition[i]+Diff);

                if (theMain->properties()->isSelected(v)) {
                    theMain->properties()->toggleSelection(v);
                    theMain->properties()->toggleSelection(N);
                }
                theList->add(new AddFeatureCommand(main()->document()->getDirtyOrOriginLayer(aRoad->layer()),N,true));
                theList->add(new WayAddNodeCommand(aRoad,N,SnapIdx,main()->document()->getDirtyOrOriginLayer(aRoad->layer())));

                Moving[i] = N;
                LastSnap = N;
            } else {
                Moving[i]->setPosition(OriginalPosition[i]+Diff);
            }
        }
        view()->invalidate(true, true, false);
    }
}

Coord MoveNodeInteraction::calculateNewPosition(QMouseEvent *event, Feature *aLast, CommandList* theList)
{
    if (aLast && CHECK_NODE(aLast))
        return STATIC_CAST_NODE(aLast)->position();

    Coord TargetC = XY_TO_COORD(event->pos());
    if (aLast && CHECK_WAY(aLast))
    {
        Way* R = STATIC_CAST_WAY(aLast);
        QPointF Target = TargetC;
        LineF L1(R->getNode(0)->position(),R->getNode(1)->position());
        qreal Dist = L1.capDistance(TargetC);
        QPointF BestTarget = L1.project(Target);
        int BestIdx = 1;
        for (int i=2; i<R->size(); ++i)
        {
            LineF L2(R->getNode(i-1)->position(),R->getNode(i)->position());
            qreal Dist2 = L2.capDistance(TargetC);
            if (Dist2 < Dist)
            {
                Dist = Dist2;
                BestTarget = L2.project(Target);
                BestIdx = i;
            }
        }
        if (theList && (Moving.size() == 1))
            theList->add(new
                WayAddNodeCommand(R,Moving[0],BestIdx,document()->getDirtyOrOriginLayer(R->layer())));
        return Coord(BestTarget);
    }
    return TargetC;
}

bool MoveNodeInteraction::isIdle()
{
    if (Moving.size() && !panning())
        return false;

    return true;
}
