/*
 * lexer.c: Reads in the .calendar files
 */
#include <stdio.h>
#include <glib.h>
#include "cal_struct.h"


#define opener "["
#define closer "]"
#define VersionMajor 2

GList *eventlist;

int skip_chars(FILE *fp, char *terminator)
{
	int c;
	int cnt;

	cnt = 0;
	while( (c = fgetc(fp)) != EOF) {
		if (c == terminator[cnt]) {
			cnt++;
			if (terminator[cnt] == '\0')
				return TRUE;
		} else
			cnt = 0;
	}
	return FALSE;
}

int peek_char(FILE *fp, char *c)
{
	if ( ((*c) = fgetc(fp)) != EOF) {
		ungetc((*c), fp);
		return TRUE;
	} else	
		return FALSE;
}

int skip_whitespace(FILE *fp)
{
	int c;

	while( (c = fgetc(fp)) != EOF)
		if (!isspace(c)) {
			ungetc(c, fp);
			return TRUE;
		}
	return FALSE;
}

int get_until(FILE *fp, char terminator, char *buf) 
{
	int c;

	while( (c = fgetc(fp)) != EOF) {
		if (c == terminator)
			return TRUE;
		*buf = (char)c;
		buf++;
	}
	return FALSE;
}

int get_number(FILE *fp, int *x) 
{
	char buf[50];
	int c;
	int cnt;

	cnt = 0;
	buf[cnt] = '\0';
	while( (c= fgetc(fp)) != EOF) {
	   if (!isdigit(c)) {
	   	ungetc(c, fp);
	   	*x = atoi(buf);
	   	return TRUE;
	   }
	   buf[cnt++] = (char)c;
	   buf[cnt] = '\0';
	}
	*x = atoi(buf);
	return FALSE;
}	

int get_string(FILE *fp, char *string)
{
	int c;
	int cnt;

	cnt = 0;
	while ( (c = fgetc(fp)) != EOF) {
		if (c == closer[0]) {
			string[cnt] = '\0';
			ungetc((char)c, fp);
			return TRUE;
		}
		string[cnt++] = (char)c;
	}	
	return FALSE;
}

int getid(FILE *fp, char *string)
{
	int c;
	int cnt;

	cnt = 0;
	while( (c =fgetc(fp)) != EOF) {
		if (isalnum(c)) 
			string[cnt++] = (char)c;
		else {
			string[cnt] = '\0';
			return TRUE;
		}
	}
	string[cnt] = '\0';
	return FALSE;
}

int parse_appointment(FILE *fp, char keyword[])
{
	char buf[50];
	int x,y,c;
	struct event *ptr;

	ptr = (struct event*)alloc(sizeof(struct event));
	if (strcmp(keyword, "Start") == 0) {
		if ( ! skip_whitespace(fp) || ! get_number(fp, &x) ) {
			g_error("Unable to get start time");
			return FALSE;
		}
		g_print ("Appointment start = %02d:%02d\n", x/60, x % 60);
		sprintf(ptr->start.time, "%d", x);
		return TRUE;
	}

	if (strcmp(keyword, "Length") == 0) {
		if ( ! skip_whitespace(fp) || ! get_number(fp, &x) ) {
			g_error("Unable to get length");
			return FALSE;
		}
		g_print ("Appointment length = %d\n", x);
		sprintf(ptr->end.time, "%d", x);
		return TRUE;
	}

	if (strcmp(keyword, "Alarms") == 0) {
		while(TRUE) {
			skip_whitespace(fp);
			if (!peek_char(fp, (char*)&c)) {
				g_error("Cannot read alarm list");
				return FALSE;
			}
			if (!isdigit(c))
				break;

			if (! get_number(fp, &x))
				return FALSE;

			g_print("New alarm %d\n", x);
		}
		return TRUE;
	}

	g_print("Unknown keyword %s\n", keyword);
	return FALSE;
}
			
int parse_item(FILE *fp, char keyword[]) 
{
	char buf[50];
	int x, y, c;

	if (strcmp(keyword, "Remind") == 0) {
		if (! skip_whitespace(fp) || ! get_number(fp, &x)) {
			g_error("Cannot get remind level");
			return FALSE;
		}
		g_print("Remind level = %d\n", x);
		return TRUE;
	}

	if (strcmp(keyword, "Owner") == 0) {
		if (!get_string(fp, buf)) {
			g_error("Cannot get owner information");
			return FALSE;
		}
		g_print("Owner = %s\n", buf);
		return TRUE;
	}

	if (strcmp(keyword, "Uid") == 0) {
		if (!skip_whitespace(fp) || !get_until(fp, *closer, buf)) {
			g_error("Cannot get unique ID");
			return FALSE;
		}
		g_print("UID = %s\n", buf);
		return TRUE;
	}

	if (strcmp(keyword, "Contents") == 0) {
		if (!get_string(fp, buf)) {
			g_error("Cannot get item text");
			return FALSE;
		}
		g_print("Contents = %s\n", buf);
		return TRUE;
	}

	if (strcmp(keyword, "Text") == 0) {
		if (! skip_whitespace(fp) || ! get_number(fp, &x) ||
		    (x < 0) || ! skip_whitespace(fp) || ! skip_chars(fp, opener) ) {
			g_error("Cannot get item text");
			return FALSE;
		}
		y = 0;
		while(y < x) {
			if ( (c = fgetc(fp)) == EOF) {
				g_error("Short item text");
				return FALSE;
			}
			buf[y++] = (char)c;
		}
		buf[y] = '\0';
		g_print("Text = %s\n", buf);
		return TRUE;
	}
	
	if (strcmp(keyword, "Dates") == 0) {
		if (! get_string(fp, buf)) {
			g_error("Cannot get date");
			return FALSE;
		}
		g_print("Date = %s\n", buf);
		return TRUE;
	}

	if (strcmp(keyword, "Deleted") == 0) {
		if (! skip_whitespace(fp) || ! get_number(fp, &x)) {
			g_error("Cannot get deleted day");
			return FALSE;
		}
		g_print("%d/", x);
		if (! skip_whitespace(fp) || ! get_number(fp, &x)) {
			g_error("Cannot get deleted month");
			return FALSE;
		}
		g_print("%d/", x);
		if (! skip_whitespace(fp) || ! get_number(fp, &x)) {
			g_error("Cannot get deleted year");
			return FALSE;
		}
		g_print("%d\n", x);
		return TRUE;
	}

	if (strcmp(keyword, "Hilite") == 0) {
		if (! get_string(fp, buf) ) {
			g_error("Cannot get hilite data");
			return FALSE;
		}
		g_print("Hilite = %s\n", buf);
		return  TRUE;
	}

	if (strcmp(keyword, "Todo") == 0) {
		g_print("Todo\n");
		return TRUE;
	}


	if (strcmp(keyword, "Done") == 0) {
		g_print("Done\n");
		return TRUE;
	}

	return FALSE;
}

void parse_ical_file(char const *file)
{
	FILE *fp;
	int finished;
	char keyword[50];
	int file_major, file_minor;
	char c;
	int item_type;
	int incomplete_item;


	if ( (fp = fopen(file, "r")) == NULL) {
		g_error("couldn't open file");
		return;
	}

	finished = FALSE;

	if (!skip_whitespace(fp))
		return;

	if (! skip_chars(fp, "Calendar") || ! skip_whitespace(fp) ) {
		g_error("unable to find calendar file");
		fclose(fp);
		return;
	}

	if (! skip_chars(fp, opener) || ! skip_chars(fp, "v") ) {
		g_error("Unable to get version line");
		fclose(fp);
		return;
	}
	if (! get_number(fp, &file_major) || ! (file_major >=0) || (file_major > VersionMajor)) {
		g_error("Missing/bad major version");
		fclose(fp);
		return;
	}

	if (! skip_chars(fp, ".") || ! get_number(fp, &file_minor) ||
	    ! skip_chars(fp, "]") || ! skip_whitespace(fp) ) {
		g_error("Missing  minor version");
		fclose(fp);
		return;
	}
	if (file_minor > 0) {
		g_error("Bad  minor version");
		fclose(fp);
		return;
	}

	while(TRUE) {
		g_print("----------------------------------------\n");
		item_type= 0;
		skip_whitespace(fp);
		if (! getid(fp,keyword) || ! skip_whitespace(fp) ||
		    ! skip_chars(fp, opener) || ! skip_whitespace(fp) ) {
			fclose(fp);
			return;
		}

		if (strcmp(keyword, "Appt") == 0) {
			g_print("New Appointment\n");
			item_type = 1;

		} else if (strcmp(keyword, "Note") == 0) {
			g_print("New Note\n");
			item_type = 2;
		} else 
			g_print("New ??? (%s)\n", keyword);

		incomplete_item = TRUE;
		while(incomplete_item) {
			if (! skip_whitespace(fp) || ! peek_char(fp, &c)) {
				g_warning("Incomplete item\n");
				fclose(fp);
				return;
			}
			if (c == closer[0]) {
				(void)fgetc(fp);
				g_print("done!\n");
				incomplete_item = FALSE;
				break;
			}
		
			if (! getid(fp,keyword) || ! skip_whitespace(fp) ||
			    ! skip_chars(fp, opener) ) {
				g_error("Error reading item property name");
				fclose(fp);
				return;
			}
			if ( ! parse_item(fp, keyword) && ! parse_appointment(fp, keyword) ) {
				g_warning("Unable to parse line\n");
				fclose(fp);
				return;
			}
			if ( ! skip_whitespace(fp) || ! skip_chars(fp, closer)) {
				g_error("Error reading item property");
				fclose(fp);
				return;
			}
		} /* while */
	} /* while */
}
		
	


int main(int argc, char *argv[])
{

	eventlist = g_list_alloc();
	parse_ical_file("/home/csmall/.calendar");
	return 0;
}

