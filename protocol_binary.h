/*
 * Copyright (c) <2008>, Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the  nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SUN MICROSYSTEMS, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SUN MICROSYSTEMS, INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Summary: Constants used by to implement the binary protocol.
 *
 * Copy: See Copyright for the status of this software.
 *
 * Author: Trond Norbye <trond.norbye@sun.com>
 */

#ifndef PROTOCOL_BINARY_H
#define PROTOCOL_BINARY_H

#include <stdint.h>

/**
 * This file contains definitions of the constants and packet formats
 * defined in the binary specification. Please note that you _MUST_ remember
 * to convert each multibyte field to / from network byte order to / from
 * host order.
 */
#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * Definition of the legal "magic" values used in a packet.
   * See section 3.1 Magic byte
   */
  typedef enum {
    PROTOCOL_BINARY_REQ = 0x80,
    PROTOCOL_BINARY_RES = 0x81,
  } protocol_binary_magic;

  /**
   * Definition of the valid response status numbers.
   * See section 3.2 Response Status
   */
  typedef enum {
    PROTOCOL_BINARY_RESPONSE_SUCCESS = 0x00,
    PROTOCOL_BINARY_RESPONSE_KEY_ENOENT = 0x01,
    PROTOCOL_BINARY_RESPONSE_KEY_EEXISTS = 0x02,
    PROTOCOL_BINARY_RESPONSE_E2BIG = 0x03,
    PROTOCOL_BINARY_RESPONSE_EINVAL = 0x04,
    PROTOCOL_BINARY_RESPONSE_NOT_STORED = 0x05,
    PROTOCOL_BINARY_RESPONSE_UNKNOWN_COMMAND = 0x81,
    PROTOCOL_BINARY_RESPONSE_ENOMEM = 0x82,
  } protocol_binary_response_status;

  /**
   * Defintion of the different command opcodes.
   * See section 3.3 Command Opcodes
   */
  typedef enum {
    PROTOCOL_BINARY_CMD_GET = 0x00,
    PROTOCOL_BINARY_CMD_SET = 0x01,
    PROTOCOL_BINARY_CMD_ADD = 0x02,
    PROTOCOL_BINARY_CMD_REPLACE = 0x03,
    PROTOCOL_BINARY_CMD_DELETE = 0x04,
    PROTOCOL_BINARY_CMD_INCREMENT = 0x05,
    PROTOCOL_BINARY_CMD_DECREMENT = 0x06,
    PROTOCOL_BINARY_CMD_QUIT = 0x07,
    PROTOCOL_BINARY_CMD_FLUSH = 0x08,
    PROTOCOL_BINARY_CMD_GETQ = 0x09,
    PROTOCOL_BINARY_CMD_NOOP = 0x0a,
    PROTOCOL_BINARY_CMD_VERSION = 0x0b,
    PROTOCOL_BINARY_CMD_GETK = 0x0c,
    PROTOCOL_BINARY_CMD_GETKQ = 0x0d,
    PROTOCOL_BINARY_CMD_APPEND = 0x0e,
    PROTOCOL_BINARY_CMD_PREPEND = 0x0f,
  } protocol_binary_command;

  /**
   * Definition of the data types in the packet
   * See section 3.4 Data Types
   */
  typedef enum {
    PROTOCOL_BINARY_RAW_BYTES = 0x00,
  } protocol_binary_datatypes;

  /**
   * Definition of the header structure for a request packet.
   * See section 2
   */
  typedef union {
    struct {
      uint8_t magic;
      uint8_t opcode;
      uint16_t keylen;
      uint8_t extlen;
      uint8_t datatype;
      uint16_t reserved;
      uint32_t bodylen;
      uint32_t opaque;
      uint64_t cas;
    } request;
    uint8_t bytes[24];
  } protocol_binary_request_header;

  /**
   * Definition of the header structure for a response packet.
   * See section 2
   */
  typedef union {
    struct {
      uint8_t magic;
      uint8_t opcode;
      uint16_t keylen;
      uint8_t extlen;
      uint8_t datatype;
      uint16_t status;
      uint32_t bodylen;
      uint32_t opaque;
      uint64_t cas;
    } response;
    uint8_t bytes[24];
  } protocol_binary_response_header;

  /**
   * Definition of a request-packet containing no extras
   */
  typedef union {
    struct {
      protocol_binary_request_header header;
    } message;
    uint8_t bytes[sizeof(protocol_binary_request_header)];
  } protocol_binary_request_no_extras;

  /**
   * Definition of a response-packet containing no extras
   */
  typedef union {
    struct {
      protocol_binary_response_header header;
    } message;
    uint8_t bytes[sizeof(protocol_binary_response_header)];
  } protocol_binary_response_no_extras;

  /**
   * Definition of the packet used by the get, getq, getk and getkq command.
   * See section 4.1
   */
  typedef protocol_binary_request_no_extras protocol_binary_request_get;
  typedef protocol_binary_request_no_extras protocol_binary_request_getq;
  typedef protocol_binary_request_no_extras protocol_binary_request_getk;
  typedef protocol_binary_request_no_extras protocol_binary_request_getkq;

  /**
   * Definition of the packet returned from a successful get, getq, getk and
   * getkq.
   * See section 4.1
   */
  typedef union {
    struct {
      protocol_binary_response_header header;
      struct {
    uint32_t flags;
      } body;
    } message;
    uint8_t bytes[sizeof(protocol_binary_response_header) + 4];
  } protocol_binary_response_get;

  typedef protocol_binary_response_get protocol_binary_response_getq;
  typedef protocol_binary_response_get protocol_binary_response_getk;
  typedef protocol_binary_response_get protocol_binary_response_getkq;

  /**
   * Definition of the packet used by the delete command
   * See section 4.2
   * Please note that the expiration field is optional, so remember to see
   * check the header.bodysize to see if it is present.
   */
  typedef union {
    struct {
      protocol_binary_request_header header;
      struct {
    uint32_t expiration;
      } body;
    } message;
    uint8_t bytes[sizeof(protocol_binary_request_header) + 4];
  } protocol_binary_request_delete;

  /**
   * Definition of the packet returned by the delete command
   * See section 4.2
   */
  typedef protocol_binary_response_no_extras protocol_binary_response_delete;

  /**
   * Definition of the packet used by the flush command
   * See section 4.3
   * Please note that the expiration field is optional, so remember to see
   * check the header.bodysize to see if it is present.
   */
  typedef union {
    struct {
      protocol_binary_request_header header;
      struct {
    uint32_t expiration;
      } body;
    } message;
    uint8_t bytes[sizeof(protocol_binary_request_header) + 4];
  } protocol_binary_request_flush;

  /**
   * Definition of the packet returned by the flush command
   * See section 4.3
   */
  typedef protocol_binary_response_no_extras protocol_binary_response_flush;

  /**
   * Definition of the packet used by set, add and replace
   * See section 4.4
   */
  typedef union {
    struct {
      protocol_binary_request_header header;
      struct {
    uint32_t flags;
    uint32_t expiration;
      } body;
    } message;
    uint8_t bytes[sizeof(protocol_binary_request_header) + 8];
  } protocol_binary_request_set;
  typedef protocol_binary_request_set protocol_binary_request_add;
  typedef protocol_binary_request_set protocol_binary_request_replace;

  /**
   * Definition of the packet returned by set, add and replace
   * See section 4.4
   */
  typedef protocol_binary_response_no_extras protocol_binary_response_set;
  typedef protocol_binary_response_no_extras protocol_binary_response_add;
  typedef protocol_binary_response_no_extras protocol_binary_response_replace;

  /**
   * Definition of the noop packet
   * See section 4.5
   */
  typedef protocol_binary_request_no_extras protocol_binary_request_noop;

  /**
   * Definition of the packet returned by the noop command
   * See section 4.5
   */
  typedef protocol_binary_response_no_extras protocol_binary_response_nnoop;

  /**
   * Definition of the structure used by the increment and decrement
   * command.
   * See section 4.6
   */
  typedef union {
    struct {
      protocol_binary_request_header header;
      struct {
    uint64_t delta;
    uint64_t initial;
    uint32_t expiration;
      } body;
    } message;
    uint8_t bytes[sizeof(protocol_binary_request_header) + 20];
  } protocol_binary_request_incr;
  typedef protocol_binary_request_incr protocol_binary_request_decr;

  /**
   * Definition of the response from an incr or decr command
   * command.
   * See section 4.6
   */
  typedef union {
    struct {
      protocol_binary_response_header header;
      struct {
    uint64_t value;
      } body;
    } message;
    uint8_t bytes[sizeof(protocol_binary_response_header) + 8];
  } protocol_binary_response_incr;
  typedef protocol_binary_response_incr protocol_binary_response_decr;

  /**
   * Definition of the quit
   * See section 4.7
   */
  typedef protocol_binary_request_no_extras protocol_binary_request_quit;

  /**
   * Definition of the packet returned by the quit command
   * See section 4.7
   */
  typedef protocol_binary_response_no_extras protocol_binary_response_quit;

  /**
   * Definition of the packet used by append and prepend command
   * See section 4.8
   */
  typedef protocol_binary_request_no_extras protocol_binary_request_append;
  typedef protocol_binary_request_no_extras protocol_binary_request_prepend;

  /**
   * Definition of the packet returned from a successful append or prepend
   * See section 4.8
   */
  typedef protocol_binary_response_no_extras protocol_binary_response_append;
  typedef protocol_binary_response_no_extras protocol_binary_response_prepend;

#ifdef __cplusplus
}
#endif
#endif /* PROTOCOL_BINARY_H */