#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#define VERSION_STRING "v0.1"


void print_usage(FILE *stream, const char *appname) {
	fprintf(stream,
		"usage: %s [various options] TOPIC [VALUE]\n"
		"   or: %s get [various options] TOPIC\n"
		"   or: %s set [various options] TOPIC [VALUE]\n"
		"   or: %s delete [various options] TOPIC\n"
		"   or: %s watch [various options] TOPIC\n"
		"   or: %s send [various options] TOPIC [VALUE]\n"
		"   or: %s ls [various options] [TOPIC]\n",
		appname, appname, appname, appname, appname, appname, appname
	);
}

void print_help(FILE *stream, const char *appname) {
	print_usage(stream, appname);
	fprintf(stream,
		"\n"
		"A utility for interacting with a Qth-compliant MQTT-based home \n"
		"automation system.\n"
		"\n"
		"optional arguments:\n"
		"  -h --help             show this help message and exit\n"
		"  -V --version          show the program's version number and exit\n"
		"  -H HOST --host HOST   set the hostname of the MQTT broker\n"
		"  -P PORT --port PORT   set the tcp port number of the MQTT broker\n"
		"  -K SECONDS --keep-alive SECONDS\n"
		"                        set the MQTT keepalive interval\n"
		"  -T SECONDS --meta-timeout SECONDS\n"
		"                        the number of seconds to wait for subscriptions\n"
		"                        to 'meta' topics to return. Defaults to 1.\n"
		"  -t SECONDS --timeout SECONDS\n"
		"                        If TOPIC is a property, gives the number of \n"
		"                        seconds to wait before the property value \n"
		"                        arrives. Defaults to 1 in this case. If TOPIC\n"
		"                        is an event, gives the maximum time between\n"
		"                        event values being received.  Defaults to 0\n"
		"                        meaning no timeout in this case.\n"
		"  -c COUNT --count COUNT\n"
		"                        If getting the value of a property or\n"
		"                        directory, exits once the value is received\n"
		"                        COUNT times. Defaults to 1 in this case. If\n"
		"                        watching an event, watch COUNT occurrences of\n"
		"                        the event before exiting. In this case defaults\n"
		"                        to 0 (unlimited).\n"
		"  -1                    An alias for --count=1\n"
		"  -p --pretty-print     pretty-print JSON values\n"
		"  -v --verbatim         show JSON values as-received without changing\n"
		"                        the formatting\n"
		"  -q --quiet            do not output received values\n"
		"\n"
		"optional arguments when used with get, set, delete, watch or send:\n"
		"  -f --force            Treat this topic as a property or event (for\n"
		"                        get, set or delete and watch or send\n"
		"                        respectively) regardless of how it has been\n"
		"                        registered.\n"
		"\n"
		"optional arguments when used with get, set, watch or send:\n"
		"  -r --register         Register the topic with the Qth registrar. The\n"
		"                        following type of registration will be used:\n"
		"                            Command  Type Registered\n"
		"                            -------  --------------------\n"
		"                            get      Many-to-One Property\n"
		"                            set      One-to-Many Property\n"
		"                            watch    Many-to-One Event\n"
		"                            send     One-to-Many Event\n"
		"  -C CLIENT_ID --client-id CLIENT_ID\n"
		"                        When -r is given, specifies the client ID to\n"
		"                        use. If not given, a client ID will be\n"
		"                        randomly generated.\n"
		"  -d DESCRIPTION --description DESCRIPTION\n"
		"                        When -r is given, specifies the description of\n"
		"                        the topic registered.\n"
		"  -U VALUE --on-unregister VALUE\n"
		"                        When -r is given, sets the value of the\n"
		"                        property or sends a final event with value \n"
		"                        VALUE when the command exits.\n"
		"\n"
		"optional arguments when used with get or set:\n"
		"  -D --delete-on-unregister\n"
		"                        When -r is given, deletes the property when the\n"
		"                        command exists.\n"
		"\n"
		"optional arguments when used with watch or send:\n"
		"  -U VALUE --on-unregister VALUE\n"
		"                        When -r is given, sends an event with value\n"
		"                        VALUE when the command exits.\n"
		"\n"
		"optional arguments when used with ls:\n"
		"  -R --recursive        list subdirectories recursively\n"
		"  -l --long             show listing in long format\n"
		"  -j --json             show listing in JSON format\n"
	);
}

void print_version(FILE *stream, const char *appname) {
	fprintf(stream, "%s " VERSION_STRING "\n", appname);
}


#define ARGPARSE_ERRORF(message, ...) do { \
	fprintf(stderr, "%s: " message "\n", argv[0], __VA_ARGS__); \
	exit(1); \
} while (0)

#define ARGPARSE_ERROR(message) do { \
	fprintf(stderr, "%s: " message "\n", argv[0]); \
	exit(1); \
} while (0)


// The type of command entered on the command line.
typedef enum {
	CMD_TYPE_AUTO = 0,
	CMD_TYPE_GET,
	CMD_TYPE_SET,
	CMD_TYPE_DELETE,
	CMD_TYPE_WATCH,
	CMD_TYPE_SEND,
	CMD_TYPE_LS,
} cmd_type_t;

// The type formatting to use when displaying JSON
typedef enum {
	JSON_FORMAT_SINGLE_LINE = 0,
	JSON_FORMAT_PRETTY,
	JSON_FORMAT_VERBATIM,
	JSON_FORMAT_QUIET,
} json_format_t;

// The list formatting to for directory listings
typedef enum {
	LS_FORMAT_SHORT = 0,
	LS_FORMAT_LONG,
	LS_FORMAT_JSON,
} ls_format_t;

// Where the value to be sent or set should be fetched from
typedef enum {
	VALUE_SOURCE_NONE = 0, // No value was provided
	VALUE_SOURCE_NULL,     // Just the JSON constant 'null'
	VALUE_SOURCE_ARG,      // Value in argument
	VALUE_SOURCE_STDIN,    // Read from stdin
} value_source_t;

// Struct defining the options specified on the commandline
typedef struct {
	// Which command was used?
	cmd_type_t cmd_type;
	
	// MQTT connection parameters
	char *mqtt_host;
	int mqtt_port;
	int mqtt_keep_alive;  // (seconds)
	
	// Qth meta access timeout (ms)
	int meta_timeout;
	
	// Value fetching timeouts (ms)
	int property_timeout;
	int event_timeout;
	
	// How many values to get before exiting
	int property_count;
	int event_count;
	
	// How should JSON be displayed
	json_format_t json_format;
	
	// Should topic type be ignored?
	bool force;
	
	// Should the topic type be registered with the Qth registrar
	bool register_topic;
	
	// Client ID to use or NULL if it should be generated randomly.
	char *client_id;
	
	// Description to use when registering a topic.
	char *description;
	
	// Value to set a registered topic to upon unregistering it (or NULL if no
	// message to be sent or if it is to be deleted)
	char *on_unregister;
	
	// Should a property be deleted when it is unregistered
	bool delete_on_unregister;
	
	// Should ls print directories recursively
	bool ls_recursive;
	
	// ls listing format
	ls_format_t ls_format;
	
	// The topic specified
	char *topic;
	
	// Where the value should be taken from
	value_source_t value_source;
	
	// If value_source is VALUE_SOURCE_ARG, a pointer to the value used as an
	// argument, otherwise NULL.
	char *value;
} options_t;


options_t argparse(int argc, char *argv[]) {
	// The options to use, initially set to defaults
	options_t opts = {
		CMD_TYPE_AUTO,  // cmd_type
		"localhost",  // mqtt_port
		1883,  // mqtt_port
		10,  // mqtt_keep_alive
		1000,  // meta_timeout
		1000,  // property_timeout
		0,  // event_timeout
		1,  // property_count
		0,  // event_count
		JSON_FORMAT_SINGLE_LINE,  // json_format
		false,  // force
		false,  // register_topic
		NULL,  // client_id
		"Created on the command-line.",  // description
		NULL,  // on_unregister
		false,  // delete_on_unregister
		false,  // ls_recursive
		LS_FORMAT_SHORT,  // ls_format
		NULL,  // topic
		VALUE_SOURCE_NONE,  // value_source
		NULL,  // value
	};
	
	// Sanity check: Have some arguments
	if (argc < 2) {
		print_usage(stderr, argv[0]);
		ARGPARSE_ERROR("Expected at least one argument.");
	}
	
	// Check to see what type of command the user has requested
	if (strcmp(argv[1], "get") == 0) opts.cmd_type = CMD_TYPE_GET;
	else if (strcmp(argv[1], "set") == 0) opts.cmd_type = CMD_TYPE_SET;
	else if (strcmp(argv[1], "delete") == 0) opts.cmd_type = CMD_TYPE_DELETE;
	else if (strcmp(argv[1], "watch") == 0) opts.cmd_type = CMD_TYPE_WATCH;
	else if (strcmp(argv[1], "send") == 0) opts.cmd_type = CMD_TYPE_SEND;
	else if (strcmp(argv[1], "ls") == 0) opts.cmd_type = CMD_TYPE_LS;
	else opts.cmd_type = CMD_TYPE_AUTO;
	
	// Skip command type and process remaining arguments with getopt
	optind = opts.cmd_type == CMD_TYPE_AUTO ? 1 : 2;
	
	const char *optstring = "hVH:P:K:T:t:c:1pvqfrC:d:U:DRlj";
	
	struct option longopts[] = {
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{"host", required_argument, NULL, 'H'},
		{"port", required_argument, NULL, 'P'},
		{"keep-alive", required_argument, NULL, 'K'},
		{"meta-timeout", required_argument, NULL, 'T'},
		{"timeout", required_argument, NULL, 't'},
		{"count", required_argument, NULL, 'c'},
		{"pretty-print", no_argument, NULL, 'p'},
		{"verbatim", no_argument, NULL, 'v'},
		{"quiet", no_argument, NULL, 'q'},
		{"force", no_argument, NULL, 'f'},
		{"register", no_argument, NULL, 'r'},
		{"description", required_argument, NULL, 'd'},
		{"on-unregister", required_argument, NULL, 'U'},
		{"delete-on-unregister", no_argument, NULL, 'D'},
		{"recursive", no_argument, NULL, 'R'},
		{"long", no_argument, NULL, 'l'},
		{"json", no_argument, NULL, 'j'},
		{"client-id", required_argument, NULL, 'C'},
		{NULL, 0, 0, 0},
	};
	
	int option;
	while ((option = getopt_long(argc, argv, optstring, longopts, NULL)) != -1) {
		switch (option) {
			case 'h':  // --help
				// Print help and exit immediately
				print_help(stdout, argv[0]);
				exit(0);
				break;
			
			case 'V':  // --version
				// Print version and exit immediately
				print_version(stdout, argv[0]);
				exit(0);
				break;
			
			case 'H':  // --host
				opts.mqtt_host = optarg;
				break;
			
			case 'P':  // --port
				opts.mqtt_port = atoi(optarg);
				break;
			
			case 'K':  // --keep-alive
				opts.mqtt_keep_alive = atoi(optarg);
				break;
			
			case 'T':  // --meta-timeout
				opts.meta_timeout = 1000 * atof(optarg);
				break;
			
			case 't':  // --timeout
				opts.property_timeout = opts.event_timeout = 1000 * atof(optarg);
				break;
			
			case 'c':  // --count
				opts.property_count = opts.event_count = atoi(optarg);
				break;
			
			case '1':  // --count
				opts.property_count = opts.event_count = 1;
				break;
			
			case 'p':  // --pretty-print
				opts.json_format = JSON_FORMAT_PRETTY;
				break;
			
			case 'v':  // --verbatim
				opts.json_format = JSON_FORMAT_VERBATIM;
				break;
			
			case 'q':  // --quiet
				opts.json_format = JSON_FORMAT_QUIET;
				break;
			
			case 'f':  // --force
				if (!(opts.cmd_type == CMD_TYPE_GET ||
				      opts.cmd_type == CMD_TYPE_SET ||
				      opts.cmd_type == CMD_TYPE_DELETE ||
				      opts.cmd_type == CMD_TYPE_WATCH ||
				      opts.cmd_type == CMD_TYPE_SEND)) {
					ARGPARSE_ERROR("'--force' can only be used with "
					               "get, set, delete, watch or send.");
				}
				opts.force = true;
				break;
			
			case 'r':  // --register
				if (!(opts.cmd_type == CMD_TYPE_GET ||
				      opts.cmd_type == CMD_TYPE_SET ||
				      opts.cmd_type == CMD_TYPE_WATCH ||
				      opts.cmd_type == CMD_TYPE_SEND)) {
					ARGPARSE_ERROR("'--register' can only be used with "
					               "get, set, watch or send.");
				}
				opts.register_topic = true;
				break;
			
			case 'd':  // --description
				if (!(opts.cmd_type == CMD_TYPE_GET ||
				      opts.cmd_type == CMD_TYPE_SET ||
				      opts.cmd_type == CMD_TYPE_WATCH ||
				      opts.cmd_type == CMD_TYPE_SEND)) {
					ARGPARSE_ERROR("'--description' can only be used with "
					               "get, set, watch or send.");
				}
				opts.description = optarg;
				break;
			
			case 'U':  // --on-unregister
				if (!(opts.cmd_type == CMD_TYPE_GET ||
				      opts.cmd_type == CMD_TYPE_SET ||
				      opts.cmd_type == CMD_TYPE_WATCH ||
				      opts.cmd_type == CMD_TYPE_SEND)) {
					ARGPARSE_ERROR("'--on-unregister' can only be used with "
					               "get, set, watch or send.");
				}
				opts.on_unregister = optarg;
				break;
			
			case 'D':  // --delete-on-unregister
				if (!(opts.cmd_type == CMD_TYPE_GET ||
				      opts.cmd_type == CMD_TYPE_SET)) {
					ARGPARSE_ERROR("'--delete-on-unregister' can only be used with "
					               "get or set.");
				}
				opts.delete_on_unregister = true;
				break;
			
			case 'R':  // --recursive
				if (opts.cmd_type != CMD_TYPE_LS) {
					ARGPARSE_ERROR("'--recursive' can only be used with ls.");
				}
				opts.ls_recursive = true;
				break;
			
			case 'l':  // --long
				if (opts.cmd_type != CMD_TYPE_LS) {
					ARGPARSE_ERROR("'--long' can only be used with ls.");
				}
				opts.ls_format = LS_FORMAT_LONG;
				break;
			
			case 'j':  // --json
				if (opts.cmd_type != CMD_TYPE_LS) {
					ARGPARSE_ERROR("'--long' can only be used with ls.");
				}
				opts.ls_format = LS_FORMAT_JSON;
				break;
		}
	}
	
	// Check for conflicting arguments
	if (!opts.register_topic && opts.on_unregister) {
		ARGPARSE_ERROR("'--on-unregister' cannot be used without '--register'.");
	}
	if (!opts.register_topic && opts.delete_on_unregister) {
		ARGPARSE_ERROR("'--delete-on-unregister' cannot be used without '--register'.");
	}
	if (opts.on_unregister && opts.delete_on_unregister) {
		ARGPARSE_ERROR("'--delete-on-unregister' and '--delete-on-unregister' "
		               "cannot be used at the same time.");
	}
	if (opts.ls_recursive && opts.ls_format == LS_FORMAT_JSON) {
		ARGPARSE_ERROR("'--recursive' and '--json' cannot be used at the same time.");
	}
	
	// Check that the topic was supplied
	if (opts.cmd_type == CMD_TYPE_LS) {
		// Special case: for the 'ls' command, the topic may be omitted to list the
		// root.
		if (optind >= argc) {
			// No ls path provided, list the root
			opts.topic = "";
		} else {
			opts.topic = argv[optind];
			optind++;
		}
	} else {
		// General case
		if (optind >= argc) {
			ARGPARSE_ERROR("expected a topic");
		} else {
			opts.topic = argv[optind];
			optind++;
		}
	}
	
	// Depending on the type of command, work out any associated value which
	// might be required.
	switch (opts.cmd_type) {
		case CMD_TYPE_AUTO:
			if (optind >= argc) {
				opts.value_source = VALUE_SOURCE_NONE;
			} else if (strcmp(argv[optind], "-") == 0) {
				opts.value_source = VALUE_SOURCE_STDIN;
				optind++;
			} else {
				opts.value_source = VALUE_SOURCE_ARG;
				opts.value = argv[optind];
				optind++;
			}
			break;
		
		case CMD_TYPE_SET:
		case CMD_TYPE_SEND:
			if (optind >= argc) {
				opts.value_source = VALUE_SOURCE_NULL;
			} else if (strcmp(argv[optind], "-") == 0) {
				opts.value_source = VALUE_SOURCE_STDIN;
				optind++;
			} else {
				opts.value_source = VALUE_SOURCE_ARG;
				opts.value = argv[optind];
				optind++;
			}
			break;
		
		default:
			// Other commands don't expect a value argument
			opts.value_source = VALUE_SOURCE_NONE;
			break;
	}
	
	// Any remaining values should not be here!
	if (optind < argc) {
		ARGPARSE_ERRORF("unexpected argument '%s'", argv[optind]);
	}
	
	return opts;
}


int main(int argc, char *argv[]) {
	argparse(argc, argv);
	
	return 0;
}


