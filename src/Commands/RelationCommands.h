#ifndef MERKAARTOR_RELATIONCOMMANDS_H_
#define MERKAARTOR_RELATIONCOMMANDS_H_

#include "Command.h"

#include <QString>

class Relation;
class Feature;
class Layer;

class RelationAddFeatureCommand : public Command
{
    public:
        RelationAddFeatureCommand(Relation* R = nullptr);
        RelationAddFeatureCommand(Relation* R, const QString& Role, Feature* W, Layer* aLayer=nullptr);
        RelationAddFeatureCommand(Relation* R, const QString& Role, Feature* W, int Position, Layer* aLayer=nullptr);
        ~RelationAddFeatureCommand(void);

        virtual void undo();
        virtual void redo();
        virtual bool buildDirtyList(DirtyList& theList);

        virtual bool toXML(QXmlStreamWriter& stream) const;
        static RelationAddFeatureCommand* fromXML(Document* d, QXmlStreamReader& stream);

    private:
        Layer* theLayer;
        Layer* oldLayer;
        Relation* theRelation;
        QString Role;
        Feature* theMapFeature;
        int Position;
};

class RelationRemoveFeatureCommand : public Command
{
    public:
        RelationRemoveFeatureCommand(Relation* R = nullptr);
        RelationRemoveFeatureCommand(Relation* R, Feature* W, Layer* aLayer=nullptr);
        RelationRemoveFeatureCommand(Relation* R, int anIdx, Layer* aLayer=nullptr);
        ~RelationRemoveFeatureCommand(void);

        virtual void undo();
        virtual void redo();
        virtual bool buildDirtyList(DirtyList& theList);

        virtual bool toXML(QXmlStreamWriter& stream) const;
        static RelationRemoveFeatureCommand* fromXML(Document* d, QXmlStreamReader& stream);

    private:
        Layer* theLayer;
        Layer* oldLayer;
        int Idx;
        Relation* theRelation;
        QString Role;
        Feature* theMapFeature;
};



#endif


