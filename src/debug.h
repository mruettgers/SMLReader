#ifndef DEBUG_H
#define DEBUG_H

#include "FormattingSerialDebug.h"

#ifdef DEBUG
#define SERIAL_DEBUG true
#else
#define SERIAL_DEBUG false
#endif

void DEBUG_DUMP_BUFFER(byte *buf, int size)
{
#if (defined(SERIAL_DEBUG_VERBOSE) && SERIAL_DEBUG_VERBOSE)
    DEBUG("----DATA----");
    for (int i = 0; i < size; i++)
    {
        if (buf[i] < 16)
        {
            SERIAL_DEBUG_IMPL.print("0");
        }
        SERIAL_DEBUG_IMPL.print(buf[i], HEX);
        SERIAL_DEBUG_IMPL.print(" ");
        if (((i + 1) % 16) == 0)
        {
            SERIAL_DEBUG_IMPL.println();
        }
    }
    SERIAL_DEBUG_IMPL.println();
    DEBUG("---END OF DATA---");
#endif
}

#endif
