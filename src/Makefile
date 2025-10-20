CC = cc
CFLAGS = -Wall -O0 -g
OBJECTS = main.o load.o
TARGETS = turbino

all: $(TARGETS)

turbino: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

clean:
	rm -f $(TARGETS) $(OBJECTS)

run:
	cd html && ../$(TARGETS)
