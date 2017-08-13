/**
 * JSON parsing and validation routines.
 */

#include <stdbool.h>
#include <string.h>

#include "json.h"

#include "qth_client.h"


////////////////////////////////////////////////////////////////////////////////
// Utilities for annotating strings with errors
////////////////////////////////////////////////////////////////////////////////

typedef struct {
	// Index of first character of the line, which may be the null-terminator
	size_t line_start;
	
	// Index just after the last character in the line and any of its trailing
	// newlines.
	size_t line_end;
	
	// Number of characters into that line the target is
	size_t line_offset;
} point_in_text_t;

/**
 * Given a string, find the line and position within that line that a
 * particular character offset is at.
 */
point_in_text_t find_point_in_text(const char *lines, size_t offset) {
	point_in_text_t out;
	
	// Sanity check (NB as size_t is unsigned, this triggers for 'negative'
	// offsets too)
	if (offset >= strlen(lines)) {
		out.line_start = strlen(lines);
		out.line_end = out.line_start;
		out.line_offset = 0;
		return out;
	}
	
	const char *start_of_line = lines + offset;
	
	// If we've been asked to point at a newline, first move the cursor before
	// that newline
	while (start_of_line != lines && *start_of_line == '\n') {
		start_of_line--;
	}
	
	// Find the start of the line on which the offset lives
	while (start_of_line != lines && *start_of_line != '\n') {
		start_of_line--;
	}
	if (*start_of_line == '\n') {
		start_of_line++;
	}
	
	// Find the end of the line of the offset
	const char *end_of_line = lines + offset;
	bool found_newline = false;
	while (*end_of_line != '\0' && !(found_newline && *end_of_line != '\n')) {
		if (*end_of_line == '\n') {
			found_newline = true;
		}
		end_of_line++;
	}
	
	out.line_start = start_of_line - lines;
	out.line_offset = offset - out.line_start;
	out.line_end = end_of_line - lines;
	return out;
}

/**
 * Return a copy of str annotated with an arrow at the provided offset,
 * proceeded by a copy of the message. All trailing newlines in the message
 * after the annotated offset are discarded. The caller is responsible for
 * freeing the memory used by the string.
 */
char *annotate_error(const char *str, size_t offset, const char *message) {
	point_in_text_t p = find_point_in_text(str, offset);
	
	// The message, a newline, the string plus inner newline and arrow and a
	// null. May over-allocate by one byte as may need an extra byte to insert a
	// newline if the line being annotated doesn't end with a newline.
	size_t message_len = strlen(message);
	size_t str_len = strlen(str);
	size_t len = message_len + 1 + str_len + 1 + p.line_offset + 1 + 1 + 1;
	
	char *out = malloc(len);
	char *cur = out;
	
	// Add the message and newline
	strcpy(cur, message);
	cur += message_len;
	*(cur++) = '\n';
	
	// Add up-to and including the annotated line
	memcpy(cur, str, p.line_end);
	cur += p.line_end;
	
	// Add a newline if the annotated line doesn't end in a newline
	if (p.line_end == 0 || str[p.line_end - 1] != '\n') {
		*(cur++) = '\n';
	}
	
	// Add the arrow
	for (size_t i = 0; i < p.line_offset; i++) {
		*(cur++) = '-';
	}
	*(cur++) = '^';
	*(cur++) = '\n';
	
	// Rest of the string
	strcpy(cur, str + p.line_end);
	cur += strlen(str + p.line_end);
	
	// Remove trailing newlines (NB: will always terminate at least when it hits
	// the '^' in the arrow)
	while (*(--cur) == '\n') {
		*cur = '\0';
	}
	
	return out;
}

////////////////////////////////////////////////////////////////////////////////
// JSON utilities
////////////////////////////////////////////////////////////////////////////////

/**
 * Parse the supplied JSON string, returning a human-readable error message
 * if the string is not valid and NULL otherwise. The caller must free any
 * string returned. The parsed JSON is returned via the obj argument.
 */
char *json_parse(const char *str, int len, json_object **obj) {
	if (len < 0) {
		len = strlen(str);
	}
	
	// Parse the string and see what happens
	json_tokener *tokener = json_tokener_new();
	*obj = json_tokener_parse_ex(tokener, str, len);
	enum json_tokener_error err = json_tokener_get_error(tokener);
	
	const char *err_message = json_tokener_error_desc(err);
	size_t err_offset = tokener->char_offset;
	
	if (err == json_tokener_success && tokener->char_offset == len) {
		// Parsed the whole string with success, JSON is valid!
		json_tokener_free(tokener);
		return NULL;
	} else if (err == json_tokener_success) {
		// Some of the end of the string was not parsed
		err_message = "unexpected extra input";
	}
	json_tokener_free(tokener);
	
	return annotate_error(str, err_offset, err_message);
}

/**
 * Validate the supplied JSON string, returning a human-readable error message
 * if the string is not valid and NULL otherwise. The caller must free any
 * string returned.
 */
char *json_validate(const char *str, int len) {
	json_object *obj;
	char *err = json_parse(str, len, &obj);
	if (obj) {
		json_object_put(obj);
	}
	return err;
}

/**
 * Given a JSON string, return the same string formatted according to a JSON-C
 * formatting constant. The returned string is in malloc-managed memory and
 * must be freed by the caller.
 */
char *json_reformat(const char *in_str, int format) {
	json_object *json = json_tokener_parse(in_str);
	const char *one_liner = json_object_to_json_string_ext(json, format);
	
	// Copy into manually managed memory
	char *out = malloc(strlen(one_liner) + 1);
	strcpy(out, one_liner);
	
	json_object_put(json);
	
	return out;
}

/**
 * Given a JSON string, return the same string formatted in the relevant style.
 * The caller must free the allocated string with 'free' afterwards.
 */
char *json_to_format(const char *in_str, json_format_t json_format) {
	switch (json_format) {
		case JSON_FORMAT_SINGLE_LINE:
			return json_reformat(in_str, JSON_C_TO_STRING_PLAIN | JSON_C_TO_STRING_NOZERO);
		
		case JSON_FORMAT_PRETTY:
			return json_reformat(in_str, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_NOZERO);
		
		case JSON_FORMAT_VERBATIM:
			return alloced_copy(in_str);
		
		case JSON_FORMAT_QUIET:
			return alloced_copy("");
	}
	// Should not reach here!
	return NULL;
	
}
