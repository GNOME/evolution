/*
 * metar.h
 */

#ifndef __METAR_H__
#define __METAR_H__

#define TIME_RE_STR  "^([0-9]{6})Z$"
#define WIND_RE_STR  "^(([0-9]{3})|VRB)([0-9]?[0-9]{2})(G[0-9]?[0-9]{2})?KT$"
#define VIS_RE_STR   "^(([0-9]?[0-9])|(M?1/[0-9]?[0-9]))SM$"
#define CLOUD_RE_STR "^(CLR|BKN|SCT|FEW|OVC)([0-9]{3})?$"
#define TEMP_RE_STR  "^(M?[0-9][0-9])/(M?[0-9][0-9])$"
#define PRES_RE_STR  "^(A|Q)([0-9]{4})$"
#define COND_RE_STR  "^(-|\\+)?(VC|MI|BC|PR|TS|BL|SH|DR|FZ)?(DZ|RA|SN|SG|IC|PE|GR|GS|UP|BR|FG|FU|VA|SA|HZ|PY|DU|SQ|SS|DS|PO|\\+?FC)$"

enum {
	TIME_RE,
	WIND_RE,
	VIS_RE,
	CLOUD_RE,
	TEMP_RE,
	PRES_RE,
	COND_RE,
	RE_NUM
};

const char *weather_sky_string (Weather *w);
char *weather_temp_string (Weather *w);
const char *weather_conditions_string (Weather *w);

void metar_init_re (void);
gboolean metar_tok_time (char *token,
			 Weather *w);
gboolean metar_tok_wind (char *tokp, 
			 Weather *w);
gboolean metar_tok_vis (char *tokp, 
			Weather *w);
gboolean metar_tok_cloud (char *tokp, 
			  Weather *w);
gboolean metar_tok_pres (char *tokp, 
			 Weather *w);
gboolean metar_tok_temp (char *tokp, 
			 Weather *w);
gboolean metar_tok_cond (char *tokp,
			 Weather *w);
#endif
