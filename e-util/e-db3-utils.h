/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * db3 utils.
 *
 * Author:
 *   Chris Lahey <clahey@ximian.com>
 *
 * Copyright 2001, Ximian, Inc.
 */

#ifndef __E_DB3_UTILS_H__
#define __E_DB3_UTILS_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

int e_db3_utils_maybe_recover (const char *filename);
int e_db3_utils_upgrade_format (const char *filename);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! __E_DB3_UTILS_H__ */

