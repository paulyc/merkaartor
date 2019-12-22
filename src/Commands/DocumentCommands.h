#ifndef MERKATOR_DOCUMENTCOMMANDS_H_
#define MERKATOR_DOCUMENTCOMMANDS_H_

#include "Command.h"

#include <QList>

class Document;
class Layer;
class Feature;

class AddFeatureCommand : public Command
{
    public:
        AddFeatureCommand(Feature* aFeature = nullptr);
        AddFeatureCommand(Layer* aDocument, Feature* aFeature, bool aUserAdded);
        virtual ~AddFeatureCommand();

        void undo();
        void redo();
        bool buildDirtyList(DirtyList& theList);

        virtual bool toXML(QXmlStreamWriter& stream) const;
        static AddFeatureCommand* fromXML(Document* d, QXmlStreamReader& stream);

    private:
        Layer* theLayer;
        Layer* oldLayer;
        Feature* theFeature;
        bool UserAdded;
};

class RemoveFeatureCommand : public Command
{
    public:
        RemoveFeatureCommand(Feature* aFeature = nullptr);
        RemoveFeatureCommand(Document* theDocument, Feature* aFeature);
        RemoveFeatureCommand(Document* theDocument, Feature* aFeature, const QList<Feature*>& Alternatives);
        virtual ~RemoveFeatureCommand();

        void undo();
        void redo();
        bool buildDirtyList(DirtyList& theList);

        virtual bool toXML(QXmlStreamWriter& stream) const;
        static RemoveFeatureCommand* fromXML(Document* d, QXmlStreamReader& stream);

    private:
        Layer* theLayer;
        Layer* oldLayer;
        Feature* theFeature;
        CommandList* CascadedCleanUp;
        bool RemoveExecuted;
        QList<Feature*> theAlternatives;
};

#endif


