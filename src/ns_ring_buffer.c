//*****************************************************************************
//
//! @file ns_ring_buffer.c
//!
//! @brief Byte-oriented ring buffer with interrupt-safe push/pop operations.
//!
//! Derived from Ambiq Micro ns-ipc ring buffer (BSD-3-Clause, rev 1.2.9).
//! Improvements over the original:
//!   - Simplified init: no setup struct, single buffer passed by pointer.
//!   - Pointer advanced explicitly in copy loops (no ui32TempLen offset trick).
//!   - Early-return bug inside AM_CRITICAL_BEGIN fixed (PRIMASK now restored).
//!   - ns_ring_buffer_full() promoted to public API.
//!   - Internal ns_ring_buffer_overwrite() made static.
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2017, Ambiq Micro
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "am_mcu_apollo.h"
#include "ns_ring_buffer.h"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static uint8_t
ns_ring_buffer_overwrite(ns_ring_buffer_t *psBuffer) {
    return (psBuffer->ui32Tail != psBuffer->ui32Head) && psBuffer->ui32Overwrite;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

uint8_t
ns_ring_buffer_empty(ns_ring_buffer_t *psBuffer) {
    return (psBuffer->ui32Tail == psBuffer->ui32Head) && !psBuffer->ui32Overwrite;
}

uint8_t
ns_ring_buffer_full(ns_ring_buffer_t *psBuffer) {
    return (psBuffer->ui32Tail == psBuffer->ui32Head) && psBuffer->ui32Overwrite;
}

void
ns_ring_buffer_init(ns_ring_buffer_t *psBuffer, void *pData, uint32_t ui32Bytes) {
    psBuffer->ui32Head      = 0;
    psBuffer->ui32Tail      = 0;
    psBuffer->ui32Overwrite = 0;
    psBuffer->ui32Capacity  = ui32Bytes;
    psBuffer->pui8Data      = (volatile uint8_t *)pData;
}

uint32_t
ns_ring_buffer_push(ns_ring_buffer_t *psBuffer, void *pvSource, uint32_t ui32Bytes,
                    bool bFullCheck) {
    uint32_t ui32CopyLen      = ui32Bytes;
    uint32_t ui32ReturnLen    = 0;
    uint32_t ui32Tail         = psBuffer->ui32Tail;
    uint32_t ui32Head         = psBuffer->ui32Head;
    uint32_t ui32Capacity     = psBuffer->ui32Capacity;
    uint8_t *pui8Source       = (uint8_t *)pvSource;
    uint32_t ui32TempLen;
    volatile uint32_t ui32Primask;

    if (bFullCheck) {
        ui32Primask = am_hal_interrupt_master_disable();

        // Refuse entirely when the buffer is already full.
        if (ns_ring_buffer_full(psBuffer)) {
            am_hal_interrupt_master_set(ui32Primask);
            return 0;
        }

        // Clamp copy length so we never push more than the free space.
        if (ns_ring_buffer_empty(psBuffer)) {
            if (ui32CopyLen >= ui32Capacity) {
                psBuffer->ui32Overwrite = 1;
                ui32CopyLen = ui32Capacity;
            }
        } else {
            uint32_t ui32Free = (ui32Head + ui32Capacity - ui32Tail) % ui32Capacity;
            if (ui32Free <= ui32CopyLen) {
                psBuffer->ui32Overwrite = 1;
                ui32CopyLen = ui32Free;
            }
        }

        ui32ReturnLen = ui32CopyLen;

        while ((ui32Tail + ui32CopyLen) >= ui32Capacity) {
            ui32TempLen = ui32Capacity - ui32Tail;
            memcpy((void *)&psBuffer->pui8Data[ui32Tail], pui8Source, ui32TempLen);
            ui32Tail = psBuffer->ui32Tail = (ui32Tail + ui32TempLen) % ui32Capacity;
            pui8Source  += ui32TempLen;
            ui32CopyLen -= ui32TempLen;
        }

        memcpy((void *)&psBuffer->pui8Data[ui32Tail], pui8Source, ui32CopyLen);
        psBuffer->ui32Tail = (ui32Tail + ui32CopyLen) % ui32Capacity;

        am_hal_interrupt_master_set(ui32Primask);
        return ui32ReturnLen;
    } else {
        // No full check: overwrite oldest data when the buffer fills up.
        ui32Primask = am_hal_interrupt_master_disable();

        if (ns_ring_buffer_empty(psBuffer)) {
            if (ui32CopyLen >= ui32Capacity) {
                psBuffer->ui32Overwrite = 1;
            }
        } else {
            uint32_t ui32Free = (ui32Head + ui32Capacity - ui32Tail) % ui32Capacity;
            if (ui32Free <= ui32CopyLen) {
                psBuffer->ui32Overwrite = 1;
            }
        }

        ui32ReturnLen = ui32CopyLen;

        while ((ui32Tail + ui32CopyLen) >= ui32Capacity) {
            ui32TempLen = ui32Capacity - ui32Tail;
            memcpy((void *)&psBuffer->pui8Data[ui32Tail], pui8Source, ui32TempLen);
            ui32Tail = psBuffer->ui32Tail = (ui32Tail + ui32TempLen) % ui32Capacity;
            pui8Source  += ui32TempLen;
            ui32CopyLen -= ui32TempLen;
        }

        memcpy((void *)&psBuffer->pui8Data[ui32Tail], pui8Source, ui32CopyLen);
        ui32Tail = psBuffer->ui32Tail = (ui32Tail + ui32CopyLen) % ui32Capacity;

        // Keep the read pointer aligned with write when we have overwritten.
        if (psBuffer->ui32Overwrite) {
            psBuffer->ui32Head = ui32Tail;
        }

        am_hal_interrupt_master_set(ui32Primask);
        return ui32ReturnLen;
    }
}

uint32_t
ns_ring_buffer_pop(ns_ring_buffer_t *psBuffer, void *pvDest, uint32_t ui32Bytes) {
    uint32_t ui32Available  = ns_ring_buffer_used(psBuffer);
    uint32_t ui32Head       = psBuffer->ui32Head;
    uint32_t ui32Capacity   = psBuffer->ui32Capacity;
    uint8_t *pui8Dest       = (uint8_t *)pvDest;
    uint32_t ui32TempLen;
    uint32_t ui32ReturnLen;
    volatile uint32_t ui32Primask;

    ui32Primask = am_hal_interrupt_master_disable();

    // If an overwrite occurred, advance the read pointer to the write pointer
    // before we start reading so the data we return is consistent.
    if (ns_ring_buffer_overwrite(psBuffer)) {
        ui32Head = psBuffer->ui32Head = psBuffer->ui32Tail;
    }

    // Clamp to available data.
    uint32_t ui32CopyLen = ui32Bytes < ui32Available ? ui32Bytes : ui32Available;
    ui32ReturnLen = ui32CopyLen;

    while ((ui32Head + ui32CopyLen) >= ui32Capacity) {
        ui32TempLen = ui32Capacity - ui32Head;
        memcpy(pui8Dest, (void *)&psBuffer->pui8Data[ui32Head], ui32TempLen);
        ui32Head = psBuffer->ui32Head = (ui32Head + ui32TempLen) % ui32Capacity;
        pui8Dest    += ui32TempLen;
        ui32CopyLen -= ui32TempLen;
    }

    memcpy(pui8Dest, (void *)&psBuffer->pui8Data[ui32Head], ui32CopyLen);
    psBuffer->ui32Head = (ui32Head + ui32CopyLen) % ui32Capacity;
    psBuffer->ui32Overwrite = 0;

    am_hal_interrupt_master_set(ui32Primask);
    return ui32ReturnLen;
}

uint32_t
ns_ring_buffer_used(ns_ring_buffer_t *psBuffer) {
    uint32_t ui32Tail     = psBuffer->ui32Tail;
    uint32_t ui32Head     = psBuffer->ui32Head;
    uint32_t ui32Capacity = psBuffer->ui32Capacity;

    if (ns_ring_buffer_overwrite(psBuffer) || ns_ring_buffer_full(psBuffer)) {
        return ui32Capacity;
    }

    return (ui32Tail + ui32Capacity - ui32Head) % ui32Capacity;
}

void
ns_ring_buffer_flush(ns_ring_buffer_t *psBuffer) {
    psBuffer->ui32Overwrite = 0;
    psBuffer->ui32Head      = psBuffer->ui32Tail;
}

uint32_t
ns_ring_buffer_drain(ns_ring_buffer_t *psSource, void *pvDest, uint32_t ui32FrameBytes) {
    if (ns_ring_buffer_used(psSource) >= ui32FrameBytes) {
        ns_ring_buffer_pop(psSource, pvDest, ui32FrameBytes);
        return 1;
    }
    return 0;
}
