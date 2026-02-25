#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

/* ==================== Configuration ==================== */

#define COLOR_ON   0xE69A8D
#define COLOR_OFF  0x16501F
#define ALPHA_MASK 0xFF000000

#ifndef OUT_PATH
#define OUT_PATH "./out"
#endif

typedef struct {
    size_t board_cap;
    size_t iterations;
    size_t image_scale;
} CAconfig;

/* ==================== Command Line ==================== */

static int handleCommandline(int argc, char **argv, CAconfig *config)
{
    config->board_cap   = 128;
    config->iterations  = 128;
    config->image_scale = 1;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-w") == 0)
        {
            if (++i >= argc) return 1;
            config->board_cap = strtoull(argv[i], NULL, 10);
        }
        else if (strcmp(argv[i], "-h") == 0)
        {
            if (++i >= argc) return 1;
            config->iterations = strtoull(argv[i], NULL, 10);
        }
        else if (strcmp(argv[i], "-s") == 0)
        {
            if (++i >= argc) return 1;
            config->image_scale = strtoull(argv[i], NULL, 10);
        }
        else
        {
            return 1;
        }
    }

    if (config->board_cap == 0 ||
        config->iterations == 0 ||
        config->image_scale == 0)
        return 1;

    return 0;
}

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

/* ==================== Automaton ==================== */

static void evolve_row(
    const uint8_t *prev,
    uint8_t *next,
    size_t width,
    uint16_t rule)
{
    next[0] = 0;
    next[width - 1] = 0;

    for (size_t x = 1; x < width - 1; ++x)
    {
        uint8_t pattern =
            (prev[x - 1] << 2) |
            (prev[x]     << 1) |
            (prev[x + 1]);

        next[x] = (rule >> pattern) & 1;
    }
}

/* ==================== Rendering ==================== */

static void save_rule_image(
    const uint8_t *board,
    const CAconfig *cfg,
    uint16_t rule)
{
    const size_t width  = cfg->board_cap * cfg->image_scale;
    const size_t height = cfg->iterations * cfg->image_scale;

    uint32_t *image = malloc(width * height * sizeof(uint32_t));

    for (size_t y = 0; y < height; ++y)
    {
        size_t src_y = y / cfg->image_scale;
        const uint8_t *src_row =
            &board[src_y * cfg->board_cap];

        uint32_t *dst_row = &image[y * width];

        for (size_t x = 0; x < width; ++x)
        {
            size_t src_x = x / cfg->image_scale;
            dst_row[x] =
                (src_row[src_x] ? COLOR_ON : COLOR_OFF)
                | ALPHA_MASK;
        }
    }

    char filename[64];
    snprintf(filename, sizeof(filename),
             OUT_PATH "/%03u.bmp", rule);

    stbi_write_bmp(
        filename,
        (int)width,
        (int)height,
        4,
        image);

    free(image);
}

/* ==================== Main ==================== */

int main(int argc, char **argv)
{
    CAconfig cfg;
    if (handleCommandline(argc, argv, &cfg) != 0)
    {
        fprintf(stderr,
            "Usage: %s [-w board_cap] [-h iterations] [-s image_scale]\n",
            argv[0]);
        return EXIT_FAILURE;
    }

    uint8_t *board =
        malloc(cfg.board_cap * cfg.iterations);

    uint32_t rng = 0xC0FFEE;

    for (uint16_t rule = 0; rule < 256; ++rule)
    {
        printf("Rule %u\n", rule);

        uint8_t *row0 = &board[0];
        for (size_t x = 0; x < cfg.board_cap; ++x)
            row0[x] = xorshift32(&rng) & 1;

        for (size_t y = 1; y < cfg.iterations; ++y)
        {
            evolve_row(
                &board[(y - 1) * cfg.board_cap],
                &board[y * cfg.board_cap],
                cfg.board_cap,
                rule);
        }

        save_rule_image(board, &cfg, rule);
    }

    free(board);
    return 0;
}