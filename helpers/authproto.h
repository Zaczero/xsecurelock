/*
 * Copyright 2014 Google Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AUTHPROTO_H
#define AUTHPROTO_H

#include <stddef.h>

// Packet format:
//
//   <ptype> <SPC> <len> <NEWLINE> <message> <NEWLINE>
//
// where
//
//   ptype = one of the below characters.
//   len = message length encoded in decimal ASCII.
//   message = len bytes that shall be shown to the user.
//
// By convention, uppercase packet types expect a reply and lowercase packet
// types are "terminal".

// PAM-to-user messages:
#define PTYPE_INFO_MESSAGE 'i'
#define PTYPE_ERROR_MESSAGE 'e'
#define PTYPE_PROMPT_LIKE_USERNAME 'U'
#define PTYPE_PROMPT_LIKE_PASSWORD 'P'
// Note: there's no specific message type for successful authentication or
// similar; the caller shall use the exit status of the helper only.

// User-to-PAM messages:
#define PTYPE_RESPONSE_LIKE_USERNAME 'u'
#define PTYPE_RESPONSE_LIKE_PASSWORD 'p'
#define PTYPE_RESPONSE_CANCELLED 'x'

/**
 * \brief Writes a packet in above form from a byte buffer.
 *
 * \param fd The file descriptor to write to.
 * \param type The packet type from above macros.
 * \param message The message bytes to include with the packet.
 * \param len The number of bytes in message.
 * \return Whether the full packet was written successfully.
 */
int WritePacketBytes(int fd, char type, const char *message, size_t len);

/**
 * \brief Writes a packet in above form.
 *
 * \param fd The file descriptor to write to.
 * \param type The packet type from above macros.
 * \param message The message to include with the packet (NUL-terminated).
 */
void WritePacket(int fd, char type, const char *message);

/**
 * \brief Reads a packet in above form.
 *
 * \param fd The file descriptor to write to.
 * \param message A pointer to store the message (will be locked in memory if
 *   possible).
 *   Will always be set if function returns nonzero; caller must free it.
 * \param eof_permitted If enabled, encountering EOF at the beginning will not
 *   count as an error but return 0 silently.
 * \return The packet type, or 0 if no packet has been read. Errors are logged.
 */
char ReadPacket(int fd, char **message, int eof_permitted);

#endif
