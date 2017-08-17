/**
 * Functions which implement parts of the Qth conventions used by the module
 */

#include <alloca.h>
#include <string.h>
#include <stdbool.h>

#include "MQTTClient.h"

#include "qth_client.h"

/**
 * Check whether a JSON object contains a valid Qth directory listing.
 */
bool qth_is_directory_listing(json_object *dir) {
	// Should be an object
	bool is_object = json_object_get_type(dir) == json_type_object;
	if (!is_object) {
		return false;
	}
	
	// Should be mapping from topics to arrays of objects containing 'behaviour'
	json_object_object_foreach(dir, topic, entry_list) {
		(void)topic;  // Unused
		
		if (json_object_get_type(entry_list) != json_type_array) {
			return false;
		}
		
		// Array should just contain objects with a 'behaviour'
		for (int i = 0; i < json_object_array_length(entry_list); i++) {
			json_object *entry = json_object_array_get_idx(entry_list, i);
			if (json_object_get_type(entry) != json_type_object) {
				return false;
			}
			
			// 'behaviour' should be a string
			json_object *behaviour;
			bool has_behaviour = json_object_object_get_ex(entry, "behaviour", &behaviour);
			if (!has_behaviour) {
				return false;
			}
			if (json_object_get_type(behaviour) != json_type_string) {
				return false;
			}
		}
	}
	
	return true;
}


/**
 * Return a NULL-terminated array of strings giving the behaviours associated
 * with a given subpath in an Qth directory listing. A null pointer will be
 * returned if the subpath doesn't exist or the directory listing is not valid.
 * The caller is responsible for freeing the returned array. The memory for the
 * strings are managed by the json_object provided.
 */
const char **qth_subdirectory_get_behaviours(json_object *dir, const char *subpath) {
	if (!qth_is_directory_listing(dir)) {
		// Malformed directory listing
		return NULL;
	}
	
	json_object *entries_list;
	if (!json_object_object_get_ex(dir, subpath, &entries_list)) {
		// Subpath does not exist
		return NULL;
	}
	
	int entries_list_len = json_object_array_length(entries_list);
	const char **behaviours = malloc(sizeof(const char *) * (entries_list_len + 1));
	
	for (int i = 0; i < entries_list_len; i++) {
		json_object *entry = json_object_array_get_idx(entries_list, i);
		json_object *behaviour;
		json_object_object_get_ex(entry, "behaviour", &behaviour);
		behaviours[i] = json_object_get_string(behaviour);
	}
	
	// Null terminate
	behaviours[entries_list_len] = NULL;
	
	return behaviours;
}


/**
 * Return true if a given directory listing contains a particular subpath with
 * the specified behaviour.
 */
bool qth_subdirectory_has_behaviour(json_object *dir,
                                    const char *subpath,
                                    const char *behaviour) {
	const char **behaviours = qth_subdirectory_get_behaviours(dir, subpath);
	if (!behaviours) {
		return false;
	}
	
	const char **p = behaviours;
	while (*p) {
		if (strcmp(*p, behaviour) == 0) {
			free(behaviours);
			return true;
		}
		p++;
	}
	
	free(behaviours);
	return false;
}


/**
 * Return the JSON representing the Qth directory listing for a given path via
 * the 'dir' argument. If the path does not exist or the MQTT connection fails
 * an error message is returned (which must be freed by the caller), otherwise
 * NULL is returned on success.
 *
 * Parameters
 * ----------
 * * client: The (connected) MQTT client
 * * path: The directory path to search for
 * * dir: Will be set to the received string containing the directory listing or
 *   NULL if the command failed.
 * * int meta_timeout: The number of ms to wait for a listing to arrive.
 */
char *qth_get_directory(MQTTClient *client, const char *path, char **dir, int meta_timeout) {
	*dir = NULL;
	
	// Count how many pieces does the path split into
	size_t depth = 1;
	for (const char *c = path; *c != '\0'; c++) {
		if (*c == '/') {
			depth++;
		}
	}
	
	// If the path is not a directory, fail
	size_t path_len = strlen(path);
	if (path_len > 0 && path[path_len - 1] != '/') {
		return alloced_copy("Path is not a valid directory name (must end in '/' or be empty).");
	}
	
	// Create an array with just the parts of the path. For example, for the path
	// foo/bar/baz, the parts would be 'foo', 'bar' and 'baz'. For the path
	// 'some/directory/' the parts would be 'some', 'directory' and ''.
	char **parts = alloca(sizeof(char *)* (depth-1));
	const char *cursor = path;
	for (size_t i = 0; i < depth-1; i++) {
		// Find the end of the current path segment.
		size_t part_len = 0;
		const char *part = cursor;
		while(*cursor != '\0' && *cursor != '/') {
			cursor++;
			part_len++;
		}
		cursor++;
		
		parts[i] = alloca(part_len + 1);
		memcpy(parts[i], part, part_len);
		parts[i][part_len] = '\0';
	}
	
	// Create an array of directory listing paths, for example the path
	// 'foo/bar/baz' will result in 'meta/ls/', 'meta/ls/foo/',
	// 'meta/ls/foo/bar/' and 'meta/ls/foo/bar/baz'.
	char **ls_paths = alloca(sizeof(char *) * depth);
	size_t last_path_len = 8;
	ls_paths[0] = "meta/ls/";
	for (size_t i = 0; i < depth-1; i++) {
		size_t part_len = strlen(parts[i]);
		ls_paths[i+1] = alloca(last_path_len + part_len + 2);
		strcpy(ls_paths[i+1], ls_paths[i]);
		strcpy(ls_paths[i+1] + last_path_len, parts[i]);
		
		*(ls_paths[i+1] + last_path_len + part_len) = '/';
		*(ls_paths[i+1] + last_path_len + part_len + 1) = '\0';
		last_path_len += part_len + 1;
	}
	
	// Subscribe to the directory listing of all of these parts since we need to
	// check every level of the tree to be sure the directory actually exists
	// (and isn't a stale property).
	int qos[depth];
	for (size_t i = 0; i < depth; i++) {
		qos[i] = 2;
	}
	int mqtt_err = MQTTClient_subscribeMany(client, depth, ls_paths, qos);
	if (mqtt_err != MQTTCLIENT_SUCCESS) {
		char *err = "Could not subscribe to directory listings.";
		char *err_out = malloc(strlen(err));
		strcpy(err_out, err);
		return err_out;
	}
	
	bool verified_parts[depth+1];
	size_t num_verified_parts = 0;
	for (size_t i = 0; i < depth; i++) {
		verified_parts[i] = false;
	}
	
	// Await responses, verifying each level of the path exists in the tree
	char *leaf_dir = NULL;
	char *topic = NULL;
	int topic_len;
	MQTTClient_message *message;
	while (num_verified_parts < depth) {
		int mqtt_err = MQTTClient_receive(client, &topic, &topic_len, &message, meta_timeout);
		if (mqtt_err != MQTTCLIENT_SUCCESS) {
			return alloced_copy("MQTT error while fetching directory listing.");
		} else if (topic == NULL) {
			return alloced_copy("Timeout while fetching directory listing. ");
		}
		
		// Check to see if the directory exists
		for (size_t i = 0; i < depth; i++) {
			if (strncmp(topic, ls_paths[i], topic_len) == 0) {
				// Parse the listing
				json_object *obj;
				char *json_err = json_parse(message->payload, message->payloadlen, &obj);
				if (json_err) {
					MQTTClient_unsubscribeMany(client, depth, ls_paths);
					
					char *err_out = alloced_cat("Couldn't parse directory listing: ", json_err);
					free(json_err);
					if (leaf_dir) {
						free(leaf_dir);
					}
					MQTTClient_free(topic);
					MQTTClient_freeMessage(&message);
					return err_out;
				}
				
				bool is_valid = false;
				if (i == depth-1) {
					// Leaf directory, ensure this is a directory listing
					is_valid = qth_is_directory_listing(obj);
					if (is_valid) {
						if (leaf_dir) {
							free(leaf_dir);
						}
						leaf_dir = alloced_copyn(message->payload, message->payloadlen);
					}
				} else {
					// Branch directory, make sure next subdirectory is listed
					is_valid = qth_subdirectory_has_behaviour(obj, parts[i], "DIRECTORY");
				}
				
				json_object_put(obj);
				
				// Flag this part of the path as verified
				if (is_valid) {
					if (!verified_parts[i]) {
						num_verified_parts++;
					}
					verified_parts[i] = true;
				} else {
					if (leaf_dir) {
						free(leaf_dir);
					}
					MQTTClient_free(topic);
					MQTTClient_freeMessage(&message);
					return alloced_copy("Directory not found.");
				}
				
				break;
			}
		}
		
		MQTTClient_free(topic);
		MQTTClient_freeMessage(&message);
	}
	
	// Unsubscribe again
	MQTTClient_unsubscribeMany(client, depth, ls_paths);
	*dir = leaf_dir;
	return NULL;
}


/**
 * Set a Qth property or send a Qth event. Returns an error message if there is
 * a problem (which must be freed by the caller).
 */
char *qth_set_delete_or_send(MQTTClient *client, const char *topic, char *value,  bool is_property, int timeout) {
	MQTTClient_deliveryToken tok;
	int status = MQTTClient_publish(client,
	                                topic,
	                                strlen(value), value,
	                                2,  // QoS
	                                is_property,  // Retain
	                                &tok);
	if (status == MQTTCLIENT_SUCCESS) {
		status = MQTTClient_waitForCompletion(client, tok, timeout);
		if (status == MQTTCLIENT_SUCCESS) {
			return NULL;
		} else {
			return alloced_copy("Timeout while waiting for MQTT message to send.");
		}
	} else {
		return alloced_copy("Couldn't send MQTT message.");
	}
}


/**
 * Set a Qth Property. Returns an error message if there is a problem (which
 * must be freed by the caller).
 */
char *qth_set_property(MQTTClient *client, const char *topic, char *value, int timeout) {
	return qth_set_delete_or_send(client, topic, value, true, timeout);
}

/**
 * Send a Qth event. Returns an error message if there is a problem (which must
 * be freed by the caller).
 */
char *qth_send_event(MQTTClient *client, const char *topic, char *value, int timeout) {
	return qth_set_delete_or_send(client, topic, value, false, timeout);
}


/**
 * Extract the path a topic resides in, returning the path as a string to be
 * freed by the caller.
 */
char *get_topic_path(const char *topic) {
	const char *cur = topic;
	const char *last_slash = topic - 1;
	while (*cur) {
		if (*cur == '/') {
			last_slash = cur;
		}
		cur++;
	}
	
	size_t path_len = (last_slash - topic) / sizeof(char);
	char *path = malloc(path_len + 2);
	if (last_slash == topic - 1) {
		path[0] = '\0';
	} else {
		memcpy(path, topic, path_len + 1);
		path[path_len + 1] = '\0';
	}
	
	return path;
}

/**
 * Get the name of a topic within its path. Returns a pointer into the provided
 * string.
 */
const char *get_topic_name(const char *topic) {
	const char *cur = topic;
	const char *last_slash = topic - 1;
	while (*cur) {
		if (*cur == '/') {
			last_slash = cur;
		}
		cur++;
	}
	
	return last_slash + 1;
}


/**
 * Check a topic exists and has the expected behaviour. Returns 0 if it does
 * and non-zero (printing a message to stderr) if not successful.
 */
int verify_topic(MQTTClient *client, const char *topic,
                 const char *desired_behaviour, int meta_timeout) {
	char *path = get_topic_path(topic);
	const char *name = get_topic_name(topic);
	
	char *dir = NULL;
	char *err = qth_get_directory(client, path, &dir, meta_timeout);
	free(path);
	if (err) {
		fprintf(stderr, "Error: %s\n", err);
		free(err);
		return 1;
	}
	
	json_object *dir_obj = json_tokener_parse(dir);
	free(dir);
	
	json_object *topic_obj;
	if (!json_object_object_get_ex(dir_obj, name, &topic_obj)) {
		json_object_put(dir_obj);
		fprintf(stderr, "Error: Topic does not exist.\n");
		return 1;
	}
	
	bool has_behaviour = qth_subdirectory_has_behaviour(dir_obj, name, desired_behaviour);
	json_object_put(dir_obj);
	if (!has_behaviour) {
		fprintf(stderr, "Error: Topic does not have behaviour '%s'.\n",
		        desired_behaviour);
		return 1;
	}
	
	return 0;
}


/**
 * Find out the behaviour of a topic. If the topic does not have a single
 * unique non-directory behaviour (or does not exist), an error is printed on
 * stdout and a non-zero value is returned. The behaviour type returned should
 * be freed by the caller.
 */
int get_topic_behaviour(MQTTClient *client, const char *topic,
                        int meta_timeout, char **behaviour) {
	*behaviour = NULL;
	
	char *path = get_topic_path(topic);
	const char *name = get_topic_name(topic);
	
	char *dir = NULL;
	char *err = qth_get_directory(client, path, &dir, meta_timeout);
	free(path);
	if (err) {
		fprintf(stderr, "Error: %s\n", err);
		free(err);
		return 1;
	}
	
	json_object *dir_obj = json_tokener_parse(dir);
	free(dir);
	
	json_object *topic_obj;
	if (!json_object_object_get_ex(dir_obj, name, &topic_obj)) {
		json_object_put(dir_obj);
		fprintf(stderr, "Error: Topic does not exist.\n");
		return 1;
	}
	
	const char **behaviours = qth_subdirectory_get_behaviours(dir_obj, name);
	const char **p = behaviours;
	const char *best_behaviour = NULL;
	while (*p) {
		if (strcmp(*p, "DIRECTORY") != 0) {
			if (best_behaviour == NULL) {
				best_behaviour = *p;
			} else {
				json_object_put(dir_obj);
				fprintf(stderr, "Error: Topic has more than one behaviour.\n");
				return 1;
			}
		}
		p++;
	}
	
	if (best_behaviour == NULL) {
		json_object_put(dir_obj);
		fprintf(stderr, "Error: Topic is a directory.\n");
		return 1;
	} else {
		*behaviour = alloced_copy(best_behaviour);
		json_object_put(dir_obj);
		return 0;
	}
}
