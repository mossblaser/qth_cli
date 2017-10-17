/**
 * All option parsing and validation logic.
 */

#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qth_client.h"

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
		"This utility implements several subcommands which perform various\n"
		"actions on the Qth system. If no subcommand is given, the appropriate\n"
		"command is guessed based on how the topic is registered.\n"
		"\n"
		"If a subcommand expects an optional VALUE, it will default to 'null'\n"
		"unless the command is used with --register in which case values will\n"
		"be read, one-per-line, from STDIN. To read values from STDIN for other\n"
		"commands, use '-' for the topic on the commandline.\n"
		"\n"
		"optional arguments:\n"
		"  -h --help             show this help message and exit\n"
		"  -V --version          show the program's version number and exit\n"
		"  -H HOST --host HOST   set the hostname of the MQTT broker (defaults\n"
		"                        to the value of the QTH_HOST environment\n"
		"                        variable, or 'localhost' if not defined).\n"
		"  -P PORT --port PORT   set the tcp port number of the MQTT broker\n"
		"                        (defaults to the value of the QTH_PORT\n"
		"                        environment variable, or 1883 if not defined).\n"
		"  -K SECONDS --keep-alive SECONDS\n"
		"                        set the MQTT keepalive interval\n"
		"  -C CLIENT_ID --client-id CLIENT_ID\n"
		"                        Specifies the client ID to use. If not given, a\n"
		"                        client ID will be randomly generated.\n"
		"  -T SECONDS --meta-timeout SECONDS\n"
		"                        the number of seconds to wait for subscriptions\n"
		"                        to 'meta' topics to return. Defaults to 1.\n"
		"  -t SECONDS --timeout SECONDS\n"
		"                        If setting or deleting a property or sending an\n"
		"                        event, the number of seconds to wait for it to\n"
		"                        be sent (default 1). If getting a property, the\n"
		"                        number of seconds to wait for the value to\n"
		"                        arrive (default 1, or 0 = wait forever if\n"
		"                        --register is used). If watching an event, the\n"
		"                        number of seconds to wait between each event\n"
		"                        arrival (default 0 = wait forever).\n"
		"  -p --pretty-print     pretty-print JSON values\n"
		"  -v --verbatim         show JSON values as-received without changing\n"
		"                        the formatting\n"
		"  -q --quiet            do not output received values (output empty\n"
		"                        lines instead)\n"
		"\n"
		"optional arguments when used with no subcommand or the get, set, \n"
		"delete, watch or send subcommands:\n"
		"  -s --strict           Only allow setting/deleting/sending N:1\n"
		"                        properties and events (or 1:N if registering)\n"
		"                        and getting/watching 1:N properties and events\n"
		"                        (or N:1 if registering).\n"
		"optional arguments when used with get, set, delete, watch or send:\n"
		"  -f --force            Treat this topic as a property or event (for\n"
		"                        get, set or delete and watch or send\n"
		"                        respectively) regardless of how it has been\n"
		"                        registered.\n"
		"\n"
		"optional arguments when used with no subcommand or the get, set, watch\n"
		"or send subcommands:\n"
		"  -c COUNT --count COUNT\n"
		"                        The number of values or events to send/receive\n"
		"                        before exiting. If set to any value except '1',\n"
		"                        also sets --timeout to 0 (no timeout). Override\n"
		"                        this by setting --timeout in a later argument.\n"
		"  -0                    An alias for --count=0\n"
		"  -1                    An alias for --count=1\n"
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

options_t argparse(int argc, char *argv[]) {
	char *default_mqtt_host = getenv("QTH_HOST");
	if (!default_mqtt_host) {
		default_mqtt_host = "localhost";
	}
	char *default_mqtt_port_str = getenv("QTH_PORT");
	if (!default_mqtt_port_str) {
		default_mqtt_port_str = "1883";
	}
	int default_mqtt_port = atoi(default_mqtt_port_str);
	
	// The options to use, initially set to defaults
	options_t opts = {
		CMD_TYPE_AUTO,  // cmd_type
		default_mqtt_host,  // mqtt_host
		default_mqtt_port,  // mqtt_port
		10,  // mqtt_keep_alive
		NULL,  // client_id
		1000,  // meta_timeout
		1000,  // get_timeout
		1000,  // set_timeout
		1000,  // delete_timeout
		0,  // watch_timeout
		1000,  // send_timeout
		1,  // get_count;
		1,  // set_count;
		0,  // watch_count;
		1,  // send_count;
		JSON_FORMAT_SINGLE_LINE,  // json_format
		false,  // strict
		false,  // force
		false,  // register_topic
		"Created on the command-line.",  // description
		NULL,  // on_unregister
		false,  // delete_on_unregister
		false,  // ls_recursive
		LS_FORMAT_SHORT,  // ls_format
		NULL,  // topic
		VALUE_SOURCE_NONE,  // value_source
		NULL,  // value
	};
	
	// The default timeout for 'get' varies depending on whether registration
	// takes place or not. The appropriate choice is made later.
	int get_unregistered_timeout = 1000;
	int get_registered_timeout = 0;
	
	// Default values for 'count' will differ depending on whether the value
	// comes from stdin or from an argument or whether the topic is being
	// registered or not. The appropriate choice is made later...
	int set_arg_count = 1;
	int set_stdin_count = 0;
	int send_arg_count = 1;
	int send_stdin_count = 0;
	int get_unregistered_count = 1;
	int get_registered_count = 0;
	
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
	
	const char *optstring = "hVH:P:K:T:t:c:01pvqsfrC:d:U:DRlj";
	
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
		{"strict", no_argument, NULL, 's'},
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
			
			case 'C':  // --client-id
				opts.client_id = optarg;
				break;
			
			case 'T':  // --meta-timeout
				opts.meta_timeout = 1000 * atof(optarg);
				break;
			
			case 't':  // --timeout
				opts.set_timeout
					= opts.delete_timeout
					= opts.watch_timeout
					= opts.send_timeout
					= get_unregistered_timeout
					= get_registered_timeout
					= 1000 * atof(optarg);
				break;
			
			case 'c':  // --count
				if (!(opts.cmd_type == CMD_TYPE_AUTO ||
				      opts.cmd_type == CMD_TYPE_SET ||
				      opts.cmd_type == CMD_TYPE_GET ||
				      opts.cmd_type == CMD_TYPE_WATCH ||
				      opts.cmd_type == CMD_TYPE_SEND)) {
					ARGPARSE_ERROR("'--count' can only be used with "
					               "get, set, watch or send.");
				}
				opts.watch_count
					= get_unregistered_count
					= get_registered_count
					= set_arg_count
					= set_stdin_count
					= send_arg_count
					= send_stdin_count
					= atoi(optarg);
				
				// Default to infinate timeout when non-one count supplied.
				if (atoi(optarg) != 1) {
					opts.watch_timeout
						= get_unregistered_timeout
						= get_registered_timeout
						= 0;
				}
				break;
			
			case '0':  // -0
				if (!(opts.cmd_type == CMD_TYPE_AUTO ||
				      opts.cmd_type == CMD_TYPE_SET ||
				      opts.cmd_type == CMD_TYPE_GET ||
				      opts.cmd_type == CMD_TYPE_WATCH ||
				      opts.cmd_type == CMD_TYPE_SEND)) {
					ARGPARSE_ERROR("'-0' can only be used with "
					               "get, set, watch or send.");
				}
				opts.watch_count
					= get_unregistered_count
					= get_registered_count
					= set_arg_count
					= set_stdin_count
					= send_arg_count
					= send_stdin_count
					= 0;
				opts.watch_timeout
					= get_unregistered_timeout
					= get_registered_timeout
					= 0;
				break;
			
			case '1':  // -1
				if (!(opts.cmd_type == CMD_TYPE_AUTO ||
				      opts.cmd_type == CMD_TYPE_SET ||
				      opts.cmd_type == CMD_TYPE_GET ||
				      opts.cmd_type == CMD_TYPE_WATCH ||
				      opts.cmd_type == CMD_TYPE_SEND)) {
					ARGPARSE_ERROR("'-1' can only be used with "
					               "get, set, watch or send.");
				}
				opts.watch_count
					= get_unregistered_count
					= get_registered_count
					= set_arg_count
					= set_stdin_count
					= send_arg_count
					= send_stdin_count
					= 1;
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
			
			case 's':  // --strict
				if (!(opts.cmd_type == CMD_TYPE_AUTO ||
				      opts.cmd_type == CMD_TYPE_GET ||
				      opts.cmd_type == CMD_TYPE_SET ||
				      opts.cmd_type == CMD_TYPE_DELETE ||
				      opts.cmd_type == CMD_TYPE_WATCH ||
				      opts.cmd_type == CMD_TYPE_SEND)) {
					ARGPARSE_ERROR("'--strict' can only be used with "
					               "get, set, delete, watch or send.");
				}
				if (opts.force) {
					ARGPARSE_ERROR("'--strict' may not be used with '--force'");
				}
				opts.strict = true;
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
				if (opts.strict) {
					ARGPARSE_ERROR("'--force' may not be used with '--strict'");
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
				char *err = json_validate(optarg, -1);
				if (err) {
					ARGPARSE_ERRORF("'--on-unregister' must be valid JSON: %s", err);
					free(err);  // XXX: Not reached since ARGPARSE_ERROR* calls exit()...
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
				if (opts.register_topic) {
					// If registering default to stdin since the command makes more sense
					// to be long-running
					opts.value_source = VALUE_SOURCE_STDIN;
				} else {
					opts.value_source = VALUE_SOURCE_NULL;
					opts.value = "null";
				}
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
	
	switch (opts.value_source) {
		default:
		case VALUE_SOURCE_ARG:
		case VALUE_SOURCE_NULL:
			opts.set_count = set_arg_count;
			opts.send_count = send_arg_count;
			break;
		
		case VALUE_SOURCE_STDIN:
			opts.set_count = set_stdin_count;
			opts.send_count = send_stdin_count;
			break;
	}
	if (opts.register_topic) {
		opts.get_count = get_registered_count;
		opts.get_timeout = get_registered_timeout;
	} else {
		opts.get_count = get_unregistered_count;
		opts.get_timeout = get_unregistered_timeout;
	}
	
	// Verify any JSON value passed in
	if (opts.value_source == VALUE_SOURCE_ARG) {
		char *err = json_validate(opts.value, -1);
		if (err) {
			ARGPARSE_ERRORF("VALUE must be valid JSON: %s", err);
			free(err);  // XXX: Not reached since ARGPARSE_ERROR* calls exit()...
		}
	}
	
	// Any remaining values should not be here!
	if (optind < argc) {
		ARGPARSE_ERRORF("unexpected argument '%s'", argv[optind]);
	}
	
	return opts;
}
