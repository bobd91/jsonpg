
#define COW_MOS_CAPACITY  256

CowStream cow_new(Allocator a, MemoryInputStream mis)
{
        CowStream cow = allocator_allocate(a, sizeof(struct cow_stream_s));
        if(!cow)
                return NULL;

        cow->mos = mos_new(a, COW_MOS_CAPACITY);
        if(!cow->mos) {
                allocator_free(cow);
                return NULL;
        }
        cow->mis = mis;

        return cow;
}

void cow_start(CowStream cow)
{
        cow->copied = false;
        cow->ptr = mis_peek(cow->mis);
        mos_reset(cow->mos);
}

size_t cow_length(CowStream cow)
{
        return cow->copied
                ? mos_length(cow->mos)
                : mis_length(cow->mis) - cow->ptr;
}

Bytes cow_pop(CowStream cow)
{
        return cow->copied
                ? mos_pop(cow->mos)
                : mis_at(cow->mis, cow->ptr);
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
        size_t to_copy = mis_tell(mis) - cow->ptr;
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
        return cow->copied
                ? cow_reserve(cow, 0)
                : true;
}

