#include "Global.h"

#include "MainWindow.h"
#include "Features.h"
#include "Command.h"
#include "DocumentCommands.h"
#include "FeatureCommands.h"
#include "RelationCommands.h"
#include "WayCommands.h"
#include "NodeCommands.h"
#include "Document.h"
#include "Layer.h"
#include "MasPaintStyle.h"
#include "TagSelector.h"
#include "MapView.h"
#include "PropertiesDock.h"

#include "Utils.h"

#include <QApplication>
#include <QUuid>
#include <QProgressDialog>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

qint64 g_feat_rndId = 0;
QStringList TechnicalTags = QString(TECHNICAL_TAGS).split("#");

IFeature::FId Feature::newId(IFeature::FeatureType type) const
{
    IFeature::FId id = IFeature::FId(type, --g_feat_rndId);
    return id;
}

//static QString randomId()
//{
//	QUuid uuid = QUuid::createUuid();
//#ifdef _MOBILE
//	return uuid.toString();
//#else
//	// This is fairly horrible, but it's also around 10 times faster than QUuid::toString()
//	// and randomId() is called a lot during large imports
//
//	// Lookup table of hex value-pairs representing a byte
//	static char hex[(2*256)+1] = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";
//
//	char buffer[39] = "{________-____-____-____-____________}";
//
//
//
//	// {__%08x__-____-____-____-____________}
//	memcpy(&buffer[ 1], &hex[((uuid.data1 >> 24) & 0xFF)], 2);
//	memcpy(&buffer[ 3], &hex[((uuid.data1 >> 16) & 0xFF)], 2);
//	memcpy(&buffer[ 5], &hex[((uuid.data1 >>  8) & 0xFF)], 2);
//	memcpy(&buffer[ 7], &hex[((uuid.data1 >>  0) & 0xFF)], 2);
//
//	// {________-%04x-____-____-____________}
//	memcpy(&buffer[10], &hex[((uuid.data2 >>  8) & 0xFF)], 2);
//	memcpy(&buffer[12], &hex[((uuid.data2 >>  0) & 0xFF)], 2);
//
//	// {________-____-%04x-____-____________}
//	memcpy(&buffer[15], &hex[((uuid.data3 >>  8) & 0xFF)], 2);
//	memcpy(&buffer[17], &hex[((uuid.data3 >>  0) & 0xFF)], 2);
//
//	// {________-____-____-%04x-____________}
//	memcpy(&buffer[20], &hex[uuid.data4[0]], 2);
//	memcpy(&buffer[22], &hex[uuid.data4[1]], 2);
//
//	// {________-____-____-____-___%012x____}
//	for (int i=0; i<6; i++) {
//		memcpy(&buffer[25+(i*2)], &hex[uuid.data4[i+2]], 2);
//	}
//
//	return QString::fromLatin1(buffer,38);
//#endif
//}

void copyTags(Feature* Dest, Feature* Src)
{
    for (int i=0; i<Src->tagSize(); ++i)
        Dest->setTag(Src->tagKey(i),Src->tagValue(i));
}


class FeaturePrivate
{
public:
    FeaturePrivate(Feature* aFeature)
        :  LastActor(Feature::User)
        , PossiblePaintersUpToDate(false)
        , PixelPerMForPainter(-1), CurrentPainter(0), HasPainter(false)
        , theFeature(aFeature), LastPartNotification(0)
        , Deleted(false), Visible(true), Uploaded(false), FilterRevision(-1)
        , Virtual(false), Special(false), DirtyLevel(0)
        , parentLayer(0)
    #ifndef FRISIUS_BUILD
        , Time(QDateTime::currentDateTime().toTime_t()), User(0xffffffff)
    #endif
    {
#ifndef FRISIUS_BUILD
        initVersionNumber();
        //            qDebug() << "MapFeaturePrivate size: " << sizeof(FeaturePrivate) << sizeof(IFeature::FId) << sizeof(RenderPriority);
#endif
    }
    FeaturePrivate(const FeaturePrivate& other)
        : Tags(other.Tags), LastActor(other.LastActor)
        , PossiblePaintersUpToDate(false)
        , PixelPerMForPainter(-1), CurrentPainter(0), HasPainter(false)
        , theFeature(nullptr), LastPartNotification(0)
        , Deleted(false), Visible(true), Uploaded(false), FilterRevision(-1)
        , Virtual(other.Virtual), Special(other.Special), DirtyLevel(0)
        , parentLayer(0)
    #ifndef FRISIUS_BUILD
        , Time(other.Time), User(other.User)
    #endif
    {
#ifndef FRISIUS_BUILD
        initVersionNumber();
#endif
    }

    void updatePossiblePainters();
    void blankPainters();
    void updatePainters(qreal PixelPerM);
#ifndef FRISIUS_BUILD
    void initVersionNumber()
    {
        //    static int VN = -1;
        //    VersionNumber = VN--;
        VersionNumber = 0;
    }
#endif

    mutable IFeature::FId Id; // 9 (16)
    QList<QPair<quint32, quint32> > Tags; // 4
    Feature::ActorType LastActor; // 4
    QList<const FeaturePainter*> PossiblePainters; // 4
    bool PossiblePaintersUpToDate; // 1
    qreal PixelPerMForPainter; // 8
    const FeaturePainter* CurrentPainter; // 4
    bool HasPainter; // 1
    Feature* theFeature; // 4
    QList<Feature*> Parents; // 4
    int LastPartNotification; // 4
#ifndef FRISIUS_BUILD
    uint Time; // 4
    quint32 User; // 4
    int VersionNumber; // 4
#endif
    bool Deleted; // 1
    bool Visible; // 1
    bool Uploaded; // 1
    int FilterRevision; // 4
    bool Virtual; // 1
    bool Special; // 1
    int DirtyLevel; // 4
    QList<FilterLayer*> FilterLayers; // 4
    qreal Alpha; // 8
    Layer* parentLayer; // 4
};

Feature::Feature()
: MetaUpToDate(false), ReadOnly(false)
{
    p = new FeaturePrivate(this);
    p->Id = IFeature::FId(IFeature::Uninitialized, 0);

    //     qDebug() << "Feature size: " << sizeof(Feature);
}

Feature::Feature(const Feature& other)
: IFeature(other), MetaUpToDate(false), ReadOnly(other.ReadOnly)
{
    p = new FeaturePrivate(*other.p);
    p->Id = IFeature::FId(IFeature::Uninitialized, 0);
    p->theFeature = this;
}

Feature::~Feature(void)
{
    // TODO Those cleanup shouldn't be necessary and lead to crashes
    //      Check for side effect of supressing them.
//    while (sizeParents())
//        getParent(0)->remove(this);
    delete p;
}

void Feature::setLayer(Layer* aLayer)
{
    p->parentLayer = aLayer;
}

Layer* Feature::layer() const
{
    return p->parentLayer;
}

void Feature::setLastUpdated(Feature::ActorType A)
{
    p->LastActor = A;
}

Feature::ActorType Feature::lastUpdated() const
{
    Layer* L = p->parentLayer;
    if (L && L->classType() == Layer::DirtyLayerType)
        return Feature::User;
    else
        return p->LastActor;
}

QString Feature::stripToOSMId(const IFeature::FId& id)
{
    return QString::number(id.numId);
}

void Feature::setId(const IFeature::FId& id)
{
    if (id == p->Id)
        return;

    if (p->parentLayer)
    {
        if (p->Id.type != IFeature::Uninitialized)
            p->parentLayer->notifyIdUpdate(p->Id,0);
        if (id.type != IFeature::Uninitialized)
            p->parentLayer->notifyIdUpdate(id,this);
    }
    p->Id = id;

    // Make sure any new id is unique
    if (id.numId < g_feat_rndId)
        g_feat_rndId = id.numId;
}

const IFeature::FId& Feature::resetId() const
{
    p->Id = newId((IFeature::FeatureType)getType());
    return p->Id;
}

const IFeature::FId& Feature::id() const
{
    if (p->Id.type == IFeature::Uninitialized)
        resetId();

    return p->Id;
}

QString Feature::xmlId() const
{
    return stripToOSMId(id());
}

bool Feature::hasOSMId() const
{
    return (p->Id.numId >= 0);
}

#ifndef FRISIUS_BUILD

const QDateTime Feature::time() const
{
    return QDateTime::fromTime_t(p->Time);
}

void Feature::setTime(const QDateTime& time)
{
    p->Time = time.toTime_t();
}

void Feature::setTime(uint epoch)
{
    p->Time = epoch;
}

const QString& Feature::user() const
{
    return g_getUser(p->User);
}

void Feature::setUser(const QString& user)
{
    p->User = g_setUser(user);
}

void Feature::setVersionNumber(int vn)
{
    p->VersionNumber = vn;
}

int Feature::versionNumber() const
{
    return p->VersionNumber;
}


#endif

int Feature::incDirtyLevel(int inc)
{
    return p->DirtyLevel += inc;
}

int Feature::decDirtyLevel(int inc)
{
    return p->DirtyLevel -= inc;
}

int Feature::setDirtyLevel(int newLevel)
{
    return (p->DirtyLevel = newLevel);
}

int Feature::getDirtyLevel() const
{
    return p->DirtyLevel;
}

qreal Feature::getAlpha()
{
    if (!MetaUpToDate)
        updateMeta();
    return p->Alpha;
}

bool Feature::isDirty() const
{
    if (g_Merk_Frisius)
        return (p->DirtyLevel > 0);

    if (p->parentLayer)
        return (p->parentLayer->classType() == Layer::DirtyLayerType);
    else
        return false;
}

void Feature::setUploaded(bool state)
{
    p->Uploaded = state;
}

bool Feature::isUploaded() const
{
    return p->Uploaded;
}

bool Feature::isUploadable() const
{
    if (p->parentLayer)
        return (p->parentLayer->isUploadable());
    else
        return false;
}

void Feature::setReadonly(bool val)
{
    ReadOnly = val;
}

bool Feature::isReadonly()
{
    if (!MetaUpToDate)
        updateMeta();
    return ReadOnly;
}

void Feature::setDeleted(bool delState)
{
    if (delState == p->Deleted)
        return;
    p->Deleted = delState;
    g_backend.sync(this);
}

bool Feature::isDeleted() const
{
    return p->Deleted;
}

void Feature::setVisible(bool state)
{
    if (state == p->Visible)
        return;
    p->Visible = state;
}

bool Feature::isVisible()
{
    if (!MetaUpToDate)
        updateMeta();
    return p->Visible;
}

bool Feature::isHidden()
{
    if (!MetaUpToDate)
        updateMeta();
    return !p->Visible;
}

void Feature::setVirtual(bool val)
{
    if (val == p->Virtual)
        return;
    p->Virtual = val;
    if (!p->Virtual) {
        resetId();
    }
    g_backend.sync(this);
}

bool Feature::isVirtual() const
{
    return p->Virtual;
}

void Feature::setSpecial(bool val)
{
    p->Special = val;
}

void Feature::buildPath(const Projection &)
{
}

bool Feature::isSpecial() const
{
    return p->Special;
}

void Feature::setTag(int index, const QString& key, const QString& value)
{
    if (key.toLower() == "created_by")
        return;

    QPair<quint32, quint32> pi = g_addToTagList(key, value);

    int i = 0;
    for (; i<p->Tags.size(); ++i)
        if (p->Tags[i].first == pi.first)
        {
            if (p->Tags[i].second == pi.second)
                return;
            g_removeFromTagList(p->Tags[i].first, p->Tags[i].second);
            p->Tags[i].second = pi.second;
            break;
        }
    if (i == p->Tags.size()) {
        p->Tags.insert(p->Tags.begin() + index, pi);
    }
    invalidatePainter();
    invalidateMeta();
}

void Feature::setTag(const QString& key, const QString& value)
{
    if (key.toLower() == "created_by")
        return;

    QPair<quint32, quint32> pi = g_addToTagList(key, value);

    int i = 0;
    for (; i<p->Tags.size(); ++i)
        if (p->Tags[i].first == pi.first)
        {
            if (p->Tags[i].second == pi.second)
                return;
            g_removeFromTagList(p->Tags[i].first, p->Tags[i].second);
            p->Tags[i].second = pi.second;
            break;
        }
    if (i == p->Tags.size()) {
        p->Tags.push_back(pi);
    }
    invalidateMeta();
    invalidatePainter();
}

void Feature::clearTags()
{
    while (p->Tags.size()) {
        g_removeFromTagList(p->Tags[0].first, p->Tags[0].second);
        p->Tags.erase(p->Tags.begin());
    }
    invalidateMeta();
    invalidatePainter();
}

void Feature::clearTag(const QString& k)
{
    quint32 ik = g_getTagKeyIndex(k);

    for (int i=0; i<p->Tags.size(); ++i)
        if (p->Tags[i].first == ik)
        {
            g_removeFromTagList(p->Tags[i].first, p->Tags[i].second);
            p->Tags.erase(p->Tags.begin()+i);
            break;
        }
    invalidateMeta();
    invalidatePainter();
}

void Feature::removeTag(int idx)
{
    g_removeFromTagList(p->Tags[idx].first, p->Tags[idx].second);
    p->Tags.erase(p->Tags.begin()+idx);
    invalidateMeta();
    invalidatePainter();
}

int Feature::tagSize() const
{
    return p->Tags.size();
}

QString Feature::tagValue(int i) const
{
    return g_getTagValue(p->Tags[i].second);
}

QString Feature::tagKey(int i) const
{
    return g_getTagKey(p->Tags[i].first);
}

int Feature::findKey(const QString &k) const
{
    for (int i=0; i<p->Tags.size(); ++i)
        if (tagKey(i) == k)
            return i;
    return -1;
}

QString Feature::tagValue(const QString& k, const QString& Default) const
{
    for (int i=0; i<p->Tags.size(); ++i)
        if (tagKey(i) == k)
            return tagValue(i);
    return Default;
}

void Feature::invalidateMeta()
{
    MetaUpToDate = false;
}

void Feature::invalidatePainter()
{
    p->PossiblePaintersUpToDate = false;
    p->PixelPerMForPainter = -1;
}

static QPainterPath painterPath;

const QPainterPath& Feature::getPath() const
{
    return painterPath;
}

void FeaturePrivate::updatePossiblePainters()
{
    QMutexLocker mutlock(&theFeature->featMutex);

    //still match features with no tags and no parent, i.e. "lost" trackpoints
    if ( (theFeature->layer()->isTrack()) && M_PREFS->getDisableStyleForTracks() ) return blankPainters();

    if ( (theFeature->layer()->isTrack()) || theFeature->sizeParents() ) {
        if (CHECK_NODE(theFeature) && !STATIC_CAST_NODE(theFeature)->isPOI()) return blankPainters();
        if (!theFeature->tagSize()) return blankPainters();
    }

    PossiblePainters.clear();
    QList<const FeaturePainter*> DefaultPainters;
    for (int i=0; i<theFeature->layer()->getDocument()->getPaintersSize(); ++i)
    {
        const FeaturePainter* Current = dynamic_cast<const FeaturePainter*>(theFeature->layer()->getDocument()->getPainter(i));
        switch (Current->matchesTag(theFeature,nullptr)) {
        case TagSelect_Match:
            PossiblePainters.push_back(Current);
            break;
        case TagSelect_DefaultMatch:
            DefaultPainters.push_back(Current);
            break;
        default:
            break;
        }
    }
    if (!PossiblePainters.size())
        PossiblePainters = DefaultPainters;
    PossiblePaintersUpToDate = true;
    HasPainter = (PossiblePainters.size() > 0);
}

void FeaturePrivate::updatePainters(qreal PixelPerM)
{
    if (!PossiblePaintersUpToDate)
        updatePossiblePainters();

    QMutexLocker mutlock(&theFeature->featMutex);
    CurrentPainter = nullptr;
    PixelPerMForPainter = PixelPerM;
    for (int i=0; i<PossiblePainters.size(); ++i)
        if (PossiblePainters[i]->matchesZoom(PixelPerM))
        {
            CurrentPainter = PossiblePainters[i];
            return;
        }
}

void FeaturePrivate::blankPainters()
{
    CurrentPainter = nullptr;
    PossiblePainters.clear();
    PossiblePaintersUpToDate = true;
    HasPainter = false;
}

const FeaturePainter* Feature::getPainter(qreal PixelPerM) const
{
    if (p->PixelPerMForPainter != PixelPerM)
        p->updatePainters(PixelPerM);
    return p->CurrentPainter;
}

const FeaturePainter* Feature::getCurrentPainter() const
{
    if (p->CurrentPainter)
        return p->CurrentPainter;
    else {
        if (p->PossiblePainters.size())
            return p->PossiblePainters[0];
        else return nullptr;
    }
}

bool Feature::hasPainter() const
{
    if (!p->PossiblePaintersUpToDate)
        p->updatePossiblePainters();

    return p->HasPainter;
}

bool Feature::hasPainter(qreal PixelPerM) const
{
    if (!layer())
        return false;
    if (p->PixelPerMForPainter != PixelPerM)
        p->updatePainters(PixelPerM);
    return (p->CurrentPainter != nullptr);
}

void Feature::setParentFeature(Feature* F)
{
    if (std::find(p->Parents.begin(),p->Parents.end(),F) == p->Parents.end())
        p->Parents.push_back(F);
}

void Feature::unsetParentFeature(Feature* F)
{
    for (int i=0; i<p->Parents.size(); ++i)
        if (p->Parents[i] == F)
        {
            p->Parents.erase(p->Parents.begin()+i);
            return;
        }
}

void Feature::updateFilters()
{
    p->FilterLayers.clear();

    Layer* L = layer();
    if (!L)
        return;

    Document* D = L->getDocument();
    if (!D)
        return;

    for (int i=0; i<D->layerSize(); ++i) {
        if (D->getLayer(i)->classType() == Layer::FilterLayerType) {
            FilterLayer* Fl = dynamic_cast<FilterLayer*>(D->getLayer(i));
            if (!Fl->isEnabled() || !Fl->selector())
                continue;
            if (Fl->selector()->matches(this, 0) != TagSelect_NoMatch)
                p->FilterLayers << Fl;
        }
    }
    invalidateMeta();
}

void Feature::updateMeta()
{
    updateFilters();

    Layer* L = layer();
    if (!L)
        return;

    if (!L->isVisible())
        p->Visible = false;
    else {
        p->Visible = true;
        foreach(FilterLayer* Fl, p->FilterLayers) {
            if (!Fl->isVisible()) {
                p->Visible = false;
                break;
            }
        }
    }

    if (L->getAlpha() != 1.0)
        p->Alpha = L->getAlpha();
    else {
        p->Alpha = 1.0;
        foreach(FilterLayer* Fl, p->FilterLayers) {
            if (Fl->getAlpha() != 1) {
                p->Alpha = Fl->getAlpha();
                break;
            }
        }
    }

    if (L->isReadonly())
        ReadOnly = true;
    else {
        ReadOnly = false;
        foreach(FilterLayer* Fl, p->FilterLayers) {
            if (Fl->isReadonly()) {
                ReadOnly = true;
                break;
            }

        }
    }
}

int Feature::sizeParents() const
{
    return p->Parents.size();
}

IFeature* Feature::getParent(int i)
{
    return p->Parents[i];
}

const IFeature* Feature::getParent(int i) const
{
    return p->Parents[i];
}

void Feature::notifyChanges()
{
    static int Id = 0;
    notifyParents(++Id);
}

void Feature::notifyParents(int Id)
{
    if (Id != p->LastPartNotification)
    {
        p->LastPartNotification = Id;
        for (int i=0; i<p->Parents.size(); ++i)
            p->Parents[i]->partChanged(this, Id);
    }
}


void Feature::drawHover(QPainter& thePainter, MapView* theView)
{
    QPen TP(M_PREFS->getHoverColor(),M_PREFS->getHoverWidth(),Qt::SolidLine);
    thePainter.setPen(TP);

    drawSpecial(thePainter, TP, theView);

    drawChildrenSpecial(thePainter, TP, theView, 1);

    if (M_PREFS->getShowParents()) {
        TP.setDashPattern(M_PREFS->getParentDashes());
        thePainter.setPen(TP);
        drawParentsSpecial(thePainter, TP, theView);
    }
}

void Feature::drawFocus(QPainter& thePainter, MapView* theView)
{
    QPen TP(M_PREFS->getFocusColor(),M_PREFS->getFocusWidth(),Qt::SolidLine);
    thePainter.setPen(TP);

    drawSpecial(thePainter, TP, theView);

    drawChildrenSpecial(thePainter, TP, theView, 1);

    if (M_PREFS->getShowParents()) {
        TP.setDashPattern(M_PREFS->getParentDashes());
        thePainter.setPen(TP);
        drawParentsSpecial(thePainter, TP, theView);
    }
}

void Feature::drawHighlight(QPainter& thePainter, MapView* theView)
{
    QPen TP(M_PREFS->getHighlightColor(),M_PREFS->getHighlightWidth(),Qt::SolidLine);
    thePainter.setPen(TP);

    drawSpecial(thePainter, TP, theView);

    drawChildrenSpecial(thePainter, TP, theView, 1);

    if (M_PREFS->getShowParents()) {
        TP.setDashPattern(M_PREFS->getParentDashes());
        thePainter.setPen(TP);
        drawParentsSpecial(thePainter, TP, theView);
    }
}

QString Feature::toXML(int lvl, QProgressDialog * progress)
{
    QString xml;
    QXmlStreamWriter stream(&xml);
    stream.setAutoFormatting(true);
    stream.setAutoFormattingIndent(lvl*2);

    stream.writeStartDocument();
    stream.writeStartElement("root");
    if (toXML(stream, progress)) {
        stream.writeEndElement();
        stream.writeEndDocument();
        return xml;
    } else
        return QString();
}

void Feature::fromXML(QXmlStreamReader& stream, Feature* F)
{
    bool Deleted = (stream.attributes().value("deleted") == "true");
    int Dirty = (stream.attributes().hasAttribute("dirtylevel") ? stream.attributes().value("dirtylevel").toString().toInt() : 0);
    bool Uploaded = (stream.attributes().value("uploaded") == "true");
    bool Special = (stream.attributes().value("special") == "true");
//    bool Selected = (stream.attributes().value("selected") == "true");

    QDateTime time;
    time = QDateTime::fromString(stream.attributes().value("timestamp").toString().left(19), Qt::ISODate);
    QString user = stream.attributes().value("user").toString();
    int Version = stream.attributes().value("version").toString().toInt();
    if (Version < 1)
        Version = 0;
    Feature::ActorType A = (Feature::ActorType)(stream.attributes().hasAttribute("actor") ? stream.attributes().value("actor").toString().toInt() : 2);

    F->setLastUpdated(A);
    F->setDeleted(Deleted);
    F->setDirtyLevel(Dirty);
    F->setUploaded(Uploaded);
    F->setSpecial(Special);
#ifndef FRISIUS_BUILD
    F->setTime(time);
    F->setUser(user);
    F->setVersionNumber(Version);
#endif

    // TODO Manage selection at document level
//    if(Selected)
//        g_Merk_MainWindow->properties()->addSelection(F);
}

void Feature::toXML(QXmlStreamWriter& stream, bool strict, QString changetsetid)
{
    stream.writeAttribute("id", xmlId());
#ifndef FRISIUS_BUILD
    stream.writeAttribute("timestamp", time().toString(Qt::ISODate)+"Z");
    stream.writeAttribute("version", QString::number(versionNumber()));
    stream.writeAttribute("user", user());
#endif
    if (!changetsetid.isEmpty())
        stream.writeAttribute("changeset", changetsetid);
    if (!strict) {
        stream.writeAttribute("actor", QString::number((int)lastUpdated()));
        if (isDeleted())
            stream.writeAttribute("deleted","true");
        if (getDirtyLevel())
            stream.writeAttribute("dirtylevel", QString::number(getDirtyLevel()));
        if (isUploaded())
            stream.writeAttribute("uploaded","true");
        if (isSpecial())
            stream.writeAttribute("special","true");
        // TODO Manage selection at document level
#ifndef _MOBILE
        if (g_Merk_MainWindow && g_Merk_MainWindow->properties()->isSelected(this))
            stream.writeAttribute("selected","true");
#endif
    }
}

bool Feature::tagsToXML(QXmlStreamWriter& stream, bool strict)
{
    bool OK = true;

    for (int i=0; i<tagSize(); ++i)
    {
        if (strict) {
            if (tagKey(i).startsWith('_') && (tagKey(i).endsWith('_')))
                continue;
        }

        stream.writeStartElement("tag");
        stream.writeAttribute("k", tagKey(i));
        stream.writeAttribute("v", tagValue(i));
        stream.writeEndElement();
    }

    return OK;
}

void Feature::tagsFromXML(Document* d, Feature * f, QXmlStreamReader& stream)
{
    Q_UNUSED(d)
    while(!stream.atEnd() && !stream.isEndElement()) {
        if (stream.name() == "tag") {
            f->setTag(stream.attributes().value("k").toString(), stream.attributes().value("v").toString());
            stream.readNext();
        }
        stream.readNext();
    }
}

Relation * Feature::GetSingleParentRelation(Feature * mapFeature)
{
    int parents = mapFeature->sizeParents();

    if (parents == 0)
        return nullptr;

    Relation * parentRelation = nullptr;

    int i;
    for (i=0; i<parents; i++)
    {
        Relation * rel = dynamic_cast<Relation*>(mapFeature->getParent(i));
        if (!rel || rel->isDeleted()) continue;

        if (parentRelation)
            return nullptr;

        if (rel->layer()->isEnabled())
            parentRelation = rel;
    }

    return parentRelation;
}

//Static
Node* Feature::getNodeOrCreatePlaceHolder(Document *theDocument, Layer *theLayer, const IFeature::FId& Id)
{
    Node* Part = CAST_NODE(theDocument->getFeature(Id));
    if (!Part)
    {
        if (!theLayer)
            theLayer = theDocument->getDirtyOrOriginLayer();
        Part = g_backend.allocNode(theLayer, Coord(0,0));
        Part->setId(Id);
        Part->setLastUpdated(Feature::NotYetDownloaded);
        theLayer->add(Part);
    }
    return Part;
}

TrackNode* Feature::getTrackNodeOrCreatePlaceHolder(Document *theDocument, Layer *theLayer, const IFeature::FId& Id)
{
    TrackNode* Part = CAST_TRACKNODE(theDocument->getFeature(Id));
    if (!Part)
    {
        if (!theLayer)
            theLayer = theDocument->getDirtyOrOriginLayer();
        Part = g_backend.allocTrackNode(theLayer, Coord(0,0));
        Part->setId(Id);
        Part->setLastUpdated(Feature::NotYetDownloaded);
        theLayer->add(Part);
    }
    return Part;
}

Way* Feature::getWayOrCreatePlaceHolder(Document *theDocument, Layer *theLayer, const IFeature::FId& Id)
{
    Way* Part = CAST_WAY(theDocument->getFeature(Id));
    if (!Part)
    {
        if (!theLayer)
            theLayer = theDocument->getDirtyOrOriginLayer();
        Part = g_backend.allocWay(theLayer);
        Part->setId(Id);
        Part->setLastUpdated(Feature::NotYetDownloaded);
        theLayer->add(Part);

    }
    return Part;
}

Relation* Feature::getRelationOrCreatePlaceHolder(Document *theDocument, Layer *theLayer, const IFeature::FId& Id)
{
    Relation* Part = CAST_RELATION(theDocument->getFeature(Id));
    if (!Part)
    {
        if (!theLayer)
            theLayer = theDocument->getDirtyOrOriginLayer();
        Part = g_backend.allocRelation(theLayer);
        Part->setId(Id);
        Part->setLastUpdated(Feature::NotYetDownloaded);
        theLayer->add(Part);
    }
    return Part;
}

void Feature::mergeTags(Document* theDocument, CommandList* L, Feature* Dest, Feature* Src)
{
    for (int i=0; i<Src->tagSize(); ++i)
    {
        QString k = Src->tagKey(i);
        QString v1 = Src->tagValue(i);
        int j = Dest->findKey(k);
        if (j == -1)
            L->add(new SetTagCommand(Dest,k,v1, theDocument->getDirtyOrOriginLayer(Dest->layer())));
        else
        {
            QString v2 = Dest->tagValue(j);
            if (v1 != v2)
            {
                L->add(new SetTagCommand(Dest,k,QString("%1;%2").arg(v2).arg(v1), theDocument->getDirtyOrOriginLayer(Dest->layer())));
            }
        }
    }
}


QString Feature::toMainHtml(QString type, QString systemtype)
{
    QString desc;
    QString name(tagValue("name",""));
    if (!name.isEmpty())
        desc = QString("<big><b>%1</b></big><br/><small>(%2)</small>").arg(name).arg(id().numId);
    else
        desc = QString("<big><b>%1</b></big>").arg(id().numId);

    QString S =
    "<html><head/><body>"
    "<small><i>" + type + "</i></small><br/>"
    + desc +
    "<br/>"
    "<i>"
    "<small>";
#ifndef FRISIUS_BUILD
    S += QApplication::translate("MapFeature", "<i>V: </i><b>%1</b> ").arg(QString::number(versionNumber()));
    if (!user().isEmpty())
        S += QApplication::translate("MapFeature", "<i>last: </i><b>%1</b> by <b>%2</b>").arg(time().toString(Qt::SystemLocaleDate)).arg(user());
    else
        S += QApplication::translate("MapFeature", "<i>last: </i><b>%1</b>").arg(time().toString(Qt::SystemLocaleDate));
#endif
    if (layer())
        S += QApplication::translate("MapFeature", "<br/><i>layer: </i><b>%1</b> ").arg(layer()->name());
    S += "</small>"
    "<hr/>"
    "%1";

    if (tagSize()) {
        QStringList sTags;
        int t=0;
        for (int i=0; i<tagSize(); ++i) {
            if (tagKey(i) != "_description_" && tagKey(i) != "_comment_") {
                ++t;
                sTags << tagKey(i) + "&nbsp;=&nbsp;" + "<b>" + tagValue(i) + "</b>";
            }
        }

        if (t) {
            S += "<hr/><small>";
            S += sTags.join("<br/>");
            S += "</small>";
        }
    }

    if (hasOSMId()) {
        S += "<hr/>"
        "<a href='/browse/" + systemtype + "/" + xmlId() + "'>"+QApplication::translate("MapFeature", "Browse")+"</a>";
    }
    S += "</body></html>";

    return S;
}

void Feature::getLock()
{
	featMutex.lock();
}

void Feature::releaseLock()
{
	featMutex.unlock();
}
