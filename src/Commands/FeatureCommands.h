#ifndef MERKAARTOR_FEATURECOMMANDS_H_
#define MERKAARTOR_FEATURECOMMANDS_H_

#include "Command.h"

#include <QtCore/QString>

#include <utility>
#include <QList>

class Feature;
class Document;
class Layer;

class TagCommand : public Command
{
    public:
        TagCommand(Feature* aF, Layer* aLayer);
        TagCommand(Feature* aF);
        ~TagCommand(void);

        virtual void undo() = 0;
        virtual void redo() = 0;
        virtual bool buildDirtyList(DirtyList& theList);

    protected:
        Feature* theFeature;
        QList<QPair<QString, QString> > Before, After;
        bool FirstRun;
        Layer* theLayer;
        Layer* oldLayer;
};

class SetTagCommand : public TagCommand
{
    public:
        SetTagCommand(Feature* aF);
        SetTagCommand(Feature* aF, int idx, const QString& k, const QString& v, Layer* aLayer=nullptr);
        SetTagCommand(Feature* aF, const QString& k, const QString& v, Layer* aLayer=nullptr);

        virtual void undo();
        virtual void redo();
        virtual bool buildDirtyList(DirtyList& theList);

        virtual bool toXML(QXmlStreamWriter& stream) const;
        static SetTagCommand* fromXML(Document* d, QXmlStreamReader& stream);

    private:
        int theIdx;
        QString theK;
        QString theV;
        QString oldK;
        QString oldV;
};

class ClearTagsCommand : public TagCommand
{
    public:
        ClearTagsCommand(Feature* aF, Layer* aLayer=nullptr);

        virtual void undo();
        virtual void redo();

        virtual bool toXML(QXmlStreamWriter& stream) const;
        static ClearTagsCommand* fromXML(Document* d, QXmlStreamReader& stream);
};

class ClearTagCommand : public TagCommand
{
    public:
        ClearTagCommand(Feature* aF);
        ClearTagCommand(Feature* aF, const QString& k, Layer* aLayer=nullptr);

        virtual void undo();
        virtual void redo();
        virtual bool buildDirtyList(DirtyList& theList);

        virtual bool toXML(QXmlStreamWriter& stream) const;
        static ClearTagCommand* fromXML(Document* d, QXmlStreamReader& stream);

    private:
        int theIdx;
        QString theK;
        QString theV;

};

#endif



