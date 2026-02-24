#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

/* ==================== Configuration ==================== */

#define TYPE unsigned __int128
#define BOARD_CAP   (sizeof(TYPE) * 8)
#define ITERATIONS  (BOARD_CAP * 2)   /* now safe to increase */
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

static TYPE randomBoard(uint32_t *rng)
{
    TYPE b = 0;
    for (size_t i = 0; i < sizeof(TYPE) / sizeof(uint32_t); ++i)
    {
        b |= (TYPE)xorshift32(rng) << (i * 32);
    }
    return b;
}

/* ==================== Automaton ==================== */

static void evolve(TYPE *board, uint16_t rule)
{
    uint8_t pattern = *board & 7;

    for (size_t j = 1; j < BOARD_CAP - 1; ++j)
    {
        *board &= ~((TYPE)1 << j);
        *board |= ((TYPE)((rule >> pattern) & 1) << j);
        pattern = ((pattern >> 1) & 3) | ((*board >> j) & 4);
    }
}

/* ==================== Rendering ==================== */

static void render_rule_to_buffer(
    const TYPE *board_buffer,
    uint32_t *dst,
    size_t dst_width)
{
    for (size_t y = 0; y < ITERATIONS; ++y)
    {
        for (size_t x = 0; x < BOARD_CAP; ++x)
        {
            size_t bit = BOARD_CAP - 1 - x;
            uint32_t pixel =
                (((board_buffer[y] >> bit) & 1)
                    ? COLOR_ON
                    : COLOR_OFF) | ALPHA_MASK;

            for (size_t dy = 0; dy < IMAGE_SCALE; ++dy)
            {
                for (size_t dx = 0; dx < IMAGE_SCALE; ++dx)
                {
                    size_t py = y * IMAGE_SCALE + dy;
                    size_t px = x * IMAGE_SCALE + dx;
                    dst[py * dst_width + px] = pixel;
                }
            }
        }
    }
}

/* ==================== Single Rule Image ==================== */

static void save_rule_image(
    const TYPE *board_buffer,
    uint16_t rule)
{
    const size_t width  = BOARD_CAP * IMAGE_SCALE;
    const size_t height = ITERATIONS * IMAGE_SCALE;

    uint32_t *image = calloc(width * height, sizeof(uint32_t));
    if (!image)
    {
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    render_rule_to_buffer(board_buffer, image, width);

    char filename[64];
    snprintf(filename, sizeof(filename),
             OUT_PATH "/%03u.png", rule);

    if (!stbi_write_png(
            filename,
            (int)width,
            (int)height,
            4,
            image,
            (int)(width * sizeof(uint32_t))))
    {
        fprintf(stderr, "Failed to write %s\n", filename);
        exit(EXIT_FAILURE);
    }

    free(image);
}

/* ==================== Debug ==================== */

static void printEvolution(TYPE board)
{
    for (size_t i = 0; i < BOARD_CAP; ++i)
    {
        putchar(" 8"[(board >> (BOARD_CAP - 1 - i)) & 1]);
    }
    putchar('\n');
}

/* ==================== Main ==================== */

int main(void)
{
    TYPE *board_buffer = malloc(sizeof(TYPE) * ITERATIONS);
    if (!board_buffer)
    {
        perror("malloc");
        return EXIT_FAILURE;
    }

    for (uint16_t rule = 0; rule < 256; ++rule)
    {
        printf("Rule %u\n", rule);

        uint32_t rng = 0xC0FFEE;
        TYPE board = randomBoard(&rng);

        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            board_buffer[i] = board;
            evolve(&board, rule);
        }

        save_rule_image(board_buffer, rule);
    }

    free(board_buffer);
    return 0;
}