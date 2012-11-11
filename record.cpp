class RecType
{
public:
    static const RecType* const INT_TYPE;
    static const RecType* const FLOAT_TYPE;
    static const RecType* const STRING_TYPE;
};

class RecField
{
    char* m_Name;
    const RecType* m_Type;
};

class RecShape : public RecType
{
    RecField* m_Fields;
    int m_NumFields;
};

static const RecType s_INT_TYPE;
static const RecType s_FLOAT_TYPE;
static const RecType s_STRING_TYPE;
static const RecType* const INT_TYPE = &s_INT_TYPE;
static const RecType* const FLOAT_TYPE = &s_FLOAT_TYPE;
static const RecType* const STRING_TYPE = &s_STRING_TYPE;