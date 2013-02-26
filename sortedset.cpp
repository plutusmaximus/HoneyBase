#include "sortedset.h"

#include <new.h>

namespace honeybase
{

SortedSet*
SortedSet::Create(const KeyType keyType)
{
    SortedSet* set = (SortedSet*) Heap::ZAlloc(sizeof(SortedSet));
    if(set)
    {
        new (set) SortedSet();

        set->m_Bt = BTree::Create(keyType);
        if(set->m_Bt)
        {
            set->m_Ht = HashTable::Create();
        }

        if(!set->m_Bt || !set->m_Ht)
        {
            SortedSet::Destroy(set);
        }
    }

    return set;
}

void
SortedSet::Destroy(SortedSet* set)
{
    if(set)
    {
        set->~SortedSet();
        Heap::Free(set);
    }
}

bool
SortedSet::Set(const Key& key, const KeyType keyType, const Key& score)
{
    HashTable::Slot* slot;
    u32 hash;
    HtItem** pitem = m_Ht->Find(key, keyType, &slot, &hash);

    hbassert(pitem);

    if(*pitem)
    {
        HtItem* item = *pitem;

        hb_static_assert(sizeof(Key) == sizeof(Value));

        hbassert(m_Bt->GetKeyType() == (KeyType)item->m_ValueType);

        hbverify(m_Bt->Delete((const Key&)item->m_Value));

        if(hbverify(m_Bt->Insert(score, (const Value&)key, (ValueType) keyType)))
        {
            if(KEYTYPE_BLOB == m_Bt->GetKeyType())
            {
                item->m_Value.m_Blob->Unref();
                item->m_Value.m_Blob = ((const Value&)score).m_Blob->Ref();
            }
            else
            {
                item->m_Value = (const Value&)score;
            }

            return true;
        }
    }
    else
    {
        hb_static_assert(sizeof(Key) == sizeof(Value));

        HtItem* item = HtItem::Create(key,
                                        keyType,
                                        (const Value&)score,
                                        (ValueType)m_Bt->GetKeyType(),
                                        hash);

        if(hbverify(item))
        {
            if(hbverify(m_Bt->Insert(score, (const Value&)key, (ValueType) keyType)))
            {
                bool replaced;
                m_Ht->Set(item, pitem, slot, &replaced);
                return true;
            }
            else
            {
                HtItem::Destroy(item);
            }
        }
    }

    return false;
}

bool
SortedSet::Clear(const Key& key, const KeyType keyType)
{
    Value value;
    ValueType valueTyoe;
    if(m_Ht->Find(key, keyType, &value, &valueTyoe))
    {
        hb_static_assert(sizeof(Key) == sizeof(Value));

        return m_Bt->Delete((const Key&)value);
    }

    return false;
}

void
SortedSet::ClearAll()
{
}

bool
SortedSet::Find(const Key& score, Key* key, KeyType* keyType) const
{
    hb_static_assert(sizeof(Key) == sizeof(Value));
    return m_Bt->Find(score, (Value*)key, (ValueType*)keyType);
}

u64
SortedSet::Count() const
{
    return m_Ht->Count();
}

double
SortedSet::GetUtilization() const
{
    return 0;
}

//private:

SortedSet::SortedSet()
    : m_Bt(NULL)
    , m_Ht(NULL)
{
}

SortedSet::~SortedSet()
{
    if(m_Bt)
    {
        BTree::Destroy(m_Bt);
    }

    if(m_Ht)
    {
        HashTable::Destroy(m_Ht);
    }
}

}   //namespace honeybase