/**
 * Implementation of the 'ls' subcommand.
 */

#include <stdbool.h>
#include <string.h>

#include "json.h"
#include "MQTTClient.h"

#include "qth_client.h"


void print_ls_short(json_object *obj) {
	json_object_object_foreach(obj, topic, value) {
		(void)value;  // Unused
		
		// Check if topic is a directory or non-directory
		bool is_directory = false;
		bool is_non_directory = false;
		const char **behaviours = qth_subdirectory_get_behaviours(obj, topic);
		const char **p = behaviours;
		while (*p) {
			if (strcmp(*p, "DIRECTORY") == 0) {
				is_directory = true;
			} else {
				is_non_directory = true;
			}
			p++;
		}
		free(behaviours);
		
		
		// Print accordingly
		if (is_directory) {
			printf("%s/\n", topic);
		}
		if (is_non_directory) {
			printf("%s\n", topic);
		}
	}
}

void print_ls_long(json_object *obj) {
	json_object_object_foreach(obj, topic, value) {
		(void)value;
		const char **behaviours = qth_subdirectory_get_behaviours(obj, topic);
		const char **p = behaviours;
		while (*p) {
			if (strcmp(*p, "DIRECTORY") == 0) {
				printf("%s\t%s/\n", *p, topic);
			} else {
				printf("%s\t%s\n", *p, topic);
			}
			
			p++;
		}
		free(behaviours);
	}
}

void print_ls_json(const char *json, json_format_t json_format) {
	char *formatted = json_to_format(json, json_format);
	printf("%s\n", formatted);
	free(formatted);
}

/**
 * Implements the 'ls' command.
 */
int cmd_ls(MQTTClient *mqtt_client,
           const char *path,
           int meta_timeout,
           bool ls_recursive,
           ls_format_t ls_format,
           json_format_t json_format) {
	if (ls_recursive) {
		if (path[0] == '\0') {
			printf("[root]:\n");
		} else {
			printf("%s:\n", path);
		}
	}
	
	char *dir;
	char *err = qth_get_directory(mqtt_client, path, &dir, meta_timeout);
	if (err) {
		fprintf(stderr, "Error: %s\n", err);
		free(err);
		return 1;
	} else if (!dir) {
		// Should not happen.
		fprintf(stderr, "Error: Directory not found (dir is NULL)\n");
		return 1;
	} else {
		// NB: the JSON string has been verified as a valid directory listing by
		// qth_get_directory.
		json_object *obj = json_tokener_parse(dir);
		
		// Show this directory
		switch (ls_format) {
			case LS_FORMAT_SHORT:
				print_ls_short(obj);
				break;
			
			case LS_FORMAT_LONG:
				print_ls_long(obj);
				break;
			
			case LS_FORMAT_JSON:
				print_ls_json(dir, json_format);
				break;
		}
		
		// Recurse
		if (ls_recursive) {
			json_object_object_foreach(obj, part, value) {
				(void)value;
				if (qth_subdirectory_has_behaviour(obj, part, "DIRECTORY")) {
					// Create subdir path
					size_t path_len = strlen(path);
					size_t part_len = strlen(part);
					char *subpath = alloca(path_len + part_len + 1 + 1);
					strcpy(subpath, path);
					strcpy(subpath + path_len, part);
					subpath[path_len + part_len] = '/';
					subpath[path_len + part_len + 1] = '\0';
					
					// Recurse
					printf("\n");
					int state = cmd_ls(mqtt_client, subpath, meta_timeout, ls_recursive,
					                   ls_format, json_format);
					if (state != 0) {
						return state;
					}
				}
			}
		}
		
		json_object_put(obj);
	}
	
	free(dir);
	
	return 0;
}
