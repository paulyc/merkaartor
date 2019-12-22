#ifndef MERKAARTOR_TRACKSEGMENTCOMMANDS_H_
#define MERKAARTOR_TRACKSEGMENTCOMMANDS_H_

#include "Command.h"

class TrackSegment;
class TrackNode;
class Layer;

class TrackSegmentAddNodeCommand : public Command
{
    public:
        TrackSegmentAddNodeCommand(TrackSegment* R = nullptr);
        TrackSegmentAddNodeCommand(TrackSegment* R, TrackNode* W, Layer* aLayer=nullptr);
        TrackSegmentAddNodeCommand(TrackSegment* R, TrackNode* W, int Position, Layer* aLayer=nullptr);
        ~TrackSegmentAddNodeCommand(void);

        virtual void undo();
        virtual void redo();
        virtual bool buildDirtyList(DirtyList& theList);

        virtual bool toXML(QXmlStreamWriter& stream) const;
        static TrackSegmentAddNodeCommand* fromXML(Document* d, QXmlStreamReader& stream);

    private:
        Layer* theLayer;
        Layer* oldLayer;
        TrackSegment* theTrackSegment;
        TrackNode* theNode;
        int Position;
};

class TrackSegmentRemoveNodeCommand : public Command
{
    public:
        TrackSegmentRemoveNodeCommand(TrackSegment* R = nullptr);
        TrackSegmentRemoveNodeCommand(TrackSegment* R, TrackNode* W, Layer* aLayer=nullptr);
        TrackSegmentRemoveNodeCommand(TrackSegment* R, int anIdx, Layer* aLayer=nullptr);
        ~TrackSegmentRemoveNodeCommand(void);

        virtual void undo();
        virtual void redo();
        virtual bool buildDirtyList(DirtyList& theList);

        virtual bool toXML(QXmlStreamWriter& stream) const;
        static TrackSegmentRemoveNodeCommand* fromXML(Document* d, QXmlStreamReader& stream);

    private:
        Layer* theLayer;
        Layer* oldLayer;
        int Idx;
        TrackSegment* theTrackSegment;
        TrackNode* theTrackPoint;
};

#endif


