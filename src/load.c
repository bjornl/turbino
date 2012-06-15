#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "load.h"

struct data *
load(char *file)
{
	int fd, i, j = 0;
	ssize_t len = 1;
	char buf[1400];
	//void *data;
	//unsigned int dlen = 0;
	struct data *d = malloc(sizeof(struct data *));

	d->data = NULL;
	d->len = 0;
	d->key = NULL;

	fd = open(file, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return NULL;
	}

	while (len) {
		len = read(fd, buf, 1400);
		printf("read %zd bytes from fd\n", len);
		if (!len) {
			break;
		} else if (len < 0) {
			perror("read");
			return NULL;
		} else {
			d->len = d->len + len;
			d->data = realloc(d->data, d->len);
			memcpy(d->data+(d->len-len), buf, len);
		}
	}

	close(fd);

	if (d->len > 0) {
		d->key = malloc(strlen(file) + 1);
		sprintf(d->key, "%s", file);
	}

	for (i = strlen(file)-1 ; file[i] != '.' ; i--) {
		j++;
	}

	d->type = malloc(j + 1);
	memcpy(d->type, file+(strlen(file)-j), j);
	d->type[j] = '\0';

	printf("Loaded %d bytes of data\n", d->len);

	return d;
}
