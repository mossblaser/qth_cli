SOURCES = main.c qth.c json_utils.c option_parsing.c util.c cmd_ls.c
HEADERS = qth_client.h

qth : $(SOURCES) $(HEADERS)
	gcc -g -Wall -Werror -lm -lpaho-mqtt3c `pkg-config --libs --cflags json-c` -o qth $(SOURCES)
