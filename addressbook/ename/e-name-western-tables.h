#ifndef __E_NAME_WESTERN_TABLES_H__
#define __E_NAME_WESTERN_TABLES_H__

char *e_name_western_pfx_table[] = {

	/*
	 * English.
	 */
	"mister", "miss.", "mr.", "mrs.", "ms.",
	"miss", "mr", "mrs", "ms", "sir", 
	"professor", "prof.", "dr", "dr.", "doctor",
	"reverend", "president", "judge", "senator",
	"congressman", "congresswoman", "commander",
	"lieutenant", "colonel", "major", "general",  

	"the honorable", "the reverend", "his holiness",
	"his eminence",
	

	/*
	 * French.
	 */
	"monsieur", "mr.", "mademoiselle", "melle.",
	"madame", "mme.", "professeur",

	/*
	 * Spanish.
	 */
	"senor", "senora", "senorita",

	NULL};

char *e_name_western_sfx_table[]  = {

	/*
	 * English.
	 */
	"junior", "senior", "jr", "sr", "I", "II", "III", "IV", "V",
	"phd", "ms", "md", "esq", "esq.", "esquire",

	NULL};

char *e_name_western_twopart_sfx_table[] = {

	/*
	 * English.
	 */
	"the first", "the second", "the third",

	NULL};

char *e_name_western_complex_last_table[] = {"van", "von", "de", NULL};

#endif /* ! __E_NAME_WESTERN_TABLES_H__ */
