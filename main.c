#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

/* ==================== Configuration ==================== */

#define BOARD_CAP   128
#define ITERATIONS  (BOARD_CAP * 2)
#define IMAGE_SCALE 20

#define COLOR_ON   0xE69A8D
#define COLOR_OFF  0x16501F
#define ALPHA_MASK 0xFF000000

#ifndef OUT_PATH
#define OUT_PATH "./out"
#endif

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

static void evolve_all(
    uint8_t *board,
    uint16_t rule,
    uint32_t *rng)
{
    for (size_t x = 0; x < BOARD_CAP; ++x)
        board[x] = xorshift32(rng) & 1;

    for (size_t y = 1; y < ITERATIONS; ++y)
    {
        uint8_t *prev = &board[(y - 1) * BOARD_CAP];
        uint8_t *cur  = &board[y * BOARD_CAP];

        cur[0] = 0;
        cur[BOARD_CAP - 1] = 0;

        for (size_t x = 1; x < BOARD_CAP - 1; ++x)
        {
            uint8_t pattern =
                (prev[x - 1] << 0) |
                (prev[x    ] << 1) |
                (prev[x + 1] << 2);

            cur[x] = (rule >> pattern) & 1;
        }
    }
}

/* ==================== Rendering ==================== */

static void save_rule_image(
    const uint8_t *board,
    uint16_t rule)
{
    const size_t width  = BOARD_CAP * IMAGE_SCALE;
    const size_t height = ITERATIONS * IMAGE_SCALE;

    uint32_t *image = malloc(width * height * sizeof(uint32_t));

    for (size_t y = 0; y < height; ++y)
    {
        size_t src_y = y / IMAGE_SCALE;
        const uint8_t *row = &board[src_y * BOARD_CAP];
        uint32_t *dst = &image[y * width];

        for (size_t x = 0; x < width; ++x)
        {
            size_t src_x = x / IMAGE_SCALE;
            dst[x] =
                (row[src_x] ? COLOR_ON : COLOR_OFF) | ALPHA_MASK;
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

/* ==================== Debug ==================== */

static void print_row(const uint8_t *row)
{
    for (size_t i = 0; i < BOARD_CAP; ++i)
        putchar(row[i] ? '8' : ' ');
    putchar('\n');
}

/* ==================== Main ==================== */

int main(void)
{
    uint8_t *board =
        malloc(BOARD_CAP * ITERATIONS * sizeof(uint8_t));

    uint32_t rng = 0xC0FFEE;

    for (uint16_t rule = 0; rule < 256; ++rule)
    {
        printf("Rule %u\n", rule);
        evolve_all(board, rule, &rng);
        save_rule_image(board, rule);
    }

    free(board);
    return 0;
}