
#define COW_MOS_CAPACITY  256

struct cow_stream_s {
        MemoryOutputStream mos;
        MemoryInputStream mis;
        size_t mis_ptr;
        size_t mis_end;
        bool copied;
};

CowStream cow_new(Allocator a, MemoryInputStream mis)
{
        CowStream cow = allocator_alloc(a, sizeof(struct cow_stream_s));
        if(!cow)
                return NULL;

        cow->mos = mos_new(a, COW_MOS_CAPACITY);
        if(!cow->mos)
                return NULL;

        cow->mis = mis;

        return cow;
}

void cow_start(CowStream cow)
{
        cow->copied = false;
        cow->mis_ptr = mis_tell(cow->mis);
        mos_reset(cow->mos);
}

size_t cow_length(CowStream cow)
{
        return cow->copied
                ? mos_length(cow->mos)
                : cow->mis_end - cow->mis_ptr;
}

Bytes cow_pop(CowStream cow)
{
        return cow->copied
                ? mos_pop(cow->mos)
                : mis_at(cow->mis, cow->mis_ptr);
}

void cow_adjust(CowStream cow, size_t amount)
{
        mos_adjust(cow->mos, amount);
        cow->mis_ptr = mis_tell(cow->mis);
}

Bytes cow_reserve(CowStream cow, size_t count)
{
        MemoryInputStream mis = cow->mis;
        cow->copied = true;
        size_t to_copy = mis_tell(mis) - cow->mis_ptr;
        Bytes s = mos_reserve(cow->mos, to_copy + count);
        if(!s)
                return NULL;

        memcpy(s, mis_at(cow->mis, cow->mis_ptr), to_copy);

        cow->mis_ptr += to_copy;
        s += to_copy;
        
        return s;
}

bool cow_finalize(CowStream cow)
{
        cow->mis_end = mis_tell(cow->mis);
        return cow->copied
                ? cow_reserve(cow, 0) != NULL
                : true;
}

