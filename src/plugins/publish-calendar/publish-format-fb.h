/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: David Trowbridge <trowbrds@cs.colorado.edu>
 */

#include <libecal/libecal.h>

#include "publish-location.h"

#ifndef PUBLISH_FORMAT_FB_H
#define PUBLISH_FORMAT_FB_H

void publish_calendar_as_fb (GOutputStream *stream, EPublishUri *uri, GError **error);

#endif
