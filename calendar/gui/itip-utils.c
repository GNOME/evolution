/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * itip-utils.c
 *
 * Authors:
 *    Jesse Pavel <jpavel@helixcode.com>
 *
 * Copyright 2000, Helix Code, Inc.
 */

#include "itip-utils.h"

gchar *partstat_values[] = {
	"Needs action",
	"Accepted",
	"Declined",
	"Tentative",
	"Delegated",
	"Completed",
	"In Progress",
	"Unknown"
};

gchar *role_values[] = {
	"Chair",
	"Required Participant",
	"Optional Participant",
	"Non-Participant",
	"Other"
};



/* Note that I have to iterate and check myself because
   ical_property_get_xxx_parameter doesn't take into account the
   kind of parameter for which you wish to search! */
icalparameter *
get_icalparam_by_type (icalproperty *prop, icalparameter_kind kind)
{
	icalparameter *param;

	for (param = icalproperty_get_first_parameter (prop, ICAL_ANY_PARAMETER);
	     param != NULL && icalparameter_isa (param) != kind;
	     param = icalproperty_get_next_parameter (prop, ICAL_ANY_PARAMETER) );

	return param;
}


