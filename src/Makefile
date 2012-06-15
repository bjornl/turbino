TARGETS = omphello turbino

all: $(TARGETS)

omphello:
	gcc -Wall -fopenmp omphello.c -o omphello

turbino:
	gcc -Wall -fopenmp turbino.c load.c -o turbino

clean:
	rm -f $(TARGETS)
