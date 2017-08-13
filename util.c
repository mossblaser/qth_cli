#include <string.h>

#include "qth_client.h"


/**
 * Make a (malloced) copy of a string.
 */
char *alloced_copy(const char *str) {
	char *str_out = malloc(strlen(str) + 1);
	strcpy(str_out, str);
	return str_out;
}

/**
 * Make a (malloced), null-terminated copy of a lengthed string.
 */
char *alloced_copyn(const char *str, size_t len) {
	char *str_out = malloc(len + 1);
	memcpy(str_out, str, len);
	str_out[len] = '\0';
	return str_out;
}


/**
 * Allocate a new string which contains the concatenation of two strings.
 */
char *alloced_cat(const char *a, const char *b) {
	size_t a_len = strlen(a);
	size_t b_len = strlen(b);
	char *str_out = malloc(a_len + b_len + 1);
	strcpy(str_out, a);
	strcpy(str_out + a_len, b);
	return str_out;
}

