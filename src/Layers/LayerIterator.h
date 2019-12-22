class IDocument;

template <class L>
class LayerIterator
{

public:
    LayerIterator(IDocument* aDoc)
        : theDocument(aDoc), curLayerIdx(0), isAtEnd(false)
    {
        docSize = theDocument->layerSize();

        if(!check() && !isAtEnd)
            ++(*this);
    }
    virtual ~LayerIterator() {}

    bool isEnd() const
    {
        return isAtEnd;
    }

    LayerIterator& operator ++()
    {
        docSize = theDocument->layerSize();

        if (curLayerIdx < docSize-1) {
            curLayerIdx++;
        } else
            isAtEnd = true;
        while(!isAtEnd && !check()) {
            if (curLayerIdx < docSize-1) {
                curLayerIdx++;
            } else
                isAtEnd = true;
        }

        return *this;
    }

    L get()
    {
        return qobject_cast<L>(theDocument->getLayer(curLayerIdx));
    }

    int index()
    {
        return curLayerIdx;
    }

protected:
    virtual bool check()
    {
        if (curLayerIdx >= docSize) {
            isAtEnd = true;
            return false;
        }
        if (qobject_cast<L>(theDocument->getLayer((curLayerIdx))) == nullptr)
            return false;

        return true;
    }

    IDocument* theDocument;
    int curLayerIdx;
    bool isAtEnd;
    int docSize;
};


