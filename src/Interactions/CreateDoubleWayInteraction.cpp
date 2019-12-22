#include "CreateDoubleWayInteraction.h"
#include "DocumentCommands.h"
#include "WayCommands.h"
#include "NodeCommands.h"
#include "Painting.h"
#include "Way.h"
#include "Node.h"
#include "LineF.h"
#include "MainWindow.h"
#include "MerkaartorPreferences.h"
#include "Global.h"

#include <QDockWidget>
#include <QPainter>

CreateDoubleWayInteraction::CreateDoubleWayInteraction(MainWindow* aMain)
    : Interaction(aMain), R1(0), R2(0), FirstPoint(0,0), HaveFirst(false), theDock(0)
{
#ifndef _MOBILE
    theDock = new QDockWidget(theMain);
    QWidget* DockContent = new QWidget(theDock);
    DockData.setupUi(DockContent);
    theDock->setWidget(DockContent);
    theDock->setAllowedAreas(Qt::LeftDockWidgetArea);
    theMain->addDockWidget(Qt::LeftDockWidgetArea, theDock);
    theDock->show();
    DockData.DriveRight->setChecked(M_PREFS->getrightsidedriving());
    DockData.RoadDistance->setText(QString().setNum(M_PREFS->getdoubleroaddistance()));

    theMain->view()->setCursor(cursor());
#endif
}

CreateDoubleWayInteraction::~CreateDoubleWayInteraction()
{
    M_PREFS->setrightsidedriving(DockData.DriveRight->isChecked());
    M_PREFS->setdoubleroaddistance(DockData.RoadDistance->text().toDouble());

    delete theDock;
    view()->update();
}

QString CreateDoubleWayInteraction::toHtml()
{
    QString help;
    //help = (MainWindow::tr("LEFT-CLICK to select; LEFT-DRAG to move"));

    QString desc;
    desc = QString("<big><b>%1</b></big><br/>").arg(MainWindow::tr("Create Parallel Way interaction"));
    desc += QString("<b>%1</b><br/>").arg(help);

    QString S =
    "<html><head/><body>"
    "<small><i>" + QString(metaObject()->className()) + "</i></small><br/>"
    + desc;
    S += "</body></html>";

    return S;
}

void CreateDoubleWayInteraction::paintEvent(QPaintEvent* /* anEvent */, QPainter& thePainter)
{
    if (R1 && (!R1->layer() || R1->isDeleted())) { // The roads were begon and then undoed. Restarting....
        HaveFirst = false;
        view()->setInteracting(false);
        R1 = R2 = nullptr;
    }

    qreal rB = view()->pixelPerM()*DockData.RoadDistance->text().toDouble()/2;
    if (!HaveFirst)
    {
        thePainter.setPen(QColor(0,0,0));
        thePainter.drawEllipse(int(LastCursor.x()-rB),int(LastCursor.y()-rB),int(rB*2),int(rB*2));
    }
    else
    {
        Coord PreviousPoint;
        if (R1 && R1->size())
            PreviousPoint = PreviousPoints[R1->size()-1];
        else
            PreviousPoint = FirstPoint;

        if (distance(COORD_TO_XY(PreviousPoint), LastCursor) > 1)
        {
            qreal rA = FirstDistance * view()->pixelPerM()/2;
            LineF FA1(COORD_TO_XY(PreviousPoint),LastCursor);
            LineF FA2(FA1);
            LineF FB1(FA1);
            LineF FB2(FA1);
            FA1.slide(-rA);
            FA2.slide(rA);
            FB1.slide(-rB);
            FB2.slide(rB);
            QPointF A1(FA1.project(COORD_TO_XY(PreviousPoint)));
            QPointF A2(FA2.project(COORD_TO_XY(PreviousPoint)));
            QPointF B1(FB1.project(LastCursor));
            QPointF B2(FB2.project(LastCursor));

            QBrush SomeBrush(QColor(0xff,0x77,0x11,128));
            QPen TP(SomeBrush,view()->pixelPerM()*4);
            if (DockData.DriveRight->isChecked())
            {
                ::draw(thePainter,TP,Feature::OneWay, B1,A1,rB/4,view()->projection());
                ::draw(thePainter,TP,Feature::OneWay, A2,B2,rB/4,view()->projection());
            }
            else
            {
                ::draw(thePainter,TP,Feature::OneWay, A1,B1,rB/4,view()->projection());
                ::draw(thePainter,TP,Feature::OneWay, B2,A2,rB/4,view()->projection());
            }
        }
    }
}

void CreateDoubleWayInteraction::mouseMoveEvent(QMouseEvent* event)
{
    LastCursor = event->pos();
    Interaction::mouseMoveEvent(event);
    view()->update();
    Interaction::mouseMoveEvent(event);
}

void CreateDoubleWayInteraction::mouseReleaseEvent(QMouseEvent* anEvent)
{
    if (M_PREFS->getMouseSingleButton() && anEvent->button() == Qt::RightButton) {
        HaveFirst = false;
        view()->update();
    }
    Interaction::mouseReleaseEvent(anEvent);
}

void CreateDoubleWayInteraction::mousePressEvent(QMouseEvent* anEvent)
{
    if (anEvent->buttons() & Qt::LeftButton)
    {
        if (!HaveFirst)
        {
            HaveFirst = true;
            view()->setInteracting(true);
            FirstPoint = XY_TO_COORD(anEvent->pos());
            FirstDistance = DockData.RoadDistance->text().toDouble();
        }
        else if (R1)
        {
            int i1 = R1->size()-1;
            int i2 = 1;
            LineF P1(
                COORD_TO_XY(R1->getNode(i1-1)),
                COORD_TO_XY(R1->getNode(i1)));
            LineF P2(
                COORD_TO_XY(R2->getNode(i2-1)),
                COORD_TO_XY(R2->getNode(i2)));

            Coord PreviousPoint = PreviousPoints[R1->size()-1];
            if (distance(COORD_TO_XY(PreviousPoint), LastCursor) > 1)
            {
                qreal rB = view()->pixelPerM()*DockData.RoadDistance->text().toDouble()/2;
                qreal rA = FirstDistance * view()->pixelPerM()/2;
                LineF FA1(COORD_TO_XY(PreviousPoint),LastCursor);
                LineF FA2(FA1);
                LineF FB1(FA1);
                LineF FB2(FA1);
                qreal Modifier = DockData.DriveRight->isChecked()?1:-1;
                FA1.slide(rA*Modifier);
                FA2.slide(-rA*Modifier);
                FB1.slide(rB*Modifier);
                FB2.slide(-rB*Modifier);
                LineF N1(FA1.project(COORD_TO_XY(PreviousPoint)), FB1.project(LastCursor));
                LineF N2(FA2.project(COORD_TO_XY(PreviousPoint)), FB2.project(LastCursor));

                Node* A1;
                Node* A2;
                CommandList* L  = new CommandList(MainWindow::tr("Add nodes to parallel way %1").arg(R1->id().numId), R1);
                A1 = R1->getNode(i1);
                A2 = R2->getNode(i2-1);
                L->add(new MoveNodeCommand(A1,XY_TO_COORD(
                    P1.intersectionWith(N1).toPoint())));
                L->add(new MoveNodeCommand(A2,XY_TO_COORD(
                    P2.intersectionWith(N2).toPoint())));
                Node* B1 = g_backend.allocNode(theMain->document()->getDirtyOrOriginLayer(), XY_TO_COORD(
                    FB1.project(LastCursor).toPoint()));
                Node* B2 = g_backend.allocNode(theMain->document()->getDirtyOrOriginLayer(), XY_TO_COORD(
                    FB2.project(LastCursor).toPoint()));

                L->add(new AddFeatureCommand(theMain->document()->getDirtyOrOriginLayer(),B1,true));
                L->add(new AddFeatureCommand(theMain->document()->getDirtyOrOriginLayer(),B2,true));
                L->add(new WayAddNodeCommand(R1,B1));
                L->add(new WayAddNodeCommand(R2,B2,(int)0));
                document()->addHistory(L);
                view()->invalidate(true, true, false);
                //FirstPoint = view()->projection().inverse(anEvent->pos());
                PreviousPoints[R1->size()-1] = XY_TO_COORD(anEvent->pos());
                FirstDistance = DockData.RoadDistance->text().toDouble();
            }
        }
        else
        {
            Coord PreviousPoint = FirstPoint;
            if (distance(COORD_TO_XY(PreviousPoint), LastCursor) > 1)
            {
                qreal rB = view()->pixelPerM()*DockData.RoadDistance->text().toDouble()/2;
                qreal rA = FirstDistance * view()->pixelPerM()/2;
                LineF FA1(COORD_TO_XY(PreviousPoint),LastCursor);
                LineF FA2(FA1);
                LineF FB1(FA1);
                LineF FB2(FA1);
                qreal Modifier = DockData.DriveRight->isChecked()?1:-1;
                FA1.slide(rA*Modifier);
                FA2.slide(-rA*Modifier);
                FB1.slide(rB*Modifier);
                FB2.slide(-rB*Modifier);

                Node* A1 = g_backend.allocNode(theMain->document()->getDirtyOrOriginLayer(), XY_TO_COORD(
                    FA1.project(COORD_TO_XY(PreviousPoint)).toPoint()));
                Node* A2 = g_backend.allocNode(theMain->document()->getDirtyOrOriginLayer(), XY_TO_COORD(
                    FA2.project(COORD_TO_XY(PreviousPoint)).toPoint()));
                Node* B1 = g_backend.allocNode(theMain->document()->getDirtyOrOriginLayer(), XY_TO_COORD(
                    FB1.project(LastCursor).toPoint()));
                Node* B2 = g_backend.allocNode(theMain->document()->getDirtyOrOriginLayer(), XY_TO_COORD(
                    FB2.project(LastCursor).toPoint()));
                R1 = g_backend.allocWay(theMain->document()->getDirtyOrOriginLayer());
                R2 = g_backend.allocWay(theMain->document()->getDirtyOrOriginLayer());

                CommandList* L  = new CommandList(MainWindow::tr("Create parallel way %1").arg(R1->id().numId), R1);
                L->add(new AddFeatureCommand(theMain->document()->getDirtyOrOriginLayer(),A1,true));
                L->add(new AddFeatureCommand(theMain->document()->getDirtyOrOriginLayer(),A2,true));
                L->add(new AddFeatureCommand(theMain->document()->getDirtyOrOriginLayer(),B1,true));
                L->add(new AddFeatureCommand(theMain->document()->getDirtyOrOriginLayer(),B2,true));

                L->add(new AddFeatureCommand(theMain->document()->getDirtyOrOriginLayer(),R1,true));
                L->add(new AddFeatureCommand(theMain->document()->getDirtyOrOriginLayer(),R2,true));
                if (M_PREFS->getAutoSourceTag()) {
                    QStringList sl = theMain->document()->getCurrentSourceTags();
                    if (sl.size()) {
                        R1->setTag("source", sl.join(";"));
                        R2->setTag("source", sl.join(";"));
                    }
                }
                R1->setTag("oneway","yes");
                R2->setTag("oneway","yes");
                L->add(new WayAddNodeCommand(R1,A1));
                L->add(new WayAddNodeCommand(R1,B1));
                L->add(new WayAddNodeCommand(R2,B2));
                L->add(new WayAddNodeCommand(R2,A2));
                document()->addHistory(L);
                view()->invalidate(true, true, false);
                //FirstPoint = view()->projection().inverse(anEvent->pos());
                PreviousPoints[R1->size()-1] = XY_TO_COORD(anEvent->pos());
                FirstDistance = DockData.RoadDistance->text().toDouble();
            }
        }
    }
    else
        Interaction::mousePressEvent(anEvent);
}

#ifndef _MOBILE
QCursor CreateDoubleWayInteraction::cursor() const
{
    return QCursor(Qt::CrossCursor);
}

#endif
