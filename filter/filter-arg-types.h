/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *
 *  Implementations of the filter-args.
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

#ifndef _FILTER_ARG_TYPES_H
#define _FILTER_ARG_TYPES_H

#include "filter-arg.h"

/* An Address */
#define FILTER_ARG_ADDRESS(obj)         GTK_CHECK_CAST (obj, filter_arg_address_get_type (), FilterArgAddress)
#define FILTER_ARG_ADDRESS_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, filter_arg_address_get_type (), FilterArgAddressClass)
#define IS_FILTER_ARG_ADDRESS(obj)      GTK_CHECK_TYPE (obj, filter_arg_address_get_type ())

typedef struct _FilterArgAddress      FilterArgAddress;
typedef struct _FilterArgAddressClass FilterArgAddressClass;

struct _FilterArgAddress {
	FilterArg arg;
};

struct _FilterArgAddressClass {
	FilterArgClass parent_class;
};

struct filter_arg_address {
	char *name;
	char *email;
};

guint		 filter_arg_address_get_type	(void);
FilterArg	*filter_arg_address_new	(char *name);
void             filter_arg_address_add(FilterArg *, char *name, char *email);
void             filter_arg_address_remove(FilterArg *, char *name, char *email);

/* A simple String */
#define FILTER_ARG_STRING(obj)         GTK_CHECK_CAST (obj, filter_arg_string_get_type (), FilterArgString)
#define FILTER_ARG_STRING_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, filter_arg_string_get_type (), FilterArgStringClass)
#define IS_FILTER_ARG_STRING(obj)      GTK_CHECK_TYPE (obj, filter_arg_string_get_type ())

typedef struct _FilterArgString      FilterArgString;
typedef struct _FilterArgStringClass FilterArgStringClass;

struct _FilterArgString {
	FilterArg arg;

	/* Name/property to save/load to xml */
	/* char *xmlname; */
	/* char *xmlprop; */
};

struct _FilterArgStringClass {
	FilterArgClass parent_class;
};

guint		 filter_arg_string_get_type	(void);
FilterArg	*filter_arg_string_new	(char *name);
void             filter_arg_string_add(FilterArg *, char *name);
void             filter_arg_string_remove(FilterArg *, char *name);

/* A Folder, subclass of a string */
#define FILTER_ARG_FOLDER(obj)         GTK_CHECK_CAST (obj, filter_arg_folder_get_type (), FilterArgFolder)
#define FILTER_ARG_FOLDER_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, filter_arg_folder_get_type (), FilterArgFolderClass)
#define IS_FILTER_ARG_FOLDER(obj)      GTK_CHECK_TYPE (obj, filter_arg_folder_get_type ())

typedef struct _FilterArgFolder      FilterArgFolder;
typedef struct _FilterArgFolderClass FilterArgFolderClass;

struct _FilterArgFolder {
	FilterArgString arg;
};

struct _FilterArgFolderClass {
	FilterArgStringClass parent_class;
};

guint		 filter_arg_folder_get_type	(void);
FilterArg	*filter_arg_folder_new	(char *name);
void             filter_arg_folder_add(FilterArg *, char *name);
void             filter_arg_folder_remove(FilterArg *, char *name);

#endif /* ! _FILTER_ARG_TYPES_H */

