#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define TYPE unsigned __int128
#define BOARD_CAP (sizeof(TYPE) * 8)
#define ITERATIONS BOARD_CAP
#define IMAGE_SCALE 5
#define COLOR_ON 0x60
#define COLOR_OFF 0x10

// -------------------- Helper Functions --------------------
static uint32_t xorshift32(uint32_t *rng)
{
    uint32_t x = *rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *rng = x;
}

TYPE randomBoard(uint32_t *rng)
{
    TYPE b = 0;
    for (uint8_t chunk = 0; chunk < sizeof(TYPE) / sizeof(uint32_t); ++chunk)
    {
        b |= (TYPE)(xorshift32(rng)) << (32 * chunk);
    }
    return b;
}

// -------------------- Board Initialization --------------------
void prepareBoard(TYPE *board, const TYPE start)
{
    *board = start;
}

// -------------------- Evolution --------------------
void evolve(TYPE *board, uint16_t rule)
{
    uint8_t pattern = *board & 7;
    for (size_t j = 1; j < sizeof(*board) * 8 - 1; ++j)
    {
        *board &= ~((TYPE)1 << j);
        *board |= ((TYPE)(rule >> pattern) & 1) << j;
        pattern = ((pattern >> 1) & 3) | ((*board >> j) & 4);
    }
}

// -------------------- Rendering --------------------
void render_rule_to_buffer(TYPE *board_buffer, uint8_t *dst, int stride)
{
    for (size_t y = 0; y < ITERATIONS; ++y)
    {
        for (size_t x = 0; x < BOARD_CAP; ++x)
        {
            size_t bit = BOARD_CAP - 1 - x;
            uint8_t pixel =
                ((board_buffer[y] >> bit) & 1) ? COLOR_ON : COLOR_OFF;

            for (size_t dy = 0; dy < IMAGE_SCALE; ++dy)
            {
                for (size_t dx = 0; dx < IMAGE_SCALE; ++dx)
                {
                    dst[(y * IMAGE_SCALE + dy) * stride +
                        (x * IMAGE_SCALE + dx)] = pixel;
                }
            }
        }
    }
}

void save_to_image(TYPE *board_buffer, uint16_t rule)
{
    enum { SCALE = 10 };
    char filename[32];
    sprintf(filename, "./out/%03d.bmp", rule);
    uint8_t data[ITERATIONS * SCALE][BOARD_CAP * SCALE] = {0};

    for (size_t y = 0; y < ITERATIONS; ++y)
    {
        for (size_t x = 0; x < BOARD_CAP; ++x)
        {
            size_t bit = BOARD_CAP - 1 - x;
            uint8_t pixel = (board_buffer[y] >> bit) & 1 ? COLOR_ON : COLOR_OFF;
            for (size_t dy = 0; dy < SCALE; ++dy)
            {
                for (size_t dx = 0; dx < SCALE; ++dx)
                {
                    data[y * SCALE + dy][x * SCALE + dx] = pixel;
                }
            }
        }
    }

    if (!stbi_write_bmp(filename, BOARD_CAP * SCALE, ITERATIONS * SCALE, 1, data))
        exit(1);
}

void save_all_rules_tiled()
{
    enum { COLS = 16, ROWS = 16 };
    const TYPE start =
        ((TYPE)0xAA80751640E03629ULL << 64) | 0xA19229CC7F6B8463ULL;

    TYPE board;
    TYPE board_buffer[ITERATIONS] = {};
    uint16_t rule = 218;

    const int tile_w = BOARD_CAP * IMAGE_SCALE;
    const int tile_h = ITERATIONS * IMAGE_SCALE;
    const int out_w = COLS * tile_w;
    const int out_h = ROWS * tile_h;

    uint8_t *image = malloc(out_h * out_w);
    memset(image, 0, out_h * out_w);
    uint8_t *image_local = malloc(tile_h * tile_w);
    memset(image_local, 0, tile_h * tile_w);

    for (uint16_t r = 0; r < 256; ++r)
    {
        printf("Rule %d\n", r);
        rule = r;
        prepareBoard(&board, start);

        for (size_t i = 0; i < ITERATIONS; ++i)
        {
            board_buffer[i] = board;
            evolve(&board, rule);
        }

        int tile_x = (r % COLS) * tile_w;
        int tile_y = (r / COLS) * tile_h;

        render_rule_to_buffer(board_buffer,
                              &image[tile_y * out_w + tile_x],
                              out_w);

        render_rule_to_buffer(board_buffer,
                              image_local,
                              tile_w);

        char filename[64];
        snprintf(filename, sizeof(filename), "./out/%03u.bmp", r);
        if (!stbi_write_bmp(filename, tile_w, tile_h, 1, image_local))
            exit(1);
    }

    if (!stbi_write_bmp("./out/all.bmp", out_w, out_h, 1, image))
        exit(1);

    free(image);
    free(image_local);
}

// -------------------- Debug --------------------
void printEvolution(TYPE board)
{
    for (size_t j = 0; j < BOARD_CAP; ++j)
    {
        fputc(" 8"[(board >> (BOARD_CAP - j - 1)) & 1], stdout);
    }
    fputc('\n', stdout);
}

// -------------------- Main --------------------
int main()
{
    save_all_rules_tiled();
    return 0;
}