/*
 * my-evolution-html.h: HTML as a #define
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * Authors:  Jakub Steiner  <jimmac@ximian.com>
 *           Iain Holmes  <iain@ximian.com>
 */

#ifdef NOT_ETTORE
#define HTML_1 "<html><head></head><body background=\"bcg.png\" "\
"bgcolor=\"white\" link=\"#314e6c\" alink=\"black\" vlink=\"#314e6c\">"\
"<img src=\"myevo.png\" alt=\"My Evolution\" width=\"200\" height=\"31\" border=\"0\"><br>"\
"<img src=\"empty.gif\" alt=\"\" width=\"10\" height=\"20\">"\
"<table border=\"0\" cellspacing=\"0\" cellpadding=\"0\" width=\"100%\">"\
"<tbody><tr>"
#else
#define HTML_1 "<html><head></head><body background=\"bcg.png\" "\
"bgcolor=\"white\" link=\"#314e6c\" alink=\"black\" vlink=\"#314e6c\">"\
"<img src=\"empty.gif\" alt=\"\" width=\"10\" height=\"20\">"\
"<table border=\"0\" numcols=\"4\" cellspacing=\"0\" cellpadding=\"0\" width=\"100%\">"\
"<tbody><tr>"
#endif

/* Needs a stringified date */
#define HTML_2 "<td align=\"Right\" colspan=\"4\"><b>%s</b><br> <img src=\"empty.gif\" width=\"1\" height=\"3\"></td>"

#define HTML_3 "</tr><tr><td colspan=\"4\" bgcolor=\"#000000\"><img src=\"empty.gif\" width=\"1\" height=\"1\"></td></tr>"\
"<tr valign=\"Top\">" \
"<td width=\"100%\">"\

/* Weather stuff goes here */

/* RDF Stuff goes here */

#define HTML_4 "</td><td>&#160;</td><td width=\"1\" bgcolor=\"#000000\"><img src=\"empty.gif\" width=\"1\" height=\"1\"></td>"\
"<td width=\"0\" background=\"pattern.png\">"

/* Mail stuff ici s'il vous plait */

/* And then the calendar stuff */

#ifdef NOT_ETTORE
#define HTML_5 "<p align=\"Center\"><img src=\"evologo-big.png\" width=\"200\" height=\"216\" alt=\"\"></p><p>"\
"<img src=\"empty.gif\" alt=\"\" width=\"290\" height=\"1\"></p></td>"\
"<tr bgcolor=\"#000000\"><td colspan=\"\"><img src=\"empty.gif\" alt=\"\" width=\"10\" height=\"20\"></td></tr>"\
"</tbody></table></body></html>"
#else
#define HTML_5 "<img src=\"empty.gif\" alt=\"\" width=\"290\" height=\"1\"></p></td>"\
"<tr bgcolor=\"#000000\"><td colspan=\"5\"><img src=\"empty.gif\" alt=\"\" width=\"10\" height=\"1\"></td></tr>"\
"</tbody></table></body></html>"
#endif
