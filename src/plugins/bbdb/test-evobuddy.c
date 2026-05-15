/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

void bbdb_sync_buddy_list (void);

gint
main (void)
{
	printf ("Syncing...\n");

	bbdb_sync_buddy_list ();

	printf ("Done!\n");

	return 0;
}
