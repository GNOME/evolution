/*
 * e-summary-factory.c: Executive Summary Factory.
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors:  Iain Holmes <iain@ximian.com>
 */

#ifndef __E_SUMMARY_FACTORY_H__
#define __E_SUMMARY_FACTORY_H__

BonoboControl *e_summary_factory_new_control (const char *uri,
					      const GNOME_Evolution_Shell shell);

#endif
