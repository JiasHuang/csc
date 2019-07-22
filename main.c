#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "options.h"

#define IF_ERR_EXIT(__cond) \
{ \
	int __ret = (__cond); \
	if (__ret) { \
		printf("%s:%d ret %d\n", __func__, __LINE__, __ret); \
		exit(-1); \
	} \
}

static void yuv420p10be_to_nv12_10bit(unsigned char *src, unsigned char *buf, int width, int height)
{
	int w, h;
	unsigned short y0, y1, y2, y3;
	unsigned short u0, v0, u1, v1;
	unsigned char *srcU, *srcV;

	srcU = src + (width * 2) * height;
	srcV = srcU + width * (height >> 1);

	for (h=0; h<height; h++) {
		for (w=0; w<width; w+=4) {
			y0 = src[0] << 8 | src[1];
			y1 = src[2] << 8 | src[3];
			y2 = src[4] << 8 | src[5];
			y3 = src[6] << 8 | src[7];
			buf[0] = y0 >> 2;
			buf[1] = (y0 & 0x03) << 6 | (y1 >> 4);
			buf[2] = (y1 & 0x0f) << 4 | (y2 >> 6);
			buf[3] = (y2 & 0x3f) << 2 | (y3 >> 8);
			buf[4] = y3 & 0xff;
			src += 8;
			buf += 5;
		}
	}

	height >>= 1;

	for (h=0; h<height; h++) {
		for (w=0; w<width; w+=4) {
			u0 = srcU[0] << 8 | srcU[1];
			u1 = srcU[2] << 8 | srcU[3];
			v0 = srcV[0] << 8 | srcV[1];
			v1 = srcV[2] << 8 | srcV[3];
			buf[0] = u0 >> 2;
			buf[1] = (u0 & 0x03) << 6 | (v0 >> 4);
			buf[2] = (v0 & 0x0f) << 4 | (u1 >> 6);
			buf[3] = (u1 & 0x3f) << 2 | (v1 >> 8);
			buf[4] = v1 & 0xff;
			srcU += 4;
			srcV += 4;
			buf += 5;
		}
	}

}

static void save_to_output(char *output, void *buf, int bufsize)
{
	int fd;

	fd = open(output, O_CREAT | O_TRUNC | O_WRONLY, 0666);
	IF_ERR_EXIT(fd < 0);

	write(fd, buf, bufsize);
	close(fd);
}

int main(int argc, char *argv[])
{
	int fd, bufsize;
	struct stat st;
	void *src, *buf;

	fd = -1;
	src = buf = NULL;

	parseArgs(&opts, argc, argv);

	if (!opts.input_cnt || !opts.output_cnt) {
		printf("no input/output\n");
		return 0;
	}

	fd = open(opts.input, O_RDONLY);
	IF_ERR_EXIT(fd < 0);

        fstat(fd, &st);
	src = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	IF_ERR_EXIT(src == MAP_FAILED);

	bufsize = st.st_size;
	buf = malloc(bufsize);
	IF_ERR_EXIT(!buf);

	if (!strcmp(opts.task, "yuv420p10be_to_nv12_10bit"))
		yuv420p10be_to_nv12_10bit(src, buf, opts.width, opts.height);

	save_to_output(opts.output, buf, bufsize);

	free(buf);
	munmap(src, st.st_size);
	close(fd);

	return 0;
}
