#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "options.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define IF_ERR_EXIT(__cond) \
{ \
	int __ret = (__cond); \
	if (__ret) { \
		printf("%s:%d ret %d\n", __func__, __LINE__, __ret); \
		exit(-1); \
	} \
}

struct convctx {
	char *name;
	void* (*conv)(void *src, int width, int height, int *planeCnt, int *planeSizes);
};

static void* yuv420p10be_to_nv12_10b(void *src, int width, int height, int *planeCnt, int *planeSizes)
{
	int w, h, bufsize;
	unsigned short y0, y1, y2, y3;
	unsigned short u0, v0, u1, v1;
	unsigned char *srcY, *srcU, *srcV, *buf;
	void *dst;

	srcY = src;
	srcU = srcY + (width * 2) * height;
	srcV = srcU + width * (height >> 1);

	bufsize = width * height * 15 / 8;
	dst = malloc(bufsize);
	buf = dst;

	for (h=0; h<height; h++) {
		for (w=0; w<width; w+=4) {
			y0 = srcY[0] << 8 | srcY[1];
			y1 = srcY[2] << 8 | srcY[3];
			y2 = srcY[4] << 8 | srcY[5];
			y3 = srcY[6] << 8 | srcY[7];
			buf[0] = y0 >> 2;
			buf[1] = (y0 & 0x03) << 6 | (y1 >> 4);
			buf[2] = (y1 & 0x0f) << 4 | (y2 >> 6);
			buf[3] = (y2 & 0x3f) << 2 | (y3 >> 8);
			buf[4] = y3 & 0xff;
			srcY += 8;
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

	*planeCnt = 2;
	planeSizes[0] = width * height * 5 / 4;
	planeSizes[1] = width * (height >> 1) * 5 / 4;

	return dst;
}

static void save_to_output(char *output, void *buf, int bufsize)
{
	int fd;

	fd = open(output, O_CREAT | O_TRUNC | O_WRONLY, 0666);
	IF_ERR_EXIT(fd < 0);

	write(fd, buf, bufsize);
	close(fd);
}

static const struct convctx* get_convctx_by_name(char *name)
{
	int i;

	static const struct convctx convtable[] = {
		{"yuv420p10be_to_nv12_10b", yuv420p10be_to_nv12_10b},
	};

	for (i=0; i<ARRAY_SIZE(convtable); i++)
		if (!strcmp(name, convtable[i].name))
			return &convtable[i];

	return NULL;
}

int main(int argc, char *argv[])
{
	int i, fd, bufsize, offset;
	struct stat st;
	void *src, *buf;
	const struct convctx *convctx;
	int planeCnt, planeSizes[3];

	fd = -1;
	src = buf = NULL;
	bufsize = offset = 0;

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

	convctx = get_convctx_by_name(opts.task);
	IF_ERR_EXIT(!convctx);

	buf = convctx->conv(src, opts.width, opts.height, &planeCnt, planeSizes);
	IF_ERR_EXIT(!buf);

	for (i=0; i<planeCnt; i++)
		bufsize += planeSizes[i];

	for (i=0; i<opts.output_cnt-1; i++) {
		save_to_output(opts.output[i], buf + offset, planeSizes[i]);
		offset += planeSizes[i];
	}

	/* output all remaining data */
	save_to_output(opts.output[i], buf + offset, bufsize - offset);

	free(buf);
	munmap(src, st.st_size);
	close(fd);

	return 0;
}
