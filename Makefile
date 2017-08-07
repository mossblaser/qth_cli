SOURCES = main.c json_utils.c option_parsing.c
HEADERS = qth_client.h

qth : $(SOURCES) $(HEADERS)
	gcc -Wall -Werror -lpaho-mqtt3c `pkg-config --libs --cflags json-c` -o qth $(SOURCES)
