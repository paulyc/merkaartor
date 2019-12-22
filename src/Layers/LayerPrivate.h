#ifndef LAYERPRIVATE_H
#define LAYERPRIVATE_H

class LayerPrivate
{
public:
    LayerPrivate()
    {
        theDocument = nullptr;
        selected = false;
        Enabled = true;
        Readonly = false;
        Uploadable = true;

        IndexingBlocked = false;
        VirtualsUpdatesBlocked = false;
    }
    ~LayerPrivate()
    {
    }

    QList<Feature*> Features;
    QHash<qint64, MapFeaturePtr> IdMap;

    QString Name;
    QString Description;
    bool Visible;
    bool selected;
    bool Enabled;
    bool Readonly;
    bool Uploadable;
    bool IndexingBlocked;
    bool VirtualsUpdatesBlocked;
    qreal alpha;
    int dirtyLevel;

    Document* theDocument;
};

#endif // LAYERPRIVATE_H
