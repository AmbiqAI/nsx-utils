//*****************************************************************************
//
//! @file ns_ring_buffer.h
//!
//! @brief Byte-oriented ring buffer with interrupt-safe push/pop operations.
//!
//! Derived from Ambiq Micro ns-ipc ring buffer (BSD-3-Clause, rev 1.2.9).
//! API cleaned up: no setup struct, shorter field names, full public API.
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

#ifndef NS_RING_BUFFER_H
#define NS_RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

//*****************************************************************************
//
//! @brief Ring buffer control structure.
//!
//! @note The caller must ensure that @p pui8Data points to a backing array
//! that lives at least as long as the ring buffer itself.  The struct fields
//! are all volatile so that interrupt-context access works correctly when
//! combined with the AM_CRITICAL_BEGIN / AM_CRITICAL_END guards used
//! internally.
//
//*****************************************************************************
typedef struct {
    volatile uint8_t  *pui8Data;      //!< pointer to backing storage
    volatile uint32_t  ui32Tail;      //!< write index (producer)
    volatile uint32_t  ui32Head;      //!< read  index (consumer)
    volatile uint32_t  ui32Overwrite; //!< non-zero when write has lapped read
    volatile uint32_t  ui32Capacity;  //!< total backing storage size in bytes
} ns_ring_buffer_t;

//*****************************************************************************
//
//! @brief Initialize (or re-initialize) a ring buffer.
//!
//! Calling this on a buffer that is already in use discards all pending data.
//! This operation is NOT interrupt-safe; the caller must guard it from
//! concurrent access.
//!
//! @param psBuffer   Ring buffer structure to initialize.
//! @param pData      Backing storage array (must be at least ui32Bytes long).
//! @param ui32Bytes  Capacity of the backing storage in bytes.
//
//*****************************************************************************
void ns_ring_buffer_init(ns_ring_buffer_t *psBuffer, void *pData, uint32_t ui32Bytes);

//*****************************************************************************
//
//! @brief Push bytes into the ring buffer.
//!
//! @param psBuffer   Buffer to write into.
//! @param pvSource   Source data to copy.
//! @param ui32Bytes  Number of bytes to push.
//! @param bFullCheck When @c true the push stops when the buffer is full and
//!                   no data is lost; when @c false the oldest data is silently
//!                   overwritten so that all @p ui32Bytes are accepted.
//!
//! @return Number of bytes actually written into the buffer.
//
//*****************************************************************************
uint32_t ns_ring_buffer_push(ns_ring_buffer_t *psBuffer, void *pvSource,
                             uint32_t ui32Bytes, bool bFullCheck);

//*****************************************************************************
//
//! @brief Pop bytes from the ring buffer.
//!
//! @param psBuffer   Buffer to read from.
//! @param pvDest     Destination memory for the popped bytes.
//! @param ui32Bytes  Maximum number of bytes to pop.
//!
//! @return Number of bytes actually read from the buffer.
//
//*****************************************************************************
uint32_t ns_ring_buffer_pop(ns_ring_buffer_t *psBuffer, void *pvDest, uint32_t ui32Bytes);

//*****************************************************************************
//
//! @brief Return the number of bytes currently available to read.
//
//*****************************************************************************
uint32_t ns_ring_buffer_used(ns_ring_buffer_t *psBuffer);

//*****************************************************************************
//
//! @brief Return non-zero if the buffer contains no data.
//
//*****************************************************************************
uint8_t ns_ring_buffer_empty(ns_ring_buffer_t *psBuffer);

//*****************************************************************************
//
//! @brief Return non-zero if the buffer is completely full.
//
//*****************************************************************************
uint8_t ns_ring_buffer_full(ns_ring_buffer_t *psBuffer);

//*****************************************************************************
//
//! @brief Discard all buffered data without modifying backing storage.
//
//*****************************************************************************
void ns_ring_buffer_flush(ns_ring_buffer_t *psBuffer);

//*****************************************************************************
//
//! @brief Pop exactly @p ui32FrameBytes if at least that many bytes are
//! available; otherwise do nothing.
//!
//! Designed for use inside a processing loop:
//! @code
//!   while (ns_ring_buffer_drain(src, dst, FRAME_SIZE)) {
//!       process(dst);
//!   }
//! @endcode
//!
//! @param psSource       Source ring buffer.
//! @param pvDest         Destination for the frame data.
//! @param ui32FrameBytes Exact number of bytes that make up one frame.
//!
//! @return 1 if a full frame was popped, 0 if not enough data was available.
//
//*****************************************************************************
uint32_t ns_ring_buffer_drain(ns_ring_buffer_t *psSource, void *pvDest,
                              uint32_t ui32FrameBytes);

#ifdef __cplusplus
}
#endif

#endif // NS_RING_BUFFER_H
