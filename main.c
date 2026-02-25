#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <ctype.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

/* ==================== Configuration ==================== */

#define ALPHA_MASK 0xFF000000

typedef enum {
    INIT_FIXED,
    INIT_RAND,
    INIT_RAND_SAME
} InitMode;

typedef struct {
    size_t board_cap;
    size_t iterations;
    size_t image_scale;

    uint8_t rule;
    int rule_all;

    uint32_t fg_color;
    uint32_t bg_color;

    InitMode init_mode;
    uint8_t *init_row;

    uint32_t seed;
    int seed_set;

    int render;

    char out_dir[256];
} CAconfig;

/* ==================== RNG ==================== */

static uint32_t xorshift32(uint32_t *rng)
{
    uint32_t x = *rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *rng = x;
    return x;
}

/* ==================== Initialization ==================== */

static void fill_row_from_hex(
    uint8_t *row,
    size_t width,
    const char *hex)
{
    size_t hex_len = strlen(hex);
    // Interpret hex digits left-to-right as bits
    for (size_t i = 0; i < width; ++i)
    {
        size_t bit_index = width - 1 - i;
        size_t hex_index = bit_index / 4;
        size_t bit_in_hex = bit_index % 4;

        uint8_t value = 0;
        if (hex_index < hex_len)
        {
            char c = hex[hex_len - 1 - hex_index];
            if (isdigit(c)) value = c - '0';
            else if (c >= 'a' && c <= 'f') value = 10 + (c - 'a');
            else if (c >= 'A' && c <= 'F') value = 10 + (c - 'A');
        }

        row[i] = (value >> bit_in_hex) & 1;
    }
}

static void fill_row_random(
    uint8_t *row,
    size_t width,
    uint32_t *rng)
{
    size_t x = 0;
    while (x < width)
    {
        uint32_t value = xorshift32(rng);
        for (size_t bit = 0; bit < 32 && x < width; ++bit, ++x)
            row[x] = (value >> bit) & 1;
    }
}

/* ==================== Command Line ==================== */

static int handleCommandline(int argc, char **argv, CAconfig *cfg)
{
    cfg->board_cap   = 128;
    cfg->iterations  = 128;
    cfg->image_scale = 1;

    cfg->rule        = 110;
    cfg->rule_all    = 0;

    cfg->fg_color    = 0xE69A8D;
    cfg->bg_color    = 0x16501F;

    cfg->init_mode   = INIT_FIXED;
    cfg->init_row    = NULL;

    cfg->seed_set    = 0;
    cfg->render      = 0;

    strcpy(cfg->out_dir, "./out");

    const char *fixed_hex_value = "1";

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--render") == 0)
            cfg->render = 1;
        else if (strcmp(argv[i], "-w") == 0) { i++; cfg->board_cap = strtoull(argv[i], NULL, 10); }
        else if (strcmp(argv[i], "-h") == 0) { i++; cfg->iterations = strtoull(argv[i], NULL, 10); }
        else if (strcmp(argv[i], "-s") == 0) { i++; cfg->image_scale = strtoull(argv[i], NULL, 10); }
        else if (strcmp(argv[i], "-r") == 0) { i++; if (strcmp(argv[i], "all")==0) cfg->rule_all=1; else cfg->rule=(uint8_t)strtoul(argv[i],NULL,10);}
        else if (strcmp(argv[i], "-o") == 0) { i++; strncpy(cfg->out_dir, argv[i], sizeof(cfg->out_dir)-1);}
        else if (strcmp(argv[i], "-f") == 0) { i++; cfg->fg_color = strtoul(argv[i],NULL,0);}
        else if (strcmp(argv[i], "-b") == 0) { i++; cfg->bg_color = strtoul(argv[i],NULL,0);}
        else if (strcmp(argv[i], "-i") == 0)
        {
            i++;
            if (strcmp(argv[i], "rand") == 0)
                cfg->init_mode = INIT_RAND;
            else if (strcmp(argv[i], "randsame") == 0)
                cfg->init_mode = INIT_RAND_SAME;
            else
            {
                cfg->init_mode = INIT_FIXED;
                fixed_hex_value = argv[i];
            }
        }
        else if (strcmp(argv[i], "-q") == 0) { i++; cfg->seed = strtoul(argv[i],NULL,0); cfg->seed_set=1;}
        else return 1;
    }

    if (!cfg->seed_set)
        cfg->seed = (uint32_t)time(NULL);

    if (cfg->init_mode != INIT_RAND)
    {
        cfg->init_row = malloc(cfg->board_cap);
        if (cfg->init_mode == INIT_FIXED)
            fill_row_from_hex(cfg->init_row, cfg->board_cap, fixed_hex_value);
        else
        {
            uint32_t rng = cfg->seed;
            fill_row_random(cfg->init_row, cfg->board_cap, &rng);
        }
    }

    return 0;
}

/* ==================== Automaton ==================== */

static void evolve_row(
    const uint8_t *prev,
    uint8_t *next,
    size_t width,
    uint8_t rule)
{
    next[0] = 0;
    next[width-1] = 0;
    for (size_t x=1; x<width-1; ++x)
    {
        uint8_t pattern = (prev[x-1]<<2) | (prev[x]<<1) | (prev[x+1]);
        next[x] = (rule >> pattern) & 1;
    }
}

/* ==================== Console Rendering ==================== */

static void print_board_console(
    const uint8_t *board,
    size_t width,
    size_t height)
{
    for (size_t y=0; y<height; ++y)
    {
        const uint8_t *row = &board[y*width];
        for (size_t x=0; x<width; ++x)
            putchar(row[x] ? '#' : ' ');
        putchar('\n');
    }
}

/* ==================== Rendering ==================== */

static void save_rule_image(
    const uint8_t *board,
    const CAconfig *cfg,
    uint8_t rule)
{
    size_t width  = cfg->board_cap * cfg->image_scale;
    size_t height = cfg->iterations * cfg->image_scale;
    uint32_t *image = malloc(width*height*sizeof(uint32_t));

    for (size_t y=0; y<height; ++y)
    {
        size_t src_y = y / cfg->image_scale;
        const uint8_t *src_row = &board[src_y*cfg->board_cap];
        uint32_t *dst = &image[y*width];
        for (size_t x=0; x<width; ++x)
        {
            size_t src_x = x / cfg->image_scale;
            dst[x] = (src_row[src_x] ? cfg->fg_color : cfg->bg_color) | ALPHA_MASK;
        }
    }

    char path[512];
    snprintf(path,sizeof(path),"%s/%03u.bmp",cfg->out_dir,rule);
    stbi_write_bmp(path,(int)width,(int)height,4,image);
    free(image);
}

/* ==================== Main ==================== */

int main(int argc, char **argv)
{
    CAconfig cfg;
    if (handleCommandline(argc,argv,&cfg)!=0)
    {
        fprintf(stderr,"Usage: %s [-w width] [-h iterations] [-s scale] [-r rule|all] [-i number|rand|randsame] [-q seed] [-o dir] [-f fg] [-b bg] [--render]\n",argv[0]);
        return EXIT_FAILURE;
    }

    if (cfg.render) mkdir(cfg.out_dir,0755);

    uint8_t *board = malloc(cfg.board_cap*cfg.iterations);
    uint32_t rng = cfg.seed;

    uint8_t start = cfg.rule_all ? 0 : cfg.rule;
    uint8_t end   = cfg.rule_all ? 255 : cfg.rule;

    for (uint8_t rule = start; rule <= end; ++rule)
    {
        printf("Rule %u\n",rule);

        if (cfg.init_mode == INIT_RAND)
            fill_row_random(board, cfg.board_cap, &rng);
        else
            memcpy(board,cfg.init_row,cfg.board_cap);

        for (size_t y=1; y<cfg.iterations; ++y)
            evolve_row(&board[(y-1)*cfg.board_cap], &board[y*cfg.board_cap], cfg.board_cap, rule);

        if (cfg.render)
            save_rule_image(board,&cfg,rule);
        else
            print_board_console(board,cfg.board_cap,cfg.iterations);
    }

    free(cfg.init_row);
    free(board);
    return 0;
}