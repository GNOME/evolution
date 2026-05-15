/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Jon Trowbridge <trow@ximian.com>
 * SPDX-FileContributor: Chris Toshok <toshok@ximian.com>
 */

#ifndef __EAB_CONTACT_COMPARE_H__
#define __EAB_CONTACT_COMPARE_H__

#include <libebook/libebook.h>

typedef enum {
	EAB_CONTACT_MATCH_NOT_APPLICABLE = 0,
	EAB_CONTACT_MATCH_NONE = 1,
	EAB_CONTACT_MATCH_VAGUE = 2,
	EAB_CONTACT_MATCH_PARTIAL = 3,
	EAB_CONTACT_MATCH_EXACT = 4
} EABContactMatchType;

typedef enum {
	EAB_CONTACT_MATCH_PART_NOT_APPLICABLE = -1,
	EAB_CONTACT_MATCH_PART_NONE = 0,
	EAB_CONTACT_MATCH_PART_GIVEN_NAME = 1 << 0,
	EAB_CONTACT_MATCH_PART_ADDITIONAL_NAME = 1 << 2,
	EAB_CONTACT_MATCH_PART_FAMILY_NAME = 1 << 3
} EABContactMatchPart;

typedef void	(*EABContactMatchQueryCallback)	(EContact *contact,
						 EContact *match,
						 EABContactMatchType type,
						 gpointer closure);

EABContactMatchType
		eab_contact_compare_name_to_string
						(EContact *contact,
						 const gchar *str);

EABContactMatchType
		eab_contact_compare_name_to_string_full
						(EContact *contact,
						 const gchar *str,
						 gboolean allow_partial_matches,
						 gint *matched_parts,
						 EABContactMatchPart *first_matched_part,
						 gint *matched_character_count);

EABContactMatchType
		eab_contact_compare_file_as	(EContact *contact1,
						 EContact *contact2);
EABContactMatchType
		eab_contact_compare_name	(EContact *contact1,
						 EContact *contact2);
EABContactMatchType
		eab_contact_compare_nickname	(EContact *contact1,
						 EContact *contact2);
EABContactMatchType
		eab_contact_compare_email	(EContact *contact1,
						 EContact *contact2);
EABContactMatchType
		eab_contact_compare_address	(EContact *contact1,
						 EContact *contact2);
EABContactMatchType
		eab_contact_compare_telephone	(EContact *contact1,
						 EContact *contact2);

EABContactMatchType
		eab_contact_compare		(EContact *contact1,
						 EContact *contact2);

void		eab_contact_locate_match	(ESourceRegistry *registry,
						 EContact *contact,
						 EABContactMatchQueryCallback cb,
						 gpointer closure);
void		eab_contact_locate_match_full	(ESourceRegistry *registry,
						 EBookClient *book_client,
						 EContact *contact,
						 GList *avoid,
						 EABContactMatchQueryCallback cb,
						 gpointer closure);

#endif /* __E_CONTACT_COMPARE_H__ */

