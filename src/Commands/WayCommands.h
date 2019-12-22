#ifndef MERKAARTOR_WAYOMMANDS_H_
#define MERKAARTOR_WAYOMMANDS_H_

#include "Command.h"

class Way;
class Node;
class Layer;

class WayAddNodeCommand : public Command
{
    public:
        WayAddNodeCommand(Way* R = nullptr);
        WayAddNodeCommand(Way* R, Node* W, Layer* aLayer=nullptr);
        WayAddNodeCommand(Way* R, Node* W, int Position, Layer* aLayer=nullptr);
        ~WayAddNodeCommand(void);

        virtual void undo();
        virtual void redo();
        virtual bool buildDirtyList(DirtyList& theList);

        virtual bool toXML(QXmlStreamWriter& stream) const;
        static WayAddNodeCommand* fromXML(Document* d, QXmlStreamReader& stream);

    private:
        Layer* theLayer;
        Layer* oldLayer;
        Way* theRoad;
        Node* theTrackPoint;
        int Position;
};

class WayRemoveNodeCommand : public Command
{
    public:
        WayRemoveNodeCommand(Way* R = nullptr);
        WayRemoveNodeCommand(Way* R, Node* W, Layer* aLayer=nullptr);
        WayRemoveNodeCommand(Way* R, int anIdx, Layer* aLayer=nullptr);
        ~WayRemoveNodeCommand(void);

        virtual void undo();
        virtual void redo();
        virtual bool buildDirtyList(DirtyList& theList);

        virtual bool toXML(QXmlStreamWriter& stream) const;
        static WayRemoveNodeCommand* fromXML(Document* d, QXmlStreamReader& stream);

    private:
        Layer* theLayer;
        Layer* oldLayer;
        int Idx;
        bool wasClosed;
        Way* theRoad;
        Node* theNode;
};

#endif


