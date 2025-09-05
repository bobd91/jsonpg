

size_t cow_length(CowStream cs)
{
        JSONPG_ASSERT(cs);

        return cs->copied
                ? mos_length(cs->os)
                : input_length(cs->is) - cs->ptr;
}

Bytes cow_pop(CowStream cs)
{
        JSONPG_ASSERT(cs);

        return cs->copied
                ? mos_pop(cs->os)
                : input_at(cs->is, cs->ptr);
}

void cow_adjust(CowStream cs, size_t amount)
{
        JSONPG_ASSERT(cs);

        mos_adjust(cs->os, amount);
        cs->is_ptr = input_tell(cs->is);
}

Bytes cow_reserve(CowStream cs, size_t count)
{
        JSONPG_ASSERT(cs);

        cs->copied = true;
        MemoryOutputStream os = cs->os;
        InputStream is = cs->is;
        size_t to_copy = input_tell(is) - cs->ptr;
        Bytes s = mos_reserve(os, to_copy + count);
        if(!s)
                return NULL;

        memcpy(s, input_at(cs->is, cs->is_ptr), to_copy);
        return s;
}

bool cow_finalize(CowStream cs)
{
        JSONPG_ASSERT(cs);

        return cs->copied
                ? cow_reserve(cs, 0)
                : true;
}

