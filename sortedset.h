#ifndef __HB_SORTEDSET_H__
#define __HB_SORTEDSET_H__

#include "btree.h"
#include "dict.h"

namespace honeybase
{

class SortedSet
{

public:

    static SortedSet* Create(const KeyType scoreType);
    static void Destroy(SortedSet* set);

    bool Set(const Key& key, const KeyType keyType, const Key& score);//, const unsigned flags);

    bool Clear(const Key& key, const KeyType keyType);

    void ClearAll();

    bool Find(const Key& score, Key* key, KeyType* keyType) const;

    KeyType GetKeyType() const
    {
        return m_Bt->GetKeyType();
    }

    u64 Count() const;

    double GetUtilization() const;

    void Validate();

private:

    SortedSet();
    ~SortedSet();
    SortedSet(const SortedSet&);
    SortedSet& operator=(const SortedSet&);

    BTree* m_Bt;
    HashTable* m_Ht;
};

}   //namespace honeybase

#endif  //__HB_SORTEDSET_H__