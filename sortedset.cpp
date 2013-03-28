#include "sortedset.h"

#include <new.h>

namespace honeybase
{

SortedSet*
SortedSet::Create(const ValueType keyType)
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
SortedSet::Set(const Value& key, const ValueType keyType, const Value& score)
{
    HashTable::Slot* slot;
    u32 hash;
    HtItem** pitem = m_Ht->Find(key, keyType, &slot, &hash);

    hbassert(pitem);

    if(*pitem)
    {
        HtItem* item = *pitem;

        hbassert(m_Bt->GetKeyType() == item->m_ValueType);

        hbverify(m_Bt->Delete(item->m_Value, key, keyType));

        if(hbverify(m_Bt->Insert(score, key, (ValueType) keyType)))
        {
            if(VALUETYPE_BLOB == m_Bt->GetKeyType())
            {
                item->m_Value.m_Blob->Unref();
                item->m_Value.m_Blob = score.m_Blob;
                item->m_Value.m_Blob->Ref();
            }
            else
            {
                item->m_Value = score;
            }

            return true;
        }
    }
    else
    {
        HtItem* item = HtItem::Create(key,
                                        keyType,
                                        score,
                                        (ValueType)m_Bt->GetKeyType(),
                                        hash);

        if(hbverify(item))
        {
            if(hbverify(m_Bt->Insert(score, key, (ValueType) keyType)))
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
SortedSet::Clear(const Value& key, const ValueType keyType)
{
    Value value;
    ValueType valueTyoe;
    if(m_Ht->Find(key, keyType, &value, &valueTyoe))
    {
        return m_Bt->Delete(value, key, keyType);
    }

    return false;
}

void
SortedSet::ClearAll()
{
}

bool
SortedSet::Find(const Value& score, Value* key, ValueType* keyType) const
{
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
        m_Ht->Unref();
    }
}

}   //namespace honeybase