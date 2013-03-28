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

    bool LT(const ValueType keyType, const Value key) const
    {
        return m_KeyType < keyType
                || (m_KeyType == keyType && m_Key.LT(keyType, key));
    }

    bool LE(const ValueType keyType, const Value key) const
    {
        return m_KeyType < keyType
                || (m_KeyType == keyType && m_Key.LE(keyType, key));
    }

    Value m_Key;
    Value m_Value;
    ValueType m_KeyType     : 4;
    ValueType m_ValueType   : 4;
};

class SkipNode
{
public:
    static SkipNode* Create(const int maxHeight);
    static void Destroy(SkipNode* node);

    static const int BLOCK_LEN  = 192;

    SkipItem m_Items[BLOCK_LEN];
    int m_NumItems;

    int m_Height;

    SkipNode* m_Prev;

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

    static SkipList* Create(const ValueType keyType);
    static void Destroy(SkipList* skiplist);

    bool Insert(const Value key, const ValueType keyType, const Value value, const ValueType valueType);

    bool Delete(const Value key, const ValueType keyType);

    bool Find(const Value key, const ValueType keyType, Value* value, ValueType* valueType) const;

    u64 Count() const;

    double GetUtilization() const;

    void Validate() const;
    
private:

    int m_Height;
    int m_MaxHeight;
    SkipNode* m_Head[MAX_HEIGHT];
    u64 m_Count;
    u64 m_Capacity;

    int LowerBound(const Value key, const ValueType keyType, const SkipItem* first, const size_t numItems) const;
    int UpperBound(const Value key, const ValueType keyType, const SkipItem* first, const size_t numItems) const;

    SkipList(const ValueType keyType);
    ~SkipList();
    SkipList(const SkipList&);
    SkipList& operator=(const SkipList&);
};

}   //namespace honeybase

#endif  //__HB_SKIPLIST_H__
