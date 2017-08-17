SOURCES = main.c \
          qth.c \
          json_utils.c \
          option_parsing.c \
          util.c \
          cmd_ls.c \
          cmd_get_set_delete_watch_send.c \
          cmd_auto.c

HEADERS = qth_client.h

qth : $(SOURCES) $(HEADERS)
	gcc -g -Wall -Werror -lm -lpaho-mqtt3c `pkg-config --libs --cflags json-c` -o qth $(SOURCES)
