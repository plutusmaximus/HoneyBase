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
    ValueType m_ValType;
};

class SkipNode
{
public:
    static SkipNode* Create();
    static SkipNode* Create(const Key key, const Value value, const ValueType valueType);
    static void Destroy(SkipNode* node);

    static const int BLOCK_LEN  = 64;

    SkipItem m_Items[BLOCK_LEN];
    int m_NumItems;

    Key m_Key;
    Value m_Value;
    int m_Height;

    ValueType m_ValType : 4;

    //This must be the last member in var in the class.
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

    bool Insert2(const Key key, const Value value, const ValueType valueType);

    bool Delete(const Key key);

    bool Find(const Key key, Value* value, ValueType* valueType) const;

    bool Find2(const Key key, Value* value, ValueType* valueType) const;

    double GetUtilization() const;
    
    const KeyType m_KeyType;
    int m_Height;
    unsigned m_Count;
    unsigned m_Capacity;
    SkipNode* m_Head[MAX_HEIGHT];

private:
    
    bool Insert(SkipNode* node);
    const SkipNode* Find(const Key key) const;

    int LowerBound(const Key key, const SkipItem* first, const SkipItem* end) const;
    int UpperBound(const Key key, const SkipItem* first, const SkipItem* end) const;

    SkipList(const KeyType keyType);
    ~SkipList();
    SkipList(const SkipList&);
    SkipList& operator=(const SkipList&);
};

}   //namespace honeybase

#endif  //__HB_SKIPLIST_H__
