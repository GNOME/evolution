/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - ToDo Conduit
 *
 * Copyright (C) 2007 Novell, Inc.
 *
 * Authors: Tom Billet <mouse256@ulyssis.org>
 *          Nathan Owens <pianocomp81@yahoo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <string.h>

#include <libedataserver/e-categories.h>
#include <e-util/e-pilot-util.h>
#include <pi-appinfo.h>
#include <glib.h>

#include "libecalendar-common-conduit.h"

/*make debugging possible if it's required for a conduit */
#define LOG(x)
#ifdef DEBUG_CALCONDUIT
#define LOG(x) x
#endif 
#ifdef DEBUG_MEMOCONDUIT
#define LOG(x) x
#endif
#ifdef DEBUG_TODOCONDUIT
#define LOG(x) x
#endif 
#ifdef DEBUG_CONDUIT
#define LOG(x) x
#endif 

/*
 * Adds a category to the category app info structure (name and ID),
 * sets category->renamed[i] to true if possible to rename.
 * 
 * This will be packed and written to the app info block during post_sync.
 * 
 * NOTE: cat_to_add MUST be in PCHAR format. Evolution stores categories
 *       in UTF-8 format. A conversion must take place before calling
 *       this function (see e_pilot_utf8_to_pchar() in e-pilot-util.c)
 */
int
e_pilot_add_category_if_possible(char *cat_to_add, struct CategoryAppInfo *category)
{
	int i, j;
	int retval = 0; /* 0 is the Unfiled category */
	LOG(g_message("e_pilot_add_category_if_possible\n"));
	
	for(i=0; i<PILOT_MAX_CATEGORIES; i++){
		/* if strlen is 0, then the category is empty
		   the PalmOS doesn't let 0-length strings for
		   categories */
		if(strlen(category->name[i]) == 0){
			int cat_to_add_len;
			
			cat_to_add_len = strlen(cat_to_add);
			retval = i;

			if(cat_to_add_len > 15){
				/* Have to truncate the category name */
				cat_to_add_len = 15;
			}
			
			/* only 15 characters for category, 16th is
			 * '\0' can't do direct mem transfer due to
			 * declaration type
			 */
			for(j=0; j<cat_to_add_len; j++){
				category->name[i][j] = cat_to_add[j];
			}

			for(j=cat_to_add_len; j<16; j++) {
				category->name[i][j] = '\0';
			}
			
			//find a desktop id that is not in use between 128 and 255
			int desktopUniqueID;
			for (desktopUniqueID = 128; desktopUniqueID <= 255; desktopUniqueID++) {
				int found = 0;
				for(j=0; j<PILOT_MAX_CATEGORIES; j++){
					if (category->ID[i] == desktopUniqueID) {
						found = 1;
					}
				}
				if (found == 0) {
					break;
				}
				if (desktopUniqueID == 255) {
					LOG (g_warning ("*** no more categories available on PC ***"));
				}
			}
			category->ID[i] = desktopUniqueID;
			
			category->renamed[i] = TRUE;
			
			break;
		}
	}
	
	if(retval == 0){
		LOG (g_warning ("*** not adding category - category list already full ***"));
	}
	
	return retval;
}

/*
 *conversion from an evolution category to a palm category
 */
void e_pilot_local_category_to_remote(int * pilotCategory, ECalComponent *comp, struct CategoryAppInfo *category)
{
	GSList *c_list = NULL;
	char * category_string;
	int i;
	e_cal_component_get_categories_list (comp, &c_list);
	if (c_list) {
		//list != 0, so at least 1 category is assigned
		category_string = e_pilot_utf8_to_pchar((const char *)c_list->data);
		if (c_list->next != 0) {
			LOG (g_message ("Note: item has more categories in evolution, first chosen"));
		}
		i=1;
		while (1) {
			if (strcmp(category_string,category->name[i]) == 0) {
				*pilotCategory = i;
				break;
			}
			i++;
			if (i == PILOT_MAX_CATEGORIES) {
				/* category not available on palm, try to create it */
				*pilotCategory = e_pilot_add_category_if_possible(category_string,category);
				break;
			}
		}
		e_cal_component_free_categories_list(c_list);
		c_list = NULL;
	} else {
		*pilotCategory = 0;
	}
	/*end category*/
}

/*
 *conversion from a palm category to an evolution category
 */
void e_pilot_remote_category_to_local(int pilotCategory, ECalComponent *comp, struct CategoryAppInfo *category)
{
	char *category_string = NULL;

	if (pilotCategory != 0) {
		/* pda has category assigned */
		category_string = e_pilot_utf8_from_pchar(category->name[pilotCategory]);

		LOG(g_message("Category: %s\n", category_string));

		/* TODO The calendar editor page and search bar are not updated until a restart of the evolution client */
		if(e_categories_exist(category_string) == FALSE){
			/* add if it doesn't exist */
			LOG(g_message("Category created on pc\n"));
			e_categories_add(category_string, NULL, NULL, TRUE);
		}
	}

	/* store the data on in evolution */
	if (category_string == NULL) {
		/* note: this space is needed to make sure evolution clears the category */
		e_cal_component_set_categories (comp, " "); 
	} 
	else {

		/* Since only the first category is synced with the PDA, add the PDA's
		 * category to the beginning of the category list */
		GSList *c_list = NULL;
		GSList *newcat_in_list;
		e_cal_component_get_categories_list (comp, &c_list);

		/* remove old item from list so we don't have duplicate entries */		
		newcat_in_list = g_slist_find_custom(c_list, category_string, (GCompareFunc)strcmp);
		if(newcat_in_list != NULL)
		{
			c_list = g_slist_remove(c_list, newcat_in_list->data);
		}

		c_list = g_slist_prepend(c_list, category_string);
		e_cal_component_set_categories_list (comp, c_list);
		e_cal_component_free_categories_list(c_list);
	}
}
