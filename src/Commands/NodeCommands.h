#ifndef MERKATOR_NODECOMMANDS_H_
#define MERKATOR_NODECOMMANDS_H_

#include "Command.h"
#include "Coord.h"

class Node;
class Layer;

class MoveNodeCommand : public Command
{
    public:
        MoveNodeCommand();
        MoveNodeCommand(Node* aPt);
        MoveNodeCommand(Node* aPt, const Coord& aPos, Layer* aLayer=nullptr);
        virtual ~MoveNodeCommand();

        void undo();
        void redo();
        bool buildDirtyList(DirtyList& theList);

        virtual bool toXML(QXmlStreamWriter& stream) const;
        static MoveNodeCommand* fromXML(Document* d,QXmlStreamReader& stream);

    private:
        Layer* theLayer;
        Layer* oldLayer;
        Node* thePoint;
        Coord OldPos, NewPos;
};

#endif


