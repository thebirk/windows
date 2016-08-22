#include "mpack.h"

typedef enum {
    TYPE_HELLO = 0,
    TYPE_GOODBYE,
} MessageType;

void WriteBasicPacket(u8 *data, u32 data_size, u32 type)
{
    mpack_writer_t writer;
    mpack_writer_init(&writer, data, data_size);
    
    mpack_write_cstr(&writer, "type");
    mpack_write_u32(&writer, type);
    mpack_writer_destroy(&writer);
}