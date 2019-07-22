
#include <getopt.h>

enum {
    OPT_INPUT,
    OPT_OUTPUT,
    OPT_WIDTH,
    OPT_HEIGHT,
    OPT_TASK,
    OPT_HELP
};

typedef struct {
    char* input;
    int   input_cnt;
    char* output;
    int   output_cnt;
    int   width;
    int   width_cnt;
    int   height;
    int   height_cnt;
    char* task;
    int   task_cnt;
} opts_t;

static struct option long_options[] = {
    {"input",  required_argument, 0, OPT_INPUT},
    {"output", required_argument, 0, OPT_OUTPUT},
    {"width",  required_argument, 0, OPT_WIDTH},
    {"height", required_argument, 0, OPT_HEIGHT},
    {"task",   required_argument, 0, OPT_TASK},
    {"help",         no_argument, 0, OPT_HELP},
    {0, 0, 0, 0}
};

#define OPTSTRING "i:o:w:h:t:"

static void printUsage ()
{
    printf("\t-i, --input file    : input file\n");
    printf("\t-o, --output file   : output file\n");
    printf("\t-w, --width number  : width (Default: 1920)\n");
    printf("\t-h, --height number : height (Default: 1080)\n");
    printf("\t-t, --task taskname : taskname (Default: yuv420p10be_to_nv12_10bit)\n");
}

static void parseArgs(opts_t *opts, int argc, char **argv)
{
    int opt;
    while ((opt = getopt_long (argc, argv, OPTSTRING, long_options, NULL)) != -1)
        switch (opt) {
            case 'i':
            case OPT_INPUT:
                opts->input = optarg;
                opts->input_cnt++;
                break;
            case 'o':
            case OPT_OUTPUT:
                opts->output = optarg;
                opts->output_cnt++;
                break;
            case 'w':
            case OPT_WIDTH:
                opts->width = strtoul(optarg, NULL, 0);
                opts->width_cnt++;
                break;
            case 'h':
            case OPT_HEIGHT:
                opts->height = strtoul(optarg, NULL, 0);
                opts->height_cnt++;
                break;
            case 't':
            case OPT_TASK:
                opts->task = optarg;
                opts->task_cnt++;
                break;
            case OPT_HELP:
            default:
                printUsage();
                exit(0);
                break;
        }
}

opts_t opts = {
    .width = 1920,
    .height = 1080,
    .task = "yuv420p10be_to_nv12_10bit",
};

