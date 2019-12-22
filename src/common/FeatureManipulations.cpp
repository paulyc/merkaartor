#include "FeatureManipulations.h"

#include "Global.h"
#include "Coord.h"
#include "DocumentCommands.h"
#include "FeatureCommands.h"
#include "WayCommands.h"
#include "RelationCommands.h"
#include "NodeCommands.h"
#include "Document.h"
#include "Features.h"
#include "PropertiesDock.h"
#include "Projection.h"
#include "ImportExportOSC.h"

#include "Utils.h"
#include "LineF.h"

#include <QtCore/QString>
#include <QMessageBox>

#include <algorithm>

static void mergeNodes(Document* theDocument, CommandList* theList, Node *node1, Node *node2);

static bool isNear(Node* a, Node* b)
{
    // For now, only if exactly same position
    // Future: use distance threshold?
    return a->position() == b->position();
}

bool canJoin(Way* R1, Way* R2)
{
    if ( (R1->size() == 0) || (R2->size() == 0) )
        return true;
    Node* Start1 = R1->getNode(0);
    Node* End1 = R1->getNode(R1->size()-1);
    Node* Start2 = R2->getNode(0);
    Node* End2 = R2->getNode(R2->size()-1);
    return (Start1 == Start2) ||
        (Start1 == End2) ||
        (End1 == Start2) ||
        (End1 == End2) ||
        isNear(Start1, Start2) ||
        isNear(Start1, End2) ||
        isNear(End1, Start2) ||
        isNear(End1, End2);
}

bool canBreak(Way* R1, Way* R2)
{
    if ( (R1->size() == 0) || (R2->size() == 0) )
        return false;
    for (int i=0; i<R1->size(); i++)
        for (int j=0; j<R2->size(); j++)
            if (R1->get(i) == R2->get(j))
                return true;
    return false;
}

bool canJoinRoads(PropertiesDock* theDock)
{
    QHash<Coord,int> ends;
    for (int i=0; i<theDock->selectionSize(); ++i)
        if (Way* R = CAST_WAY(theDock->selection(i))) {
            if (R->isClosed()) continue;
            if (!R->size()) continue;
            Coord start = R->getNode(0)->position();
            Coord end = R->getNodes().back()->position();
            ++ends[start];
            ++ends[end];
        }

    return ends.values().contains(2);
}

bool canBreakRoads(PropertiesDock* theDock)
{
    QList<Way*> Input;
    for (int i=0; i<theDock->selectionSize(); ++i)
        if (Way* R = CAST_WAY(theDock->selection(i)))
            Input.push_back(R);
    for (int i=0; i<Input.size(); ++i)
        for (int j=i+1; j<Input.size(); ++j)
            if (canBreak(Input[i],Input[j]))
                return true;
    return false;
}

bool canDetachNodes(PropertiesDock* theDock)
{
    QList<Way*> Roads, Result;
    QList<Node*> Points;
    for (int i=0; i<theDock->selectionSize(); ++i)
        if (Way* R = CAST_WAY(theDock->selection(i)))
            Roads.push_back(R);
        else if (Node* Pt = CAST_NODE(theDock->selection(i)))
            Points.push_back(Pt);

    if (Roads.size() > 1 && Points.size() > 1)
        return false;

    if (Roads.size() == 0 && Points.size()) {
        for (int i=0; i<Points.size(); ++i) {
            Way * R = Way::GetSingleParentRoad(Points[i]);
            if (R)
                return true;
        }
        return false;
    }

    if (Roads.size() && Points.size() == 1)
        return true;

    if (Roads.size() == 1 && Points.size())
        return true;

    return false;
}

void reversePoints(Document* theDocument, CommandList* theList, Way* R)
{
    Layer *layer=theDocument->getDirtyOrOriginLayer(R->layer());
    for (int i=R->size()-2; i>=0; --i)
    {
        Node* Pt = R->getNode(i);
        theList->add(new WayRemoveNodeCommand(R,i,layer));
        theList->add(new WayAddNodeCommand(R,Pt,layer));
    }
}


static qreal distanceFrom(QLineF ab, const Coord& c)
{
    // distance in metres from c to line segment ab
    qreal ab_len2 = ab.dx() * ab.dx() + ab.dy() * ab.dy();
    QPointF dp;
    if (ab_len2) {
        QLineF ac(ab.p1(), c);
        qreal u = (ac.dx() * ab.dx() + ac.dy() * ab.dy()) / ab_len2;
        if (u < 0.0) u = 0.0;
        else if (u > 1.0) u = 1.0;
        dp = ab.pointAt(u);
    } else {
        dp = ab.p1();
    }
    Coord dc(dp);
    return dc.distanceFrom(c);
}

void simplifyWay(Document *doc, Layer *layer, CommandList *theList, Way *w, int start, int end, qreal threshold)
{
    // Douglas-Peucker reduction of uninteresting points in way, subject to maximum error in metres

    // TODO Performance: Use path-hull algorithm described at http://www.cs.ubc.ca/cgi-bin/tr/1992/TR-92-07.ps
    if (end - start <= 1)
        // no removable nodes
        return;

    QLineF segment(w->getNode(start)->position(), w->getNode(end)->position());
    qreal maxdist = -1;
    int maxpos = 0;
    for (int i = start+1;  i < end;  i++) {
        qreal d = distanceFrom(segment, w->getNode(i)->position());
        if (d > maxdist) {
            maxdist = d;
            maxpos = i;
        }
    }
    // maxdist is in kilometres
    if (maxpos && maxdist * 1000 > threshold) {
        simplifyWay(doc, layer, theList, w, maxpos, end, threshold);
        simplifyWay(doc, layer, theList, w, start, maxpos, threshold);
    } else {
        for (int i = end - 1;  i > start;  i--) {
            Feature *n = w->get(i);
            if (!doc->isDownloadedSafe(n->boundingBox()) && n->hasOSMId())
                continue;
            theList->add(new WayRemoveNodeCommand(w, i, layer));
            theList->add(new RemoveFeatureCommand(doc, n));
        }
    }
}
QSet<QString> uninterestingKeys;
bool isNodeInteresting(Node *n)
{
    for (int i = 0; i < n->tagSize();  i++)
        if (!uninterestingKeys.contains(n->tagKey(i)))
            return true;
    return false;
}

void simplifyRoads(Document* theDocument, CommandList* theList, PropertiesDock* theDock, qreal threshold)
{
    if (uninterestingKeys.isEmpty())
        uninterestingKeys << "source";

    for (int i = 0;  i < theDock->selectionSize();  ++i)
        if (Way* w = CAST_WAY(theDock->selection(i))) {
            Layer *layer = theDocument->getDirtyOrOriginLayer(w->layer());
            int end = w->size() - 1;
            for (int i = end;  i >= 0;  i--) {
                Node *n = w->getNode(i);
                if (i == 0 || n->sizeParents() > 1 || isNodeInteresting(n)) {
                    simplifyWay(theDocument, layer, theList, w, i, end, threshold);
                    end = i;
                }
            }
        }
}

static void appendPoints(Document* theDocument, CommandList* L, Way* Dest, Way* Src, bool prepend, bool reverse)
{
    Layer *layer = theDocument->getDirtyOrOriginLayer(Dest->layer());
    int srclen = Src->size();
    int destpos = prepend ? 0 : Dest->size();
    for (int i=1; i<srclen; ++i) {
        int j = (reverse ? srclen-i : i) - (prepend != reverse ? 1 : 0);
        Node* Pt = Src->getNode(j);
        L->add(new AddFeatureCommand(layer, Pt, false));
        L->add(new WayAddNodeCommand(Dest, Pt, destpos++, layer));
    }
}

static Way* join(Document* theDocument, CommandList* L, Way* R1, Way* R2)
{
    QList<Feature*> Alternatives;
    if (R1->size() == 0)
    {
        Feature::mergeTags(theDocument,L,R2,R1);
        L->add(new RemoveFeatureCommand(theDocument,R1,Alternatives));
        return R2;
    }
    if (R2->size() == 0)
    {
        Feature::mergeTags(theDocument,L,R1,R2);
        L->add(new RemoveFeatureCommand(theDocument,R2,Alternatives));
        return R1;
    }
    Node* Start1 = R1->getNode(0);
    Node* End1 = R1->getNode(R1->size()-1);
    Node* Start2 = R2->getNode(0);
    Node* End2 = R2->getNode(R2->size()-1);

    bool prepend = false;       // set true if R2 meets beginning of R1
    bool reverse = false;       // set true if R2 is opposite direction to R1

    if (End1 == Start2) {
        // nothing to do, but skip the other tests
    } else if (End1 == End2) {
        reverse = true;
    } else if (Start1 == Start2) {
        prepend = true;
        reverse = true;
    } else if (Start1 == End2) {
        prepend = true;
    } else if (isNear(End1, Start2)) {
        mergeNodes(theDocument, L, End1, Start2);
    } else if (isNear(End1, End2)) {
        mergeNodes(theDocument, L, End1, End2);
        reverse = true;
    } else if (isNear(Start1, Start2)) {
        mergeNodes(theDocument, L, Start1, Start2);
        prepend = true;
        reverse = true;
    } else if (isNear(Start1, End2)) {
        mergeNodes(theDocument, L, Start1, End2);
        prepend = true;
    }

    appendPoints(theDocument, L, R1, R2, prepend, reverse);
    Feature::mergeTags(theDocument,L,R1,R2);
    L->add(new RemoveFeatureCommand(theDocument,R2,Alternatives));

    // Auto-merge nearly-closed ways
    Node *StartResult = R1->getNode(0);
    Node *EndResult = R1->getNode(R1->size()-1);
    if (StartResult != EndResult && isNear(StartResult, EndResult))
        mergeNodes(theDocument, L, StartResult, EndResult);
    return R1;
}

template<typename T>
static void replaceItem(QList<T> &list, T oldItem, T newItem)
{
    int i = list.indexOf(oldItem);
    if (i >= 0)
        list.replace(i, newItem);
}

void joinRoads(Document* theDocument, CommandList* theList, PropertiesDock* theDock)
{
    QList<Feature*> selection = theDock->selection();
    QHash<Coord,QList<Way*> > ends;

    foreach (Feature *F, selection)
        if (Way* R = CAST_WAY(F))
            if (!R->isClosed()) {
                Coord start = R->getNode(0)->position();
                ends[start] << R;
                Coord end = R->getNodes().back()->position();
                ends[end] << R;
            }

    foreach (Coord c, ends.keys()) {
        QList<Way*> ways = ends[c];
        if (ways.count() != 2) continue;
        Way *R1 = ways[0];
        Way *R2 = ways[1];
        if (R1 == R2) {
            Node *firstNode = R1->getNode(0);
            Node *lastNode = R1->getNode(R1->size()-1);
            if (firstNode != lastNode && R1->size() >= 4 && firstNode->position() == lastNode->position()) {
                // almost-area; close it properly
                mergeNodes(theDocument, theList, R1->getNode(0), R1->getNode(R1->size()-1));
            }
        } else {
            R1 = join(theDocument, theList, R1, R2);
            // replace R2 with R1 for subsequent operations
            replaceItem(ends[R2->getNode(0)->position()], R2, R1);
            replaceItem(ends[R2->getNodes().back()->position()], R2, R1);
            selection.removeOne(R2);
        }
    }
    theDock->setSelection(selection);
}

static bool handleWaysplitSpecialRestriction(Document* theDocument, CommandList* theList, Way* FirstPart, Way* NextPart, Relation* theRel)
{
    if (theRel->tagValue("type","") != "restriction")
        return false;

    for (int k=0; k < theRel->size(); k++)
    {
        // check for via
        if (theRel->getRole(k) == "via")
        {
            if (theRel->get(k) == FirstPart)
            {
                // whoops, we just split a via way, just add the new way, too
                theList->add(new RelationAddFeatureCommand(theRel, theRel->getRole(k), NextPart, k+1, theDocument->getDirtyOrOriginLayer(FirstPart->layer())));
                // we just added a member, so get over it
                k++;
            }
            else if ((theRel->get(k))->getType() == IFeature::Point)
            {
                // this seems to be a via node, check if it is member the nextPart
                if (NextPart->find(theRel->get(k)) < NextPart->size())
                {
                    // yes it is, so remove First Part and add Next Part to it
                    int idx = theRel->find(FirstPart);
                    theList->add(new RelationAddFeatureCommand(theRel, theRel->getRole(idx), NextPart, idx+1, theDocument->getDirtyOrOriginLayer(FirstPart->layer())));
                    theList->add(new RelationRemoveFeatureCommand(theRel, idx, theDocument->getDirtyOrOriginLayer(FirstPart->layer())));
                }
            }
            else if ((theRel->get(k))->getType() & IFeature::LineString)
            {
                // this is a way, check the nodes
                Way* W = CAST_WAY(theRel->get(k));
                for (int l=0; l<W->size(); l++)
                {
            // check if this node is member the nextPart
                    if (NextPart->find(W->get(l)) < NextPart->size())
                    {
                        // yes it is, so remove First Part and add Next Part to it
                        int idx = theRel->find(FirstPart);
                        theList->add(new RelationAddFeatureCommand(theRel, theRel->getRole(idx), NextPart, idx+1, theDocument->getDirtyOrOriginLayer(FirstPart->layer())));
                        theList->add(new RelationRemoveFeatureCommand(theRel, idx, theDocument->getDirtyOrOriginLayer(FirstPart->layer())));
                        break;
                    }
                }
            }
        }
    }
    return true;
}

static void handleWaysplitRelations(Document* theDocument, CommandList* theList, Way* FirstPart, Way* NextPart)
{
    /* since we may delete First Part from some Relations here, we first build a list of the relations to check */
    QList<Relation*> checkList;

    for (int j=0; j < FirstPart->sizeParents(); j++) {
        checkList.append(CAST_RELATION(FirstPart->getParent(j)));
    }

    for (int j=0; j < checkList.count(); j++) {
        Relation* L = checkList.at(j);
        if (!handleWaysplitSpecialRestriction(theDocument, theList, FirstPart, NextPart, L))
        {
            int idx = L->find(FirstPart);
            theList->add(new RelationAddFeatureCommand(L, L->getRole(idx), NextPart, idx+1, theDocument->getDirtyOrOriginLayer(FirstPart->layer())));
        }
    }
}


static void splitRoad(Document* theDocument, CommandList* theList, Way* In, const QList<Node*>& Points, QList<Way*>& Result)
{
    int pos = 0;
    if (In->isClosed()) {  // Special case: If area, rotate the area so that the start node is the first point of splitting

        QList<Node*> Target;
        for (int i=0; i < Points.size(); i++)
            if ((pos = In->find(Points[i])) != In->size()) {
                for (int j=pos+1; j<In->size(); ++j)
                    Target.push_back(In->getNode(j));
                for (int j=1; j<= pos; ++j)
                    Target.push_back(In->getNode(j));
                break;
            }
        if (pos == In->size())
            return;

        if (Points.size() == 1) // Special case: For a 1 point area splitting, de-close the road, i.e. duplicate the selected node
        {
            Node* N = g_backend.allocNode(theDocument->getDirtyOrOriginLayer(In->layer()), *(In->getNode(pos)));
            theList->add(new AddFeatureCommand(theDocument->getDirtyOrOriginLayer(In->layer()),N,true));

            Target.prepend(N);
        } else // otherwise, just close the modified area
            Target.prepend(In->getNode(pos));

        // Now, reconstruct the road/area
        while (In->size())
            theList->add(new WayRemoveNodeCommand(In,(int)0,theDocument->getDirtyOrOriginLayer(In->layer())));

        for (int i=0; i<Target.size(); ++i)
            theList->add(new WayAddNodeCommand(In,Target[i],theDocument->getDirtyOrOriginLayer(In->layer())));

        if (Points.size() == 1) {  // For 1-point, we are done
            Result.push_back(In);
            return;
        }
    }

    Way* FirstPart     = In;
    bool FirstWayValid = false;
    for (int i=1; (i+1)<FirstPart->size(); ++i)
    {
        if (std::find(Points.begin(),Points.end(),FirstPart->get(i)) != Points.end() and FirstPart->get(i) != FirstPart->get(i+1) \
                and (FirstWayValid or FirstPart->get(0) != FirstPart->get(i))) {
            Way* NextPart = g_backend.allocWay(theDocument->getDirtyOrOriginLayer(In->layer()));
            theList->add(new AddFeatureCommand(theDocument->getDirtyOrOriginLayer(In->layer()),NextPart,true));
            copyTags(NextPart,FirstPart);
            theList->add(new WayAddNodeCommand(NextPart, FirstPart->getNode(i), theDocument->getDirtyOrOriginLayer(In->layer())));
            while ( (i+1) < FirstPart->size() )
            {
                theList->add(new WayAddNodeCommand(NextPart, FirstPart->getNode(i+1), theDocument->getDirtyOrOriginLayer(In->layer())));
                theList->add(new WayRemoveNodeCommand(FirstPart,i+1,theDocument->getDirtyOrOriginLayer(In->layer())));
            }
            handleWaysplitRelations(theDocument, theList, FirstPart, NextPart);

            FirstWayValid = true;
            Result.push_back(FirstPart);
            FirstPart = NextPart;
            i=0;
        }
    }
    Result.push_back(FirstPart);

}

void splitRoads(Document* theDocument, CommandList* theList, PropertiesDock* theDock)
{
    QList<Way*> Roads, Result;
    QList<Node*> Points;
    for (int i=0; i<theDock->selectionSize(); ++i)
        if (Way* R = CAST_WAY(theDock->selection(i)))
            Roads.push_back(R);
        else if (Node* Pt = CAST_NODE(theDock->selection(i)))
            Points.push_back(Pt);

    if (Roads.size() == 0 && Points.size() == 1)
    {
        Way * R = Way::GetSingleParentRoadInner(Points[0]);
        if (R)
            Roads.push_back(R);
    }

    for (int i=0; i<Roads.size(); ++i)
        splitRoad(theDocument, theList,Roads[i],Points, Result);
    theDock->setSelection(Result);
}

/* Split road by the two nodes and return the way joining them, if there is one. */
Way *cutoutRoad(Document* theDocument, CommandList* theList, PropertiesDock* /* theDock */, Node *N1, Node *N2) {
    QList<Way*> Roads, Result;
    QList<Node*> Points;

	Way *R1 = Way::GetSingleParentRoadInner( N1 );
	Way *R2 = Way::GetSingleParentRoadInner( N2 );

	if (R1)
		Roads.push_back(R1);
	if (R2)
		Roads.push_back(R2);

    Points.push_back(N1);
    Points.push_back(N2);

    for (int i=0; i<Roads.size(); ++i)
        splitRoad(theDocument, theList, Roads[i], Points, Result);

    for (int i = 0; i < Result.size(); ++i) {
        if (Result[i]->isExtrimity(N1) && Result[i]->isExtrimity(N2))
            return Result[i];
    }
    return nullptr;
}

static void breakRoad(Document* theDocument, CommandList* theList, Way* R, Node* Pt)
{
    for (int i=0; i<R->size(); ++i) {
        if (R->get(i) == Pt) {
            Node* New = g_backend.allocNode(theDocument->getDirtyOrOriginLayer(R->layer()), *Pt);
            theList->add(new AddFeatureCommand(theDocument->getDirtyOrOriginLayer(R->layer()),New,true));
            copyTags(New,Pt);
            theList->add(new WayRemoveNodeCommand(R,i,theDocument->getDirtyOrOriginLayer(R->layer())));
            theList->add(new WayAddNodeCommand(R,New,i,theDocument->getDirtyOrOriginLayer(R->layer())));
        }
    }
    if (!Pt->sizeParents())
        theList->add(new RemoveFeatureCommand(theDocument,Pt));
}

void breakRoads(Document* theDocument, CommandList* theList, PropertiesDock* theDock)
{
    QList<Way*> Roads, Result;
    QList<Node*> Points;
    for (int i=0; i<theDock->selectionSize(); ++i)
        if (Way* R = CAST_WAY(theDock->selection(i)))
            Roads.push_back(R);
        else if (Node* Pt = CAST_NODE(theDock->selection(i)))
            Points.push_back(Pt);

    if (Roads.size() == 0 && Points.size() == 1)
    {
        for (int i=0; i<Points[0]->sizeParents() ; ++i) {
            Way * R = CAST_WAY(Points[0]->getParent(i));
            if (R && !R->isDeleted())
                Roads.push_back(R);
        }
    }

    if (Roads.size() == 1 && Points.size() ) {
        splitRoad(theDocument, theList,Roads[0],Points, Result);
        if (Roads[0]->area() > 0.0) {
            for (int i=0; i<Points.size(); ++i)
                breakRoad(theDocument, theList, Roads[0],Points[i]);
        } else {
            Roads = Result;
        }
    }

    for (int i=0; i<Roads.size(); ++i)
        for (int j=0; j<Roads[i]->size(); ++j)
            for (int k=i+1; k<Roads.size(); ++k)
                breakRoad(theDocument, theList, Roads[k],CAST_NODE(Roads[i]->get(j)));
}

bool canCreateJunction(PropertiesDock* theDock)
{
    return createJunction(nullptr, nullptr, theDock, false);
}

int createJunction(Document* theDocument, CommandList* theList, PropertiesDock* theDock, bool doIt)
{
    //TODO test that the junction do not already exists!

    QList<Way*> Roads, Result;
    for (int i=0; i<theDock->selectionSize(); ++i)
        if (Way* R = CAST_WAY(theDock->selection(i)))
            Roads.push_back(R);

    if (Roads.size() < 2)
        return 0;

    Way* R1 = Roads[0];
    Way* R2 = Roads[1];

    return Way::createJunction(theDocument, theList, R1, R2, doIt);
}

#define STREET_NUMBERS_LENGTH .0000629
#define STREET_NUMBERS_ANGLE 30.0

void createStreetNumbers(Document* theDocument, CommandList* theList, Way* theRoad, bool Left)
{
    QString streetName = theRoad->tagValue("name", "");
    QLineF l, l2, nv;

    Node* N;
    Way* R = g_backend.allocWay(theDocument->getDirtyOrOriginLayer());
    theList->add(new AddFeatureCommand(theDocument->getDirtyOrOriginLayer(),R,true));
    theList->add(new SetTagCommand(R, "addr:interpolation", ""));
    theList->add(new SetTagCommand(R, "addr:street", streetName));
    QPointF prevPoint;

    for (int j=0; j < theRoad->size(); j++) {
        if (j == 0) {
            l2 = QLineF(theRoad->getNode(j)->position(), theRoad->getNode(j+1)->position());
            l = l2;
            l.setAngle(l2.angle() + 180.);
            prevPoint = l.p2();
        } else
        if (j == theRoad->size()-1) {
            l = QLineF(theRoad->getNode(j)->position(), theRoad->getNode(j-1)->position());
            l2 = l;
            l2.setAngle(l.angle() + 180.);
        } else {
            l = QLineF(theRoad->getNode(j)->position(), theRoad->getNode(j-1)->position());
            l2 = QLineF(theRoad->getNode(j)->position(), theRoad->getNode(j+1)->position());
        }
        nv = l.normalVector().unitVector();

        qreal theAngle = (l.angle() - l2.angle());
        if (theAngle < 0.0) theAngle = 360. + theAngle;
        theAngle /= 2.;
        nv.setAngle(l2.angle() + theAngle);
        nv.setLength(STREET_NUMBERS_LENGTH/sin(angToRad(theAngle)));
        if (Left)
            nv.setAngle(nv.angle() + 180.0);

        QLineF lto(prevPoint, nv.p2());
        lto.setLength(lto.length()+STREET_NUMBERS_LENGTH);
        QPointF pto;

        bool intersectedTo = false;
        for (int k=0; k < theRoad->getNode(j)->sizeParents(); ++k) {
            Way* I = CAST_WAY(theRoad->getNode(j)->getParent(k));
            if (!I || I == theRoad || I->isDeleted())
                continue;

            for (int m=0; m < I->size()-1; ++m) {
                QLineF l3 = QLineF(I->getNode(m)->position(), I->getNode(m+1)->position());
                QPointF theIntersection;
                if (lto.intersect(l3, &theIntersection) == QLineF::BoundedIntersection) {
                    intersectedTo = true;
                    QLineF lt = QLineF(prevPoint, theIntersection);
                    if (lt.length() < lto.length())
                        lto = lt;
                }
            }
        }

        if (j != 0) {
            QLineF lfrom = QLineF(nv.p2(), prevPoint);
            lfrom.setLength(lfrom.length()*2.);
            QPointF pfrom;

            bool intersectedFrom = false;
            for (int k=0; k < theRoad->getNode(j-1)->sizeParents(); ++k) {
                Way* I = CAST_WAY(theRoad->getNode(j-1)->getParent(k));
                if (!I || I == theRoad || I->isDeleted())
                    continue;

                for (int m=0; m < I->size()-1; ++m) {
                    QLineF l3 = QLineF(I->getNode(m)->position(), I->getNode(m+1)->position());
                    QPointF theIntersection;
                    if (lfrom.intersect(l3, &theIntersection) == QLineF::BoundedIntersection) {
                        intersectedFrom = true;
                        QLineF lt = QLineF(nv.p2(), theIntersection);
                        if (lt.length() < lfrom.length())
                            lfrom = lt;
                    }
                }
            }
            if (intersectedFrom) {
                lfrom.setLength(lfrom.length() - STREET_NUMBERS_LENGTH);
                pfrom = lfrom.p2();

                R = g_backend.allocWay(theDocument->getDirtyOrOriginLayer());
                theList->add(new AddFeatureCommand(theDocument->getDirtyOrOriginLayer(),R,true));
                theList->add(new SetTagCommand(R, "addr:interpolation", ""));
                theList->add(new SetTagCommand(R, "addr:street", streetName));

                N = g_backend.allocNode(theDocument->getDirtyOrOriginLayer(), Coord(pfrom));
                theList->add(new AddFeatureCommand(theDocument->getDirtyOrOriginLayer(),N,true));
                theList->add(new WayAddNodeCommand(R, N, theDocument->getDirtyOrOriginLayer(R->layer())));
                theList->add(new SetTagCommand(N, "addr:housenumber", ""));
            } else {
                pfrom = prevPoint;
            }
        }

        if (intersectedTo) {
            if (j != 0) {
                lto.setLength(lto.length() - STREET_NUMBERS_LENGTH);
                pto = lto.p2();

                N = g_backend.allocNode(theDocument->getDirtyOrOriginLayer(), Coord(pto));
                theList->add(new AddFeatureCommand(theDocument->getDirtyOrOriginLayer(),N,true));
                theList->add(new WayAddNodeCommand(R, N, theDocument->getDirtyOrOriginLayer(R->layer())));
                theList->add(new SetTagCommand(N, "addr:housenumber", ""));
            }
        } else {
            if (theAngle < 85. || theAngle > 95. || j== 0 || j == theRoad->size()-1) {
                N = g_backend.allocNode(theDocument->getDirtyOrOriginLayer(), Coord(nv.p2()));
                theList->add(new AddFeatureCommand(theDocument->getDirtyOrOriginLayer(),N,true));
                theList->add(new WayAddNodeCommand(R, N, theDocument->getDirtyOrOriginLayer(R->layer())));
                theList->add(new SetTagCommand(N, "addr:housenumber", ""));
            }

            pto = nv.p2();
        }
        prevPoint = nv.p2();
    }
}

void addStreetNumbers(Document* theDocument, CommandList* theList, PropertiesDock* theDock)
{
    QList<Way*> Roads;
    for (int i=0; i<theDock->selectionSize(); ++i)
        if (Way* R = CAST_WAY(theDock->selection(i)))
            Roads.push_back(R);

    if (Roads.isEmpty())
        return;

    QList<Way*>::const_iterator it = Roads.constBegin();
    for (;it != Roads.constEnd(); ++it) {
        if((*it)->size() < 2)
            continue;

        createStreetNumbers(theDocument, theList, (*it), false);
        createStreetNumbers(theDocument, theList, (*it), true);
    }

    if (Roads.size() == 1)
        theList->setFeature(Roads.at(0));
}

void alignNodes(Document* theDocument, CommandList* theList, PropertiesDock* theDock)
{
    if (theDock->selectionSize() < 3) //thre must be at least 3 nodes to align something
        return;

    //We build a list of selected nodes
    QList<Node*> Nodes;
    for (int i=0; i<theDock->selectionSize(); ++i)
        if (Node* N = CAST_NODE(theDock->selection(i)))
            Nodes.push_back(N);

    //we check that we have at least 3 nodes and the first two can give a line
    if(Nodes.size() < 3)
        return;
    if(Nodes[0]->position() == Nodes[1]->position())
        return;

    //we do the alignement
    Coord pos(0,0);
    const Coord p1(Nodes[0]->position());
    const Coord p2(Nodes[1]->position()-p1);
    const qreal slope = angle(p2);
    for (int i=2; i<Nodes.size(); ++i) {
        pos=Nodes[i]->position()-p1;
        rotate(pos,-slope);
        pos.setY(0);
        rotate(pos,slope);
        pos=pos+p1;
        theList->add(new MoveNodeCommand( Nodes[i], pos, theDocument->getDirtyOrOriginLayer(Nodes[i]->layer()) ));
    }
}

void bingExtract(Document* theDocument, CommandList* theList, PropertiesDock* theDock, CoordBox vp)
{
    //We build a list of selected nodes
    QList<Node*> Nodes;
    for (int i=0; i<theDock->selectionSize(); ++i)
        if (Node* N = CAST_NODE(theDock->selection(i)))
            Nodes.push_back(N);

    //we check that we have at least 2 nodes
    if(Nodes.size() < 2)
        return;
    if(Nodes[0]->position() == Nodes[1]->position())
        return;

    //http://3667a17de9b94ccf8fd278f9de62dae4.cloudapp.net/
    QString u("http://magicshop.cloudapp.net/DetectRoad.svc/detect/?");
    u.append(QString("pt1=%1,%2").arg(COORD2STRING(Nodes[0]->position().y())).arg(COORD2STRING(Nodes[0]->position().x())));
    u.append(QString("&pt2=%1,%2").arg(COORD2STRING(Nodes[1]->position().y())).arg(COORD2STRING(Nodes[1]->position().x())));
    u.append(QString("&bbox=%1,%2,%3,%4")
             .arg(COORD2STRING(vp.top()))
             .arg(COORD2STRING(vp.left()))
             .arg(COORD2STRING(vp.bottom()))
             .arg(COORD2STRING(vp.right()))
             );

    QString sXml;
    if (!Utils::sendBlockingNetRequest(QUrl(u), sXml)) {
        QMessageBox::critical(0, QApplication::tr("Bing Road Detect"), QApplication::tr("Cannot get output."));
        return;
    }
    qDebug() << sXml;

    //Import the OSC
    Document* newDoc;
    QDomDocument xmlDoc;
    xmlDoc.setContent(sXml);
    if (!(newDoc = Document::getDocumentFromXml(&xmlDoc))) {
        QMessageBox::critical(0, QApplication::tr("Bing Road Detect"), QApplication::tr("No valid data."));
        return;
    }

    if (!newDoc->size()) {
        QMessageBox::critical(0, QApplication::tr("Bing Road Detect"), QApplication::tr("Cannot parse output."));
    } else {
        DrawingLayer* newLayer = new DrawingLayer( "Bing Road Extract" );
        newLayer->setUploadable(false);
        theDocument->add(newLayer);
        QList<Feature*> theFeats = theDocument->mergeDocument(newDoc, newLayer, theList);

        // Merge in initial nodes
        Node * N0 = nullptr;
        Node * N1 = nullptr;
        qreal Best0 = 180., Best1 = 180.;

        foreach (Feature* F, theFeats) {
            if (CHECK_NODE(F)) {
                Node* N = STATIC_CAST_NODE(F);
                qreal B = distance(Nodes[0]->position(), N->position());
                if (B < Best0) {
                    N0 = N;
                    Best0 = B;
                }
                B = distance(Nodes[1]->position(), N->position());
                if (B < Best1) {
                    N1 = N;
                    Best1 = B;
                }
            }
        }
        if (N0)
            mergeNodes(theDocument, theList, Nodes[0], N0);
        if (N1)
            mergeNodes(theDocument, theList, Nodes[1], N1);
    }
    delete newDoc;
}

void spreadNodes(Document* theDocument, CommandList* theList, PropertiesDock* theDock)
{
    // There must be at least 3 nodes to align something
    if (theDock->selectionSize() < 3)
        return;

    // We build a list of selected nodes
    // Sort by distance along the line between the first two nodes
    QList<Node*> Nodes;
    QList<float> Metrics;
    Coord p;
    Coord delta;
    for (int i=0; i<theDock->selectionSize(); ++i) {
        if (Node* N = CAST_NODE(theDock->selection(i))) {
            Coord pos(N->position());
            if (Nodes.size() == 0) {
                p = pos;
                Nodes.push_back(N);
                Metrics.push_back(0.0f);
            } else if (Nodes.size() == 1) {
                delta = pos - p;
                // First two nodes must form a line
                if (delta.isNull())
                    return;
                Nodes.push_back(N);
                Metrics.push_back(delta.x()*delta.x() + delta.y()*delta.y());
            } else {
                pos = pos - p;
                qreal metric = pos.x()*delta.x() + pos.y()*delta.y();
                // This could be done more efficiently with a binary search
                for (int j = 0; j < Metrics.size(); ++j) {
                    if (metric < Metrics[j]) {
                        Nodes.insert(j, N);
                        Metrics.insert(j, metric);
                        goto inserted;
                    }
                }
                Nodes.push_back(N);
                Metrics.push_back(metric);
inserted:
                ;
            }
        }
    }

    // We check that we have at least 3 nodes
    if(Nodes.size() < 3)
        return;

    // Do the spreading between the extremes
    p = Nodes[0]->position();
    delta = (Nodes[Nodes.size()-1]->position() - p) / (Nodes.size()-1);

    for (int i=1; i<Nodes.size()-1; ++i) {
        p = p + delta;
        theList->add(new MoveNodeCommand( Nodes[i], p, theDocument->getDirtyOrOriginLayer(Nodes[i]->layer()) ));
    }
}

static void mergeNodes(Document* theDocument, CommandList* theList, Node *node1, Node *node2)
{
    QList<Feature*> alt;
    alt.append(node1);
    Feature::mergeTags(theDocument, theList, node1, node2);
    theList->add(new RemoveFeatureCommand(theDocument, node2, alt));
}

void mergeNodes(Document* theDocument, CommandList* theList, PropertiesDock* theDock)
{
    if (theDock->selectionSize() <= 1)
        return;
    QList<Node*> Nodes;
    QList<Feature*> alt;
    for (int i=0; i<theDock->selectionSize(); ++i)
        if (Node* N = CAST_NODE(theDock->selection(i)))
            Nodes.push_back(N);
    Node* merged = Nodes[0];
    alt.push_back(merged);
    for (int i=1; i<Nodes.size(); ++i) {
        Feature::mergeTags(theDocument, theList, merged, Nodes[i]);
        theList->add(new RemoveFeatureCommand(theDocument, Nodes[i], alt));
    }
}

void detachNode(Document* theDocument, CommandList* theList, PropertiesDock* theDock)
{
    QList<Way*> Roads, Result;
    QList<Node*> Points;
    for (int i=0; i<theDock->selectionSize(); ++i)
        if (Way* R = CAST_WAY(theDock->selection(i)))
            Roads.push_back(R);
        else if (Node* Pt = CAST_NODE(theDock->selection(i)))
            Points.push_back(Pt);

    if (Roads.size() > 1 && Points.size() > 1)
        return;

    if (Roads.size() == 0 && Points.size())
    {
        for (int i=0; i<Points.size(); ++i) {
            Way * R = Way::GetSingleParentRoad(Points[i]);
            if (R)
                theList->add(new WayRemoveNodeCommand(R, Points[i],
                    theDocument->getDirtyOrOriginLayer(R)));
        }
    }

    if (Roads.size() > 1 && Points.size() == 1)
    {
        for (int i=0; i<Roads.size(); ++i) {
            if (Roads[i]->find(Points[0]) < Roads[i]->size())
                theList->add(new WayRemoveNodeCommand(Roads[i], Points[0],
                    theDocument->getDirtyOrOriginLayer(Roads[i])));
        }
    }

    if (Roads.size() == 1 && Points.size())
    {
        for (int i=0; i<Points.size(); ++i) {
            if (Roads[0]->find(Points[i]) < Roads[0]->size())
                theList->add(new WayRemoveNodeCommand(Roads[0], Points[i],
                    theDocument->getDirtyOrOriginLayer(Roads[0])));
        }
    }
}

void commitFeatures(Document* theDocument, CommandList* theList, PropertiesDock* theDock)
{
    QSet<Feature*> Features;
    QQueue<Feature*> ToAdd;

    for (int i=0; i<theDock->selectionSize(); ++i)
        if (!theDock->selection(i)->isUploadable() && !theDock->selection(i)->isSpecial())
            ToAdd.enqueue(theDock->selection(i));

    while (!ToAdd.isEmpty()) {
        Feature *feature = ToAdd.dequeue();
        Features.insert(feature);
        for (int j=0; j < feature->size(); ++j) {
            Feature *member = feature->get(j);
            if (!Features.contains(member)) {
                ToAdd.enqueue(member);
            }
        }
    }

    if (Features.size()) {
        Layer *layer = theDocument->getDirtyLayer();
        foreach (Feature *feature, Features)
            theList->add(new AddFeatureCommand(layer,feature,true));
    }
}

void addRelationMember(Document* theDocument, CommandList* theList, PropertiesDock* theDock)
{
    Relation* theRelation = nullptr;
    QList<Feature*> Features;
    for (int i=0; i<theDock->selectionSize(); ++i)
        if ((theDock->selection(i)->getClass() == "Relation") && !theRelation)
            theRelation = CAST_RELATION(theDock->selection(i));
        else
            Features.push_back(theDock->selection(i));

    if (!(theRelation && Features.size())) return;

    for (int i=0; i<Features.size(); ++i) {
        theList->add(new RelationAddFeatureCommand(theRelation, "", Features[i], theDocument->getDirtyOrOriginLayer(theRelation->layer())));
    }
}

void removeRelationMember(Document* theDocument, CommandList* theList, PropertiesDock* theDock)
{
    Relation* theRelation = nullptr;
    QList<Feature*> Features;
    for (int i=0; i<theDock->selectionSize(); ++i)
        if ((theDock->selection(i)->getClass() == "Relation") && !theRelation)
            theRelation = CAST_RELATION(theDock->selection(i));
        else
            Features.push_back(theDock->selection(i));

    if (!theRelation && Features.size() == 1)
        theRelation = Feature::GetSingleParentRelation(Features[0]);
    if (!(theRelation && Features.size())) return;

    int idx;
    for (int i=0; i<Features.size(); ++i) {
        if ((idx = theRelation->find(Features[i])) != theRelation->size())
            theList->add(new RelationRemoveFeatureCommand(theRelation, idx, theDocument->getDirtyOrOriginLayer(theRelation->layer())));
    }
}

void addToMultipolygon(Document* theDocument, CommandList* theList, PropertiesDock* theDock)
{
    Relation* theRelation = nullptr;
    QList<Way*> theWays;
    for (int i=0; i<theDock->selectionSize(); ++i) {
        if (!theRelation && CHECK_RELATION(theDock->selection(i)) && theDock->selection(i)->tagValue("type", "") == "multipolygon")
            theRelation = STATIC_CAST_RELATION(theDock->selection(i));
        if (CHECK_WAY(theDock->selection(i)))
            theWays << STATIC_CAST_WAY(theDock->selection(i));
    }

    if (!theWays.size())
        return;

    if (theRelation) {
        for (int i=0; i<theRelation->size(); ++i) {
            if (CHECK_WAY(theRelation->get(i)) && !theWays.contains(STATIC_CAST_WAY(theRelation->get(i)))) {
                theWays << STATIC_CAST_WAY(theRelation->get(i));
            }
        }
    }

    Way* outer = theWays[0];
    for (int i=1; i<theWays.size(); ++i) {
        if (outer->boundingBox().contains(theWays[i]->boundingBox()))
            continue;
        if (theWays[i]->boundingBox().contains(outer->boundingBox()))
            outer = theWays[i];
        else
            if (theWays[i]->boundingBox().intersects(outer->boundingBox()))
                outer = nullptr;
    }
    if (outer) {
        if (!theRelation) {
            theRelation = g_backend.allocRelation(theDocument->getDirtyOrOriginLayer());
            theList->add(new AddFeatureCommand(theDocument->getDirtyOrOriginLayer(),theRelation,true));
        } else {
            for (int i=0; i<theRelation->size(); ++i)
                theList->add(new RelationRemoveFeatureCommand(theRelation, 0));
        }
        theRelation->setTag("type", "multipolygon");
    } else
        return;

    theList->add(new RelationAddFeatureCommand(theRelation, "outer", outer));
    for (int i=0; i<theWays.size(); ++i) {
        if (theWays[i] != outer) {
            theList->add(new RelationAddFeatureCommand(theRelation, "inner", theWays[i]));
        }
    }
}

/* Subdivide theRoad between index and index+1 into divisions segments.
 * divisions-1 new nodes are created starting at index index+1.
 */
static void subdivideRoad(Document* theDocument, CommandList* theList,
                          Way* theRoad, unsigned int index, unsigned int divisions)
{
    Node* N0 = theRoad->getNode(index);
    Node* N1 = theRoad->getNode(index+1);
    Coord nodeBase = N0->position();
    Coord nodeDelta = (N1->position() - nodeBase) / divisions;
    for (unsigned int i = 1; i < divisions; ++i) {
        nodeBase = nodeBase + nodeDelta;
        Node* newNode = g_backend.allocNode(theDocument->getDirtyOrOriginLayer(theRoad), nodeBase);
        theList->add(new AddFeatureCommand(theDocument->getDirtyOrOriginLayer(theRoad), newNode, true));
        theList->add(new WayAddNodeCommand(theRoad, newNode, index + i, theDocument->getDirtyOrOriginLayer(theRoad)));
    }
}

bool canSubdivideRoad(PropertiesDock* theDock, Way** outTheRoad, unsigned int* outEdge)
{
    // Get the selected way and nodes
    Way* theRoad = nullptr;
    Node* theNodes[2] = { nullptr, nullptr };
    for (int i = 0; i < theDock->selectionSize(); ++i) {
        if ((theDock->selection(i)->getClass() == "Way") && !theRoad)
            theRoad = CAST_WAY(theDock->selection(i));
        else if (theDock->selection(i)->getClass() == "Node") {
            if (!theNodes[0])
                theNodes[0] = CAST_NODE(theDock->selection(i));
            else if (!theNodes[1])
                theNodes[1] = CAST_NODE(theDock->selection(i));
        }
    }

    // If the way has only two nodes, use them
    if (theRoad && theRoad->size() == 2) {
        theNodes[0] = theRoad->getNode(0);
        theNodes[1] = theRoad->getNode(1);
        // Now this would just be silly
        if (theNodes[0] == theNodes[1])
            return false;
    }

    // A way and 2 nodes
    if (!theRoad || !theNodes[0] || !theNodes[1])
        return false;

    // Nodes must be adjacent in the way
    int numNodes = theRoad->size();
    int nodeIndex0 = -1;
    for (int i = 0; i < numNodes-1; ++i) {
        Node* N0 = theRoad->getNode(i);
        Node* N1 = theRoad->getNode(i+1);
        if ((N0 == theNodes[0] && N1 == theNodes[1]) ||
            (N0 == theNodes[1] && N1 == theNodes[0])) {
            nodeIndex0 = i;
            break;
        }
    }

    if (nodeIndex0 < 0)
        return false;

    if (outTheRoad)
        *outTheRoad = theRoad;
    if (outEdge)
        *outEdge = nodeIndex0;

    return true;
}

void subdivideRoad(Document* theDocument, CommandList* theList, PropertiesDock* theDock, unsigned int divisions)
{
    // Subdividing into 1 division is no-op
    if (divisions < 2)
        return;

    // Get the selected way and nodes
    Way* theRoad;
    unsigned int edge;
    if (!canSubdivideRoad(theDock, &theRoad, &edge))
        return;
    subdivideRoad(theDocument, theList, theRoad, edge, divisions);
}

/* Remove nodes between theNodes in theArea into a separate way newArea.
 * newArea's first node will be theNodes[0], and it's last nodes will be
 * theNodes[1] and theNodes[0].
 */
static bool splitArea(Document* theDocument, CommandList* theList,
                      Way* theArea, unsigned int nodes[2], Way** outNewArea)
{
    // Make sure nodes are in order
    if (nodes[0] > nodes[1])
        qSwap(nodes[0], nodes[1]);

    // And not next to one another
    if (nodes[0] + 1 == nodes[1] ||
            (nodes[0] == 0 && nodes[1] == (unsigned int)theArea->size() - 2)) {
        qDebug() << "Nodes must not be adjacent";
        return false;
    }

    Node* theNodes[2];
    for (int i = 0; i < 2; ++i)
        theNodes[i] = theArea->getNode(nodes[i]);

    // Extract nodes between nodes[0] and nodes[1] into a separate area
    // and remove the nodes from the original area
    Way* newArea = g_backend.allocWay(theDocument->getDirtyOrOriginLayer(theArea));
    theList->add(new AddFeatureCommand(theDocument->getDirtyOrOriginLayer(theArea), newArea, true));
    copyTags(newArea, theArea);
    theList->add(new WayAddNodeCommand(newArea, theNodes[0], theDocument->getDirtyOrOriginLayer(theArea)));
    for (unsigned int i = nodes[0]+1; i < nodes[1]; ++i) {
        theList->add(new WayAddNodeCommand(newArea, theArea->getNode(nodes[0]+1),
                                           theDocument->getDirtyOrOriginLayer(theArea)));
        theList->add(new WayRemoveNodeCommand(theArea, theArea->getNode(nodes[0]+1),
                                              theDocument->getDirtyOrOriginLayer(theArea)));
    }
    theList->add(new WayAddNodeCommand(newArea, theNodes[1], theDocument->getDirtyOrOriginLayer(theArea)));
    theList->add(new WayAddNodeCommand(newArea, theNodes[0], theDocument->getDirtyOrOriginLayer(theArea)));
    handleWaysplitRelations(theDocument, theList, theArea, newArea);

    if (outNewArea)
        *outNewArea = newArea;

    return true;
}

// Cycle the node index in an area so it's >= 0, < size-1 (never the last node).
static int cycleNodeIndex(Way* area, int index)
{
    if (index >= area->size()-1)
        index -= area->size()-1;
    else if (index < 0)
        index += area->size()-1;
    return index;
}

// Find the index of a node in an area as close as possible to expected.
// Return -1 if node isn't in area. This is useful for tracing around shared
// nodes of two areas, where we want the indexes to be continuous so we can
// detect where the edges depart from one another.
static int nodeIndexInArea(Way* area, Node* node, int expected)
{
    // parent list is gonna be shorter, so check this first
    for (int i = 0; i < node->sizeParents(); ++i)
        if (node->getParent(i) == area) {
            // now we know it's on the list, find the index
            if (area->getNode(expected) == node)
                return expected;
            int forward = expected;
            int backward = expected;
            for (int j = 0; j < (area->size()-1)/2; ++j) {
                forward = cycleNodeIndex(area, forward + 1);
                if (area->getNode(forward) == node)
                    return forward;
                backward = cycleNodeIndex(area, backward - 1);
                if (area->getNode(backward) == node)
                    return backward;
            }
            break;
        }
    return -1;
}

// Determine if two area node indexes are adjacent, and in a particular
// direction if direction is 1 or -1. If they are adjacent, direction is set to
// 1 or -1. This is useful tracing the shared nodes between two areas as the
// direction is initialised to 0 and determined on the first check, and then
// enforced on later checks.
static bool nodeIndexesAdjacent(Way* way, unsigned int n1, unsigned int n2, int* direction)
{
    int dir = n2 - n1;
    if (abs(dir) > 1) {
        // wrap?
        if (!n1)
            n1 = way->size() - 1;
        else if (!n2)
            n2 = way->size() - 1;
        else
            return false;
        dir = n2 - n1;
    }
    if (abs(dir) != 1)
        return false;

    // in the correct direction?
    if (!*direction) {
        *direction = dir;
        return true;
    } else
        return *direction == dir;
}

// Join selected areas which are sharing edges. Multiple nodes adjacent in both
// areas must be shared for them to be joined.
// FIXME This does not currently produce multipolygons when the areas form an
// enclosed area. Instead the edge will run around the outside, cut into the
// middle and run around the inner part, and then cut back out the same way.
void joinAreas(Document* theDocument, CommandList* theList, PropertiesDock* theDock)
{
    // Collect the set of selected areas
    QSet<Way*> areas;
    for (int i = 0; i < theDock->selectionSize(); ++i)
        if (theDock->selection(i)->getClass() == "Way") {
            Way* area = CAST_WAY(theDock->selection(i));
            if (area->isClosed())
                areas << area;
        }

findNextJoin:
    // Go through areas in the set
    while (areas.size() > 1) {
        Way* area = *areas.begin();
        // Go through nodes shared between multiple features
        for (int i = 0; i < area->size() - 1; ++i) {
            Node* node = area->getNode(i);
            if (node->sizeParents() <= 1)
                continue;
            // Go through parents of the node that are areas in our set
            for (int p1 = 0; p1 < node->sizeParents(); ++p1) {
                if (!(node->getParent(p1)->getType() & IFeature::Polygon))
                    continue;
                Way* otherArea = CAST_WAY(node->getParent(p1));
                if (otherArea == area || !areas.contains(otherArea))
                    continue;
                // Look for the first shared mutually adjacent node.
                for (int n = 0; n < otherArea->size()-1; ++n) {
                    if (otherArea->getNode(n) != node)
                        continue;
                    int otherFirst = n;
                    int otherLast = otherFirst;
                    int otherDirection = 0;
                    int first = i;
                    int count = 1;
                    // We want node i to be the first shared node, so if the
                    // one before it is also shared and adjacent we can safely
                    // ignore it as we'll hit it again later
                    int lastOtherIndex = nodeIndexInArea(otherArea, area->getNode(cycleNodeIndex(area, i-1)), n);
                    if (lastOtherIndex != -1 && nodeIndexesAdjacent(otherArea, n, lastOtherIndex, &otherDirection))
                        continue;
                    // Continue forwards to see how many nodes are shared with
                    // the other area and adjacent. The first nodeIndexesAdjacent
                    // will set otherDirection and later nodes have to keep
                    // going in that direction.
                    for (int j = 1; j < area->size(); ++j) {
                        int jIndex = (i + j) % (area->size()-1);
                        int otherIndex = nodeIndexInArea(otherArea, area->getNode(jIndex), cycleNodeIndex(otherArea, otherLast + otherDirection));
                        // is next node in same area?
                        if (otherIndex == -1)
                            break;
                        // must be adjacent to last node (and in same direction)
                        if (!nodeIndexesAdjacent(otherArea, otherLast, otherIndex, &otherDirection))
                            break;
                        otherLast = otherIndex;
                        ++count;
                    }
                    if (count < 2)
                        continue;

                    // If all of area's nodes are shared (area is enclosed
                    // inside otherArea), then it is better to swap area and
                    // otherArea so that area is the one that gets removed.
                    if (count == area->size()) {
                        qSwap(area, otherArea);
                        qSwap(first, otherFirst);
                    }

                    // Remove nodes along the shared edge from area. No need to
                    // remove from otherArea, it'll get removed anyway.
                    for (int del = 0; del < count-2; ++del) {
                        int delIndex = cycleNodeIndex(area, first+1);
                        theList->add(new WayRemoveNodeCommand(area, delIndex, theDocument->getDirtyOrOriginLayer(area->layer())));
                        if (first > delIndex)
                            --first;
                    }

                    // If otherArea is completely enclosed then the end nodes
                    // of the shared edge are the same and are now adjacent, so
                    // remove one of them.
                    if (count == otherArea->size())
                        theList->add(new WayRemoveNodeCommand(area, first, theDocument->getDirtyOrOriginLayer(area->layer())));
                    // Seal the crack left over from an enclosed otherArea
                    // until the nodes on either side of 'first' diverge.
                    while (area->size() > 3 &&
                           area->getNode(cycleNodeIndex(area, first-1)) == area->getNode(cycleNodeIndex(area, first+1))) {
                        theList->add(new WayRemoveNodeCommand(area, first, theDocument->getDirtyOrOriginLayer(area->layer())));
                        theList->add(new WayRemoveNodeCommand(area, first, theDocument->getDirtyOrOriginLayer(area->layer())));
                        first = cycleNodeIndex(area, first-1);
                    }
                    // Add the remaining nodes of otherArea that aren't shared
                    // to area.
                    for (int add = 1; add < otherArea->size()-count; ++add) {
                        int index = cycleNodeIndex(otherArea, otherFirst - otherDirection*add);
                        theList->add(new WayAddNodeCommand(area, otherArea->getNode(index), ++first, theDocument->getDirtyOrOriginLayer(area->layer())));
                    }
                    // Merge tags and remove otherArea.
                    Feature::mergeTags(theDocument, theList, area, otherArea);
                    otherArea->deleteChildren(theDocument, theList);
                    theList->add(new RemoveFeatureCommand(theDocument, otherArea));
                    areas.remove(otherArea);
                    // Remove otherArea from selection.
                    QList<Feature*> sel = theDock->selection();
                    sel.removeOne(otherArea);
                    theDock->setSelection(sel);
                    // Restart the process again.
                    goto findNextJoin;
                }
            }
        }
        areas.remove(area);
    }
}

bool canSplitArea(PropertiesDock* theDock, Way** outTheArea, unsigned int outNodes[2])
{
    if (theDock->selectionSize() != 3)
        return false;

    // Get the selected way and nodes
    Way* theArea = nullptr;
    Node* theNodes[2] = { nullptr, nullptr };
    for (int i = 0; i < theDock->selectionSize(); ++i) {
        if ((theDock->selection(i)->getClass() == "Way") && !theArea)
            theArea = CAST_WAY(theDock->selection(i));
        else if (theDock->selection(i)->getClass() == "Node") {
            if (!theNodes[0])
                theNodes[0] = CAST_NODE(theDock->selection(i));
            else if (!theNodes[1])
                theNodes[1] = CAST_NODE(theDock->selection(i));
        }
    }

    // A way and 2 nodes
    if (!theArea || !theNodes[0] || !theNodes[1])
        return false;

    // Way must be closed
    if (!theArea->isClosed())
        return false;

    // Nodes must belong to way
    unsigned int numNodes = theArea->size();
    unsigned int nodeIndex[2];
    for (int i = 0; i < 2; ++i) {
        nodeIndex[i] = theArea->find(theNodes[i]);
        if (nodeIndex[i] >= numNodes)
            return false;
    }

    // Make sure nodes are in order
    if (nodeIndex[0] > nodeIndex[1])
        qSwap(nodeIndex[0], nodeIndex[1]);

    // And not next to one another
    if (nodeIndex[0] + 1 == nodeIndex[1] ||
            (nodeIndex[0] == 0 && nodeIndex[1] == (unsigned int)theArea->size() - 2))
        return false;


    if (outTheArea)
        *outTheArea = theArea;
    if (outNodes) {
        outNodes[0] = nodeIndex[0];
        outNodes[1] = nodeIndex[1];
    }

    return true;
}

void splitArea(Document* theDocument, CommandList* theList, PropertiesDock* theDock)
{
    Way* theArea;
    unsigned int nodes[2];
    if (!canSplitArea(theDock, &theArea, nodes))
        return;

    Way* newArea;
    if (!splitArea(theDocument, theList, theArea, nodes, &newArea))
        return;

    theDock->setSelection(QList<Way*>() << theArea << newArea);
}

static void terraceArea(Document* theDocument, CommandList* theList,
                        Way* theArea, unsigned int sides[2],
                        unsigned int divisions, int startNode,
                        QList<Feature*>* outAreas)
{
    // We're adding nodes so ordering is important
    if (sides[0] > sides[1])
        qSwap(sides[0], sides[1]);

    bool reverse = (startNode >= 0 && ((5 + startNode - sides[1]) & 2));

    // Subdivide both sides
    subdivideRoad(theDocument, theList, theArea, sides[1], divisions);
    subdivideRoad(theDocument, theList, theArea, sides[0], divisions);

    // Split apart
    for (unsigned int i = sides[0] + divisions - 1; i > sides[0]; --i) {
        Way* newArea;
        unsigned int nodes[2] = { i, i + 3 };
        splitArea(theDocument, theList, theArea, nodes, &newArea);
        if (newArea && outAreas) {
            if (reverse)
                outAreas->push_front(newArea);
            else
                outAreas->push_back(newArea);
        }
    }
    if (outAreas) {
        if (reverse)
            outAreas->push_front(theArea);
        else
            outAreas->push_back(theArea);
    }
}

bool canTerraceArea(PropertiesDock* theDock, Way** outTheArea, int* startNode)
{
    // Get the selected area
    Way* theArea = nullptr;
    Node* theNode = nullptr;
    for (int i = 0; i < theDock->selectionSize(); ++i)
        if ((theDock->selection(i)->getClass() == "Way") && !theArea) {
            theArea = CAST_WAY(theDock->selection(i));
        } else if (startNode && theDock->selection(i)->getClass() == "Node") {
            theNode = CAST_NODE(theDock->selection(i));
        }
    if (!theArea || !theArea->isClosed())
        return false;

    if (startNode) {
        if (theNode)
            *startNode = theArea->find(theNode);
        else
            *startNode = -1;
    }

    // Only work with 4 edges for now
    if (theArea->size() != 5)
        return false;

    if (outTheArea)
        *outTheArea = theArea;

    return true;
}

void terraceArea(Document* theDocument, CommandList* theList, PropertiesDock* theDock, unsigned int divisions)
{
    Way* theArea;
    int startNode;
    if (!canTerraceArea(theDock, &theArea, &startNode))
        return;

    qreal longestLen = 0.0f;
    unsigned int sides[2];
    sides[0] = 0;
    for (int i = 0; i < theArea->size()-1; ++i) {
        const Coord p1(theArea->getNode(i)->position());
        const Coord p2(theArea->getNode(i+1)->position());
        const qreal len = p1.distanceFrom(p2);
        if (len > longestLen) {
            longestLen = len;
            sides[0] = i;
        }
    }
    sides[1] = (sides[0] + 2) % (theArea->size() - 1);

    QList<Feature*> areas;
    terraceArea(theDocument, theList, theArea, sides, divisions, startNode, &areas);

    theDock->setSelection(areas);
}

// Remove repeated (adjacent) nodes in ways.
static void removeRepeatsInRoads(Document* theDocument, CommandList* theList, PropertiesDock* theDock)
{
    for (int i = 0; i < theDock->selectionSize(); ++i) {
        if (theDock->selection(i)->getClass() == "Way") {
            Way *way = CAST_WAY(theDock->selection(i));
            Node *last = 0;
            for (int j = 0; j < way->size();) {
                Node *node = way->getNode(j);
                if (node == last) {
                    theList->add(new WayRemoveNodeCommand(way, j, theDocument->getDirtyOrOriginLayer(way->layer())));
                    continue;
                }
                last = node;
                ++j;
            }
        }
    }
}

// Align a set of midpoints in a specific direction
static void axisAlignAlignMidpoints(QVector<QPointF> &midpoints, const QVector<qreal> weights, const QPointF &dir, int begin, int end)
{
    // get weighted average midpoint
    const int n = midpoints.size();
    QPointF avg(0.0, 0.0);
    qreal total_weight = 0.0;
    for (int j = begin; j < end; ++j) {
        int i = j % n;
        avg += weights[i]*midpoints[i];
        total_weight += weights[i];
    }
    avg /= total_weight;

    // project midpoints onto dir line through avg
    for (int j = begin; j < end; ++j) {
        int i = j % n;
        QPointF rel_mid = midpoints[i] - avg;
        qreal dot = dir.x()*rel_mid.x() + dir.y()*rel_mid.y();
        // assumes dir is normalised
        midpoints[i] = avg + dir*dot;
    }
}

// Axis align uses binary fixed point for angles
static const int angle_shift = 20;

// returns true on success
static bool axisAlignGetWays(/* in */ PropertiesDock *theDock,
                             /* out */ QList<Way *> &theWays)
{
    QList<Feature *> selection = theDock->selection();
    // we can't use foreach since we append to selection inside the loop
    for (; !selection.empty(); selection.pop_front()) {
        Feature *F = selection.first();
        if (F->getClass() == "Way")
            theWays << CAST_WAY(F);
        else if (F->getClass() == "Relation")
            // expand relation members onto the end of the selection list
            for (int i = 0; i < F->size(); ++i)
                selection << F->get(i);
    }
    return !theWays.empty();
}

// base the decision purely on what types of features are selected
bool canAxisAlignRoads(PropertiesDock* theDock)
{
    QList<Way *> theWays;
    return axisAlignGetWays(theDock, theWays);
}

// returns true on success
static bool axisAlignPreprocess(/* in */ PropertiesDock *theDock, const Projection &proj, int axes,
                                /* out */ QList<Way *> &theWays, QVector<int> &edge_angles, QVector<int> &edge_axis,
                                          QVector<qreal> &edge_weight, qreal &total_weight, QVector<QPointF> &midpoints)
{
    if (!axisAlignGetWays(theDock, theWays))
        return false;

    // manipulate angles as fixed point values with a unit of the angle between axes
    int nedges = 0;
    foreach (Way *theWay, theWays)
        nedges += theWay->size()-1;
    edge_angles.resize(nedges);
    edge_axis.resize(nedges);
    edge_weight.resize(nedges);
    midpoints.resize(nedges);
    total_weight = 0.0;

    // Look at nodes and find angles etc
    int i = 0;
    foreach (Way *theWay, theWays) {
        Node *n1;
        Node *n2 = theWay->getNode(0);
        QPointF p1;
        QPointF p2 = proj.project(n2);
        for (int j = 0; j < theWay->size()-1; ++j, ++i) {
            n1 = n2;
            n2 = theWay->getNode(j + 1);
            p1 = p2;
            p2 = proj.project(n2);
            if (n1 == n2 || p1 == p2) {
                qWarning() << "ERROR: duplicate nodes found during axis align in" << theWay->id().numId;
                return false;
            }
            midpoints[i] = (p1 + p2) * 0.5;
            // weight towards longer edges rather than lots of smaller edges
            edge_weight[i] = n1->position().distanceFrom(n2->position());
            edge_weight[i] *= edge_weight[i];
            total_weight += edge_weight[i];
            // calculate angle of edge
            qreal ang = QLineF(p1, p2).angle();
            edge_angles[i] = ang * ((axes << angle_shift) / 360.0);
            if (edge_angles[i] < 0) {
                edge_angles[i] += axes<<angle_shift;
            }
            edge_axis[i] = -1;
        }
    }
    return true;
}

// QVectors must be same size
// return true on success
static bool axisAlignCluster(/* in */  const QVector<int> &edge_angles, const QVector<qreal> &edge_weight,
                                       qreal total_weight, unsigned int axes,
                             /* out */ int &theta, QVector<int> &edge_axis)
{
    // Use a variant of K-Means Clustering with regular spaced clusters to find
    // the best angle for the first axis.
    const int nedges = edge_angles.size();
    bool changed;
    theta = 0;
    // This should always terminate pretty quickly, but for robustness it's better to warn than hang.
    unsigned int safety = 100;
    while (--safety) {
        // reassign edges to axes, stopping when nothing changes
        changed = false;
        for (int i = 0; i < nedges; ++i) {
            int angle = edge_angles[i] - theta;
            int new_axis = ((angle >> (angle_shift - 1)) + 1) >> 1;
            if (new_axis >= (int)axes)
                new_axis -= axes;
            else if (new_axis < 0)
                new_axis += axes;
            if (new_axis != edge_axis[i]) {
                edge_axis[i] = new_axis;
                changed = true;
            }
        }
        if (!changed)
            break;
        // adjust angle of first axis by weighted mean of angles between edges and their assigned axes
        qreal dtheta = 0;
        for (int i = 0; i < nedges; ++i) {
            int diff = (edge_angles[i] - theta - (edge_axis[i] << angle_shift)) % (1 << angle_shift);
            if (diff > ((1<<angle_shift)>>1))
                diff -= (1<<angle_shift);
            else if (diff < -((1<<angle_shift)>>1))
                diff += (1<<angle_shift);
            dtheta += edge_weight[i]*diff;
        }
        dtheta /= total_weight;
        theta += dtheta;
    }
    if (!safety)
        qWarning() << "WARNING: align axes clustering loop exceeded safety limit";

    return safety;
}

// QVectors must be same size
static qreal axisAlignCalcVariance(const QVector<int> &edge_angles, const QVector<int> &edge_axis, int theta)
{
    const int nedges = edge_angles.size();
    qreal variance = 0.0;

    for (int i = 0; i < nedges; ++i) {
        int diff = (edge_angles[i] - theta - (edge_axis[i] << angle_shift)) % (1 << angle_shift);
        if (diff > ((1<<angle_shift)>>1))
            diff -= (1<<angle_shift);
        else if (diff < -((1<<angle_shift)>>1))
            diff += (1<<angle_shift);
        qreal diff_f = (double)diff / ((1<<angle_shift)>>1);
        variance += diff_f*diff_f;
    }
    variance /= nedges;
    return variance;
}

// Guesses the number of axes in a shape.
// returns 0 on failure.
unsigned int axisAlignGuessAxes(PropertiesDock* theDock, const Projection &proj, unsigned int max_axes)
{
    QList<Way *> theWays;
    QVector<int> edge_angles;
    QVector<int> edge_axis;
    QVector<qreal> edge_weight;
    QVector<QPointF> midpoints;
    qreal total_weight;
    qreal min_var;
    int min_var_axes = 0;
    for (unsigned int axes = 3; axes <= max_axes; ++axes) {
        if (!axisAlignPreprocess(theDock, proj, axes,
                                 theWays, edge_angles, edge_axis, edge_weight, total_weight, midpoints))
            return 0;
        int theta;
        axisAlignCluster(edge_angles, edge_weight, total_weight, axes, theta, edge_axis);
        qreal var = axisAlignCalcVariance(edge_angles, edge_axis, theta);
        // prefer a lower number of axes
        var *= sqrt((float)axes);
        // we want the number of axes with the lowest weighted variance
        if (!min_var_axes || var < min_var) {
            min_var = var;
            min_var_axes = axes;
        }
    }

    return min_var_axes;
}

// maximum number of iterations
#define AXIS_ALIGN_MAX_ITS          10000
// threshold of (node movement/distance from midpoint)^2
#define AXIS_ALIGN_FAR_THRESHOLD    (1e-6)  // 1/1000th ^2

// don't add theList to history if result isn't success
AxisAlignResult axisAlignRoads(Document* theDocument, CommandList* theList, PropertiesDock* theDock, const Projection &proj, unsigned int axes)
{
    // Make sure nodes aren't pointlessly repeated or angles would be undefined
    removeRepeatsInRoads(theDocument, theList, theDock);

    // manipulate angles as fixed point values with a unit of the angle between axes
    QList<Way *> theWays;
    QVector<int> edge_angles;
    QVector<int> edge_axis;
    QVector<qreal> edge_weight;
    QVector<QPointF> midpoints;
    qreal total_weight;
    if (!axisAlignPreprocess(theDock, proj, axes,
                             theWays, edge_angles, edge_axis, edge_weight, total_weight, midpoints)) {
        // should never happen, repeated nodes already removed
        Q_ASSERT(0);
        theList->undo();
        return AxisAlignFail;
    }

    int theta;
    axisAlignCluster(edge_angles, edge_weight, total_weight, axes, theta, edge_axis);

    // Calculate axis direction vectors
    QVector<QPointF> axis_vectors(axes);
    for (unsigned int i = 0; i < axes; ++i) {
        int angle = theta + (i<<angle_shift);
        axis_vectors[i].setX(cos(M_PI*2/(axes<<angle_shift)*angle));
        axis_vectors[i].setY(-sin(M_PI*2/(axes<<angle_shift)*angle));
    }

    // If adjacent edges are in the same axis we can't intersect them to find
    // where the node should go. Align midpoints of adjacent edges on the same
    // axis so that we can just project the nodes onto the axis and make the
    // result axis aligned. Note that this does not handle adjacent edges which
    // have opposite axes and are therefore parallel (when axes is even).
    int i = 0;
    foreach (Way *theWay, theWays) {
        int start = i;
        int n = theWay->size()-1;
        int last_axis = -1;
        int first_in_seq = -1;
        int excess = 0;
        for (; i < start + n; ++i) {
            int axis = edge_axis[i];
            if (axis != last_axis) {
                if (first_in_seq == -1) {
                    // If edges sharing axis cross the join between ends, we'll
                    // come back to it anyway.
                    excess = i;
                    if (i != 0 || !theWay->isClosed() || edge_axis[start+n-1] != axis)
                        first_in_seq = i;
                } else {
                    if (i - first_in_seq > 1) {
                        axisAlignAlignMidpoints(midpoints, edge_weight, axis_vectors[last_axis], first_in_seq, i);
                    }
                    first_in_seq = i;
                }
                last_axis = axis;
            }
        }
        if (n + excess - first_in_seq > 1)
            axisAlignAlignMidpoints(midpoints, edge_weight, axis_vectors[last_axis], first_in_seq, n+excess);
    }

    // Get the positions of each unique node
    QHash<Node *, QPointF> node_pos;
    bool dups = false;
    foreach (Way *theWay, theWays) {
        for (int i = 0; i < theWay->size(); ++i) {
            Node *N = theWay->getNode(i);
            if (!dups && node_pos.contains(N) && !(theWay->isClosed() && i == theWay->size()-1))
                dups = true;
            node_pos[N] = proj.project(N);
        }
    }

    // If nodes are repeated then they will be moved more than once, so we
    // iterate and allow the nodes to converge to a point.
    qreal last_movement = -1.0;
    qreal movement = 0.0;
    for (int it = 0; ; ++it) {
        i = 0;
        bool moved_far = false;
        // Move nodes (only in node_pos) onto the intersection of the neighbouring
        // edge's axes through their midpoints. Where neighbouring edges are on the
        // same axis (and so there is no intersection), project directly onto the
        // axis through the mean midpoint.
        foreach (Way *theWay, theWays) {
            int index0, index1, max;
            int start = i;
            int n = theWay->size()-1;
            if (theWay->isClosed()) {
                index1 = start+n-1;
                max = n;
            } else {
                index1 = -1;
                max = theWay->size();
            }
            for (int j = 0; j < max; ++j) {
                index0 = index1;
                index1 = ((j < n) ? i++ : -1);
                Node *N = theWay->getNode(j);
                QPointF old_pos = node_pos[N];
                QPointF new_pos;
                QPointF mid;
                if (index0 >= 0 && index1 >= 0 && edge_axis[index0] != edge_axis[index1]) {
                    // axes different, so probably safe to intersect
                    QLineF l0(midpoints[index0], midpoints[index0] + axis_vectors[edge_axis[index0]]);
                    QLineF l1(midpoints[index1], midpoints[index1] + axis_vectors[edge_axis[index1]]);
                    if (l0.intersect(l1, &new_pos) == QLineF::NoIntersection) {
                        theList->undo();
                        return AxisAlignSharpAngles;
                    }

                    if (dups && !moved_far)
                        mid = midpoints[index0];
                } else {
                    // Axes are the same so there's no intersection point. Just
                    // project onto axis through average midpoint.
                    int index = ((index0 >= 0) ? index0 : index1);
                    QPointF midpoint = midpoints[index];
                    if (index != index1 && index1 >= 0)
                        midpoint = (midpoint + midpoints[index1]) / 2;
                    QPointF rel_pos = old_pos - midpoint;
                    QPointF dir = axis_vectors[edge_axis[index]];
                    qreal dot = dir.x()*rel_pos.x() + dir.y()*rel_pos.y();
                    new_pos = midpoint + dir*dot;

                    if (dups && !moved_far)
                        mid = midpoints[index];
                }
                if (dups) {
                    // If we're iterating, only move the node half the distance
                    // towards the new position, so that different edges can
                    // compete with one another to move the node.
                    new_pos = (new_pos + old_pos) / 2;

                    // find how far the node has moved (squared to avoid a sqrt)
                    QPointF delta = new_pos - old_pos;
                    qreal moved_sq = delta.x()*delta.x() + delta.y()*delta.y();
                    movement += moved_sq;

                    // has the node moved "far"?
                    if (!moved_far) {
                        delta = new_pos - mid;
                        qreal size_sq = delta.x()*delta.x() + delta.y()*delta.y();
                        if (moved_sq > size_sq * AXIS_ALIGN_FAR_THRESHOLD)
                            moved_far = true;
                    }
                }
                node_pos[N] = new_pos;
            }
        }

        // We're done when no nodes have moved very far.
        if (!moved_far)
            dups = false;
        if (!dups || it >= AXIS_ALIGN_MAX_ITS)
            break;
        // Every so often check that the movement has decreased since last
        // time. If it hasn't then it's likely that the ways are impossible to
        // align.
        if ((it & 0xf) == 0xf) {
            if (last_movement >= 0.0 && movement >= last_movement)
                break;
            last_movement = movement;
            movement = 0.0;
        }

        // tweak midpoints
        i = 0;
        foreach (Way *theWay, theWays) {
            Node *n2 = theWay->getNode(0);
            QPointF p1;
            QPointF p2 = node_pos[n2];
            for (int j = 0; j < theWay->size()-1; ++j, ++i) {
                n2 = theWay->getNode(j + 1);
                p1 = p2;
                p2 = node_pos[n2];
                midpoints[i] = (p1 + p2) * 0.5;
            }
        }
    }
    // dups is set to false when converged
    if (dups) {
        theList->undo();
        return AxisAlignFail;
    }
    // Commit the changes
    foreach (Way *theWay, theWays) {
        for (int i = 0; i < theWay->size(); ++i) {
            Node *N = theWay->getNode(i);
            theList->add(new MoveNodeCommand(N, proj.inverse2Coord(node_pos[N]), theDocument->getDirtyOrOriginLayer(N->layer())));
        }
    }
    return AxisAlignSuccess;
}
