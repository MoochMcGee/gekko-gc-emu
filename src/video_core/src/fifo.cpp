/*!
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * \file    fifo.cpp
 * \author  ShizZy <shizzy247@gmail.com>
 * \date    2012-03-08
 * \brief   Implementation of the GP FIFO
 *
 * \section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Official project repository can be found at:
 * http://code.google.com/p/gekko-gc-emu/
 */

#include "common.h"
#include "memory.h"
#include "std_mutex.h"
#include "core.h"

#include "video_core.h"
#include "vertex_loader.h"
#include "fifo.h"
#include "fifo_player.h"
#include "bp_mem.h"
#include "cp_mem.h"
#include "xf_mem.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Graphics Processor namespace

namespace gp {

GPFuncPtr g_exec_op[0x20];      ///< GPOpcode table

u8 g_cur_cmd = 0;               ///< Current command to be executed
u8 g_cur_vat = 0;               ///< Current vertex attribute table

u32 volatile g_fifo_write_ptr;  ///< FIFO write location
u8* volatile g_fifo_read_ptr;   ///< FIFO read location
u8* volatile g_fifo_end_ptr;    ///< End of the primary FIFO buffer

u8* volatile g_dl_read_ptr;     ///< Display list read location

u8 g_fifo_buffer[FIFO_SIZE];    ///< Primary FIFO buffer storage - Don't use directly

u32 volatile g_reset_fifo;      ///< Used to synchronize CPU-GPU threads

u32 g_dl_read_addr;             ///< Display list read address     
u32 g_dl_read_offset;           ///< Display list read offset

u8 (*Fifo_Pop8)();              ///< Pointer to FIFO 8-bit pop method (DL or FIFO)            
u16 (*Fifo_Pop16)();            ///< Pointer to FIFO 16-bit pop method (DL or FIFO)   
u32 (*Fifo_Pop24)();            ///< Pointer to FIFO 24-bit pop method (DL or FIFO)   
u32 (*Fifo_Pop32)();            ///< Pointer to FIFO 32-bit pop method (DL or FIFO)   

////////////////////////////////////////////////////////////////////////////////////////////////////
// FIFO read/write routines

/**
 * @brief Pop a 8-bit byte off FIFO, increment read pointer
 * @return 8-bit byte from FIFO
 */
static inline u8 __fifo_pop_8() {
    return *(g_fifo_read_ptr++);
}

/**
 * @brief Pop a 16-bit halfword off a display list, increment read pointer
 * @return 16-bit halfword from display list
 */
static inline u16 __fifo_pop_16() {
    u16 res = BSWAP16(*((u16*)(g_fifo_read_ptr)));
    g_fifo_read_ptr+=2;
    return res;
}

/**
 * @brief Pop a 24-bit word off a display list, increment read pointer
 * @return 24-bit word from display list
 */
static inline u32 __fifo_pop_24() {
    return (__fifo_pop_8() << 16) | (__fifo_pop_8() << 8) | __fifo_pop_8();
}

/**
 * @brief Pop a 32-bit word off a display list, increment read pointer
 * @return 32-bit word from display list
 */
static inline u32 __fifo_pop_32() {
    u32 res = BSWAP32(*((u32*)(g_fifo_read_ptr)));
    g_fifo_read_ptr+=4;
    return res;
}

/**
 * @brief Pop a 8-bit byte off a display list, increment read pointer
 * @return 8-bit byte from display list
 */
inline u8 __displaylist_pop_8() {
    u32 ret2 = g_dl_read_addr + g_dl_read_offset;
	u32 res = Mem_RAM[(ret2+0) ^ 3];
    g_dl_read_offset+=1;
    return res;
}

/**
 * @brief Pop a 16-bit halfword off a display list, increment read pointer
 * @return 16-bit halfword from display list
 */
static inline u16 __displaylist_pop_16() {
    u32 ret2 = g_dl_read_addr + g_dl_read_offset;
    u16 res = (Mem_RAM[(ret2+0) ^ 3] << 8) |
              (Mem_RAM[(ret2+1) ^ 3]);


    g_dl_read_offset+=2;
    return res;
}

/**
 * @brief Pop a 24-bit word off a display list, increment read pointer
 * @return 24-bit word from display list
 */
static inline u32 __displaylist_pop_24() {
    return (__displaylist_pop_8() << 16) | (__displaylist_pop_8() << 8) | __displaylist_pop_8();
}

/**
 * @brief Pop a 32-bit word off a display list, increment read pointer
 * @return 32-bit word from display list
 */
static inline u32 __displaylist_pop_32() {
    u32 res = g_dl_read_addr + g_dl_read_offset;
    res = (Mem_RAM[(res+0) ^ 3] << 24) |
          (Mem_RAM[(res+1) ^ 3] << 16) |
          (Mem_RAM[(res+2) ^ 3] << 8) |
          (Mem_RAM[(res+3) ^ 3]);

    g_dl_read_offset+=4;
    return res;
}

/// Sets the GP in FIFO read mode
static inline void _set_fifo_read_normal() {
    Fifo_Pop8  = __fifo_pop_8;
    Fifo_Pop16 = __fifo_pop_16;
    Fifo_Pop24 = __fifo_pop_24;
    Fifo_Pop32 = __fifo_pop_32;
}

/// Sets the GP in display list read mode
static inline void _set_fifo_read_displaylists() {
    Fifo_Pop8  = __displaylist_pop_8;
    Fifo_Pop16 = __displaylist_pop_16;
    Fifo_Pop24 = __displaylist_pop_24;
    Fifo_Pop32 = __displaylist_pop_32;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Graphics Processor Instructions

/// unknown command
GP_OPCODE(UNKNOWN) {
    _ASSERT_MSG(TGP, 0, "GP Fifo has been corrupted. Continue?");
}

/// nop - do nothing
GP_OPCODE(NOP) {
    LOG_DEBUG(TGP, "NOP");
}

// load cp register with data
GP_OPCODE(LOAD_CP_REG) {
	u8 addr = Fifo_Pop8();
	u32 data = Fifo_Pop32();
    LOG_DEBUG(TGP, "LOAD_CP_REG: addr=%02x data=%08x", addr, data);
	CP_RegisterWrite(addr, data);
}

/// load xf register with data
GP_OPCODE(LOAD_XF_REG) {
	u32 temp    = Fifo_Pop32();
	u16 length  = (temp >> 16) + 1;
    u16 addr    = temp & 0xFFFF;
    
    LOG_DEBUG(TGP, "LOAD_XF_REG: length=%d addr=%04x", length, addr);

    u32 regs[64];

    _ASSERT_MSG(TGP, length <= 64, "LOAD_XF_REG invalid length=%d!", length);
	for(int i = 0; i < length; i++) {
        regs[i] = Fifo_Pop32();
    }

    XF_Load(length, addr, regs);
}

/// load xf register with data indexed A
GP_OPCODE(LOAD_IDX_A) {
	u8 length;
	u16 index = Fifo_Pop16();
    u16 data = Fifo_Pop16();
    u16 addr;
	length = (data >> 12) + 1;
	addr = data & 0xfff;
    LOG_DEBUG(TGP, "LOAD_IDX_A: index=%04x addr=%04x length=%08x", index, addr, length);
	XF_LoadIndexed(GX_IDX_A, index, length, addr);
}

/// load xf register with data indexed B
GP_OPCODE(LOAD_IDX_B) {
	u8 length;
	u16 index = Fifo_Pop16();
    u16 data = Fifo_Pop16();
    u16 addr;
	length = (data >> 12) + 1;
	addr = data & 0xfff;
    LOG_DEBUG(TGP, "LOAD_IDX_B: index=%04x addr=%04x length=%08x", index, addr, length);
	XF_LoadIndexed(GX_IDX_B, index, length, addr);
}

/// load xf register with data indexed C
GP_OPCODE(LOAD_IDX_C) {
	u8 length;
	u16 index = Fifo_Pop16();
    u16 data = Fifo_Pop16();
    u16 addr;
	length = (data >> 12) + 1;
	addr = data & 0xfff;
    LOG_DEBUG(TGP, "LOAD_IDX_C: index=%04x addr=%04x length=%08x", index, addr, length);
	XF_LoadIndexed(GX_IDX_C, index, length, addr);
}

/// load xf register with data indexed D
GP_OPCODE(LOAD_IDX_D) {
	u8 length;
	u16 index = Fifo_Pop16();
    u16 data = Fifo_Pop16();
    u16 addr;
	length = (data >> 12) + 1;
	addr = data & 0xfff;
    LOG_DEBUG(TGP, "LOAD_IDX_D: index=%04x addr=%04x length=%08x", index, addr, length);
	XF_LoadIndexed(GX_IDX_D, index, length, addr);
}

/// call a display list
GP_OPCODE(CALL_DISPLAYLIST) {
	u32 addr = Fifo_Pop32() & RAM_MASK;
    u32 size = Fifo_Pop32();

    g_dl_read_addr = addr;
    g_dl_read_offset = 0;
    LOG_DEBUG(TGP, "CALL_DISPLAYLIST: addr=%08x size=%08x", addr, size);

    _set_fifo_read_displaylists();

    while (g_dl_read_offset < size) {
        g_cur_cmd = Fifo_Pop8();
        g_cur_vat = g_cur_cmd & 0x7;
        g_exec_op[GP_OPMASK(g_cur_cmd)]();
    }

    _set_fifo_read_normal();

    LOG_DEBUG(TGP, "CALL_DISPLAYLIST finished", addr, size);
}

/// invalidate vertex cache
GP_OPCODE(INVALIDATE_VERTEX_CACHE) {
    LOG_DEBUG(TGP, "INVALIDATE_VERTEX_CACHE");
}

/// load bp register with data
GP_OPCODE(LOAD_BP_REG) {
    u32 data = Fifo_Pop32();
    LOG_DEBUG(TGP, "LOAD_BP_REG: addr=%02x data=%08x", data >> 24, data & 0x00FFFFFF);
	BP_RegisterWrite(data >> 24, data & 0x00FFFFFF);
}

/// draw a primitive - quads
GP_OPCODE(DRAW_QUADS) {
	u16 count = Fifo_Pop16();
    LOG_DEBUG(TGP, "\t\tDRAW_QUADS: count=%04x", count);
	VertexLoader_DecodePrimitive(GX_QUADS, count);
}

/// draw a primitive - triangles
GP_OPCODE(DRAW_TRIANGLES) {
	u16 count = Fifo_Pop16();
    LOG_DEBUG(TGP, "\t\tDRAW_TRIANGLES: count=%04x", count);
    VertexLoader_DecodePrimitive(GX_TRIANGLES, count);
}

/// draw a primitive - trianglestrip
GP_OPCODE(DRAW_TRIANGLESTRIP) {
	u16 count = Fifo_Pop16();
    LOG_DEBUG(TGP, "\t\tDRAW_TRIANGLESTRIP: count=%04x", count);
	VertexLoader_DecodePrimitive(GX_TRIANGLESTRIP, count);
}

/// draw a primitive - trianglefan
GP_OPCODE(DRAW_TRIANGLEFAN) {
	u16 count = Fifo_Pop16();
    LOG_DEBUG(TGP, "\t\tDRAW_TRIANGLEFAN: count=%04x", count);
	VertexLoader_DecodePrimitive(GX_TRIANGLEFAN, count);
}

/// draw a primitive - lines
GP_OPCODE(DRAW_LINES) {
	u16 count = Fifo_Pop16();
    LOG_DEBUG(TGP, "\t\tDRAW_LINES: count=%04x", count);
	VertexLoader_DecodePrimitive(GX_LINES, count);
}

/// draw a primitive - linestrip
GP_OPCODE(DRAW_LINESTRIP) {
	u16 count = Fifo_Pop16();
    LOG_DEBUG(TGP, "\t\tDRAW_LINESTRIP: count=%04x", count);
	VertexLoader_DecodePrimitive(GX_LINESTRIP, count);
}

/// draw a primitive - points
GP_OPCODE(DRAW_POINTS) {
    u16 count = Fifo_Pop16();
    LOG_DEBUG(TGP, "\t\tDRAW_POINTS: count=%04x", count);
	VertexLoader_DecodePrimitive(GX_POINTS, count);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// FIFO flow control

/// Returns true if the current command in the FIFO is ready to be decoded
bool Fifo_NextCommandReady() {
    static int last_required_size = -1;

    // We haven't started (at the beginning), or something went terrible wrong...
    if (g_fifo_read_ptr == (g_fifo_buffer + g_fifo_write_ptr)) {
        return false;
    }
    // Otherwise, read_ptr < write_ptr:
    uintptr_t bytes_in_fifo = (g_fifo_buffer + g_fifo_write_ptr) - g_fifo_read_ptr;

    // Last size still right...
	if ((last_required_size != -1) && (last_required_size > (s32)bytes_in_fifo)) {
	    return false;
    }

    // Get current command and vat
    g_cur_cmd = FIFO_GET8(0);
    g_cur_vat = g_cur_cmd & 0x7;

    // Determine opcode size
    switch(GP_OPMASK(g_cur_cmd))
    {
    case 0: // NOP
        last_required_size = -1;
        return true;
    
    case 1: // LOAD_CP
        if(bytes_in_fifo >= 6) {
            last_required_size = -1;
            return true;
        }
        last_required_size = 6;
        break; 
    
    case 2: // LOAD XF
        if (bytes_in_fifo >= 5) { // if header is present
            u32 temp = FIFO_GET32(1);
	        u16 size  = 4 * ((temp >> 16) + 1);
            size += 5;
            if(bytes_in_fifo >= size)
            {
                last_required_size = -1;
                return true;
            }
            else {
                last_required_size = size;
            }
        } else {
            last_required_size = 5;
        }
        break;

    case 4: // LOAD IDX A
    case 5: // " B
    case 6: // " C 
    case 7: // " D
        if (bytes_in_fifo >= 5) {
            last_required_size = -1;
            return true;
        }
        last_required_size = 5;
        break; 

    case 8: // CALL_DL
        if (bytes_in_fifo >= 9) {
            last_required_size = -1;
            return true;
        }
        last_required_size = 9;
        break; 

    case 9: // INVALID_VTX_CACHE
        last_required_size = -1;
        return true;

    case 0xC: // LOAD BP
        if (bytes_in_fifo >= 5) {
            last_required_size = -1;
            return true;
        }
        last_required_size = 5;
        return false; 

    default: 		
        if(g_cur_cmd & 0x80) { // Draw command
            if(bytes_in_fifo >= 3) {    // See if header exists
                u16 numverts = FIFO_GET16(1);
                u16 vertsize = VertexLoader_GetVertexSize();
                u16 size = 3;

                size += numverts * vertsize;
                if(bytes_in_fifo >= size) {
                    last_required_size = -1;
                    return true;
                } else {
                    last_required_size = size;
                }
            } else {
                last_required_size = 3;
            }
        }
        return false;
    }
    return false;
}

int Fifo_GetCommandLength(u8* read_ptr)
{
#define GET8(ptr)      (*ptr)
#define GET16(ptr)     (BSWAP16(*((u16*)(ptr))))
#define GET32(ptr)     (BSWAP32(*((u32*)(ptr))))

    u8 cmd = *read_ptr++;
    int command_size = 0;

    switch (GP_OPMASK(cmd))
    {
    case 0: // NOP
    case 9: // INVALID_VTX_CACHE
//    case GP_OPMASK(GX_CMD_UNKNOWN_METRICS): // TODO 
        command_size = 1;
        break;

    case 0xC: // LOAD BP
        command_size = 5;
        break;

    case 1: // LOAD_CP
        command_size = 6;
        break;

    case 4: // LOAD IDX A
    case 5: // " B
    case 6: // " C
    case 7: // " D
        command_size = 5;
        break;

    case 8: // CALL_DL
        // TODO: return full size?
        command_size = 9;
        break;

    case 2: // LOAD XF
        {
            command_size = 1 + 4;
            u32 Cmd2 = GET32(read_ptr);
            int transfer_size = ((Cmd2 >> 16) & 15) + 1;
            command_size += transfer_size * 4;
        }
        break;

    default:
        if(g_cur_cmd & 0x80) // Draw command
        {
            command_size = 1 + 2;
            u16 numVertices = GET16(read_ptr);
            command_size += numVertices * VertexLoader_GetVertexSize();
        }
        else
        {
            // TODO: Error or sth
        }
        break;
    }
    return command_size;
}

/// Called at end of frame to reset FIFO
void Fifo_Reset() {
    if (g_fifo_write_ptr > FIFO_TAIL_END) {

        // Move FIFO to beginning
        g_fifo_write_ptr    = 0;
        g_fifo_read_ptr     = g_fifo_buffer;
        memset(g_fifo_buffer, 0, FIFO_SIZE);

        g_reset_fifo = 0;
    }
}

/// Decodes current FIFO command
void Fifo_DecodeCommand() {

    int bytes_in_fifo = g_fifo_write_ptr - (g_fifo_read_ptr - g_fifo_buffer);

    if (bytes_in_fifo < 1) {
        if (!g_reset_fifo) {
            return;
        }
    }
    _ASSERT_MSG(TGP, g_fifo_write_ptr >=  (g_fifo_read_ptr - g_fifo_buffer), 
        "GP decoding read_ptr > wrote_ptr, this should never happen!");
        
    // Get the next GP opcode and decode it
    if (Fifo_NextCommandReady()) {
        // TODO: Display list handling...
        if (GP_OPMASK(g_cur_cmd) != 8 && fifo_player::IsRecording())
        {
            fifo_player::Write(g_fifo_read_ptr, Fifo_GetCommandLength(g_fifo_read_ptr));
        }
        g_exec_op[GP_OPMASK(Fifo_Pop8())]();
    }
    return;
}

/// Initialize GP FIFO
void Fifo_Init() {
    _set_fifo_read_normal();

    // FIFO pointers
    g_fifo_write_ptr    = 0;
    g_fifo_read_ptr     = g_fifo_buffer;

    g_reset_fifo        = 0;

    // Zero FIFO memory
	memset(g_fifo_buffer, 0, FIFO_SIZE);

    g_reset_fifo = 0;
    g_dl_read_addr = 0;
    g_dl_read_offset = 0;

	// Init op table
	for(int i = 0; i < 0x20; i++) {
		GP_SETOP(i, GPOPCODE_UNKNOWN);
    }

	// Create op table
	GP_SETOP(GP_OPMASK(GP_NOP), GPOPCODE_NOP);	
	GP_SETOP(GP_OPMASK(GP_LOAD_CP_REG), GPOPCODE_LOAD_CP_REG);				
	GP_SETOP(GP_OPMASK(GP_LOAD_XF_REG), GPOPCODE_LOAD_XF_REG);
	GP_SETOP(GP_OPMASK(GP_LOAD_IDX_A), GPOPCODE_LOAD_IDX_A);
	GP_SETOP(GP_OPMASK(GP_LOAD_IDX_B), GPOPCODE_LOAD_IDX_B);
	GP_SETOP(GP_OPMASK(GP_LOAD_IDX_C), GPOPCODE_LOAD_IDX_C);
	GP_SETOP(GP_OPMASK(GP_LOAD_IDX_D), GPOPCODE_LOAD_IDX_D);
	GP_SETOP(GP_OPMASK(GP_CALL_DISPLAYLIST), GPOPCODE_CALL_DISPLAYLIST);
	GP_SETOP(GP_OPMASK(GP_INVALIDATE_VERTEX_CACHE), GPOPCODE_INVALIDATE_VERTEX_CACHE);
	GP_SETOP(GP_OPMASK(GP_LOAD_BP_REG), GPOPCODE_LOAD_BP_REG);	
	GP_SETOP(GP_OPMASK(GP_DRAW_QUADS), GPOPCODE_DRAW_QUADS);
	GP_SETOP(GP_OPMASK(GP_DRAW_TRIANGLES), GPOPCODE_DRAW_TRIANGLES);
	GP_SETOP(GP_OPMASK(GP_DRAW_TRIANGLESTRIP), GPOPCODE_DRAW_TRIANGLESTRIP);
	GP_SETOP(GP_OPMASK(GP_DRAW_TRIANGLEFAN), GPOPCODE_DRAW_TRIANGLEFAN);
	GP_SETOP(GP_OPMASK(GP_DRAW_LINES), GPOPCODE_DRAW_LINES);
	GP_SETOP(GP_OPMASK(GP_DRAW_LINESTRIP), GPOPCODE_DRAW_LINESTRIP);
	GP_SETOP(GP_OPMASK(GP_DRAW_POINTS), GPOPCODE_DRAW_POINTS);
}

/// Shutdown GP FIFO
void Fifo_Shutdown() {
}

} // namespace
