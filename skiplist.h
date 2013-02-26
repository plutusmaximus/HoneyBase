#ifndef __HB_SKIPLIST_H__
#define __HB_SKIPLIST_H__

#include "hb.h"

namespace honeybase
{

class SkipNode;
class SkipList;

class SkipItem
{
    friend class SkipList;

    Key m_Key;
    Value m_Value;
    ValueType m_ValueType   : 4;
};

class SkipNode
{
public:
    static SkipNode* Create();
    static void Destroy(SkipNode* node);

    static const int BLOCK_LEN  = 96;

    SkipItem m_Items[BLOCK_LEN];
    int m_NumItems;

    int m_Height;

    //*** This must be the last member in the class. ***
    SkipNode* m_Links[1];

private:

    SkipNode();
    ~SkipNode();
    SkipNode(const SkipNode&);
    SkipNode& operator=(const SkipNode&);
};

class SkipList
{
public:
    static const int MAX_HEIGHT = 32;

    static SkipList* Create(const KeyType keyType);
    static void Destroy(SkipList* skiplist);

    bool Insert(const Key key, const Value value, const ValueType valueType);

    bool Delete(const Key key);

    bool Find(const Key key, Value* value, ValueType* valueType) const;

    KeyType GetKeyType() const
    {
        return m_KeyType;
    }

    u64 Count() const;

    double GetUtilization() const;
    
private:

    const KeyType m_KeyType;
    int m_Height;
    SkipNode* m_Head[MAX_HEIGHT];
    u64 m_Count;
    u64 m_Capacity;

    int LowerBound(const Key key, const SkipItem* first, const SkipItem* end) const;
    int UpperBound(const Key key, const SkipItem* first, const SkipItem* end) const;

    SkipList(const KeyType keyType);
    ~SkipList();
    SkipList(const SkipList&);
    SkipList& operator=(const SkipList&);
};

}   //namespace honeybase

#endif  //__HB_SKIPLIST_H__
