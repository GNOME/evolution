/*
 *  Copyright (C) 2001 Ximian Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _CAMEL_SEARCH_PRIVATE_H
#define _CAMEL_SEARCH_PRIVATE_H

typedef enum {
	CAMEL_SEARCH_MATCH_START = 1<<0,
	CAMEL_SEARCH_MATCH_END = 1<<1,
	CAMEL_SEARCH_MATCH_REGEX = 1<<2, /* disables the first 2 */
	CAMEL_SEARCH_MATCH_ICASE = 1<<3,
} camel_search_flags_t;

typedef enum {
	CAMEL_SEARCH_MATCH_EXACT,
	CAMEL_SEARCH_MATCH_CONTAINS,
	CAMEL_SEARCH_MATCH_STARTS,
	CAMEL_SEARCH_MATCH_ENDS,
	CAMEL_SEARCH_MATCH_SOUNDEX,
} camel_search_match_t;

/* builds a regex that represents a string search */
int camel_search_build_match_regex(regex_t *pattern, camel_search_flags_t type, int argc, struct _ESExpResult **argv, CamelException *ex);
gboolean camel_search_message_body_contains(CamelDataWrapper *object, regex_t *pattern);

gboolean camel_search_header_match(const char *value, const char *match, camel_search_match_t how);
gboolean camel_search_header_soundex(const char *header, const char *match);

#endif /* ! _CAMEL_SEARCH_PRIVATE_H */
