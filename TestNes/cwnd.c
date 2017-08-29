//
//  cwnd.c
//  TestNes
//
//  Created by arvin on 2017/8/16.
//  Copyright © 2017年 com.fuwo. All rights reserved.
//

#include <SDL2/SDL.h>

#include "hqx.h"

uint32_t   RGBtoYUV[16777216];
uint32_t   YUV1, YUV2;

HQX_API void HQX_CALLCONV hqxInit(void)
{
    /* Initalize RGB to YUV lookup table */
    uint32_t c, r, g, b, y, u, v;
    for (c = 0; c < 16777215; c++) {
        r = (c & 0xFF0000) >> 16;
        g = (c & 0x00FF00) >> 8;
        b = c & 0x0000FF;
        y = (uint32_t)(0.299*r + 0.587*g + 0.114*b);
        u = (uint32_t)(-0.169*r - 0.331*g + 0.5*b) + 128;
        v = (uint32_t)(0.5*r - 0.419*g - 0.081*b) + 128;
        RGBtoYUV[c] = (y << 16) + (u << 8) + v;
    }
}

#define    WIDTH        256
#define    HEIGHT       240
#define    MAG          2   //magnification;

static uint32_t palette_sys[] =
{
    0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0600, 0x561D00,
    0x333500, 0x0B4800, 0x005200, 0x004F08, 0x00404D, 0x000000, 0x000000, 0x000000,
    0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B, 0xB53120, 0x994E00,
    0x6B6D00, 0x388700, 0x0C9300, 0x008F32, 0x007C8D, 0x000000, 0x000000, 0x000000,
    0xFFFEFF, 0x64B0FF, 0x9290FF, 0xC676FF, 0xF36AFF, 0xFE6ECC, 0xFE8170, 0xEA9E22,
    0xBCBE00, 0x88D800, 0x5CE430, 0x45E082, 0x48CDDE, 0x4F4F4F, 0x000000, 0x000000,
    0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF, 0xFBC2FF, 0xFEC4EA, 0xFECCC5, 0xF7D8A5,
    0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000,
};


static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_Rect rect;

static uint8_t pic_mem_orgl[WIDTH * HEIGHT * 4];
static uint8_t pic_mem_frnt[WIDTH * HEIGHT * 4 * MAG * MAG];
static int full_screen = 0;
static uint32_t frame_counter;
static uint32_t time_frame0;

int wnd_init(const char *filename)
{
    hqxInit();
    
    int error = 0;
    if ((error = SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)) != 0) {
        fprintf(stderr, "Couldn't init SDL: %s\n", SDL_GetError());
        return -1;
    }
    atexit(SDL_Quit);
    
    //初始化SDL2音频
    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = 44100;//44100,48000
    wanted_spec.format = AUDIO_F32SYS;
    wanted_spec.channels = 1;
    wanted_spec.silence = 0;
    wanted_spec.samples = 1024;
    wanted_spec.callback = NULL;//采用队列服务
    if (SDL_OpenAudio(&wanted_spec, NULL) < 0){//SDL_OpenAudio的deviceid为1
        printf("can't open audio.\n");
        return -1;
    }
    SDL_PauseAudio(0);
    
    rect.x=rect.y=0;
    rect.h=HEIGHT*MAG;
    rect.w=WIDTH*MAG;
    
    window = SDL_CreateWindow(filename,
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              WIDTH*MAG, HEIGHT*MAG,
                              SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "Couldn't create SDL window: %s\n", SDL_GetError());
        return -1;
    }
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "Couldn't create SDL renderer: %s\n", SDL_GetError());
        return -1;
    }
    if (SDL_RenderSetLogicalSize(renderer, WIDTH*MAG, HEIGHT*MAG) != 0) {
        fprintf(stderr, "Couldn't set SDL renderer logical resolution: %s\n", SDL_GetError());
        return -1;
    }
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WIDTH*MAG, HEIGHT*MAG);
    if (!texture) {
        fprintf(stderr, "Couldn't create SDL texture: %s\n", SDL_GetError());
        return -1;
    }
    
    frame_counter = 0;
    time_frame0 = SDL_GetTicks();
    
    return 0;
}

static uint32_t wnd_time_left(void)
{
    uint32_t now, timer_next;
    now = SDL_GetTicks();
    timer_next = (frame_counter * 1001) / 60 + time_frame0;
    if (timer_next <= now) return 0;
    return timer_next - now;
}

void wnd_draw(uint8_t* pixels)
{
    uint32_t *fb = (uint32_t*)pic_mem_orgl;
    for (unsigned int y = 0; y < HEIGHT; y++){
        for (unsigned int x = 0; x < WIDTH; x++) {
            uint32_t pixel = *(pixels + WIDTH * y + x);
            uint32_t rgb = palette_sys[pixel];
            *fb++ = rgb;
        }
    }
    
    if (MAG == 1) {
        memcpy(pic_mem_frnt, pic_mem_orgl, WIDTH*HEIGHT*4);
    }
    if (MAG == 2) {
        hq2x_32((uint32_t*)pic_mem_orgl, (uint32_t*)pic_mem_frnt, WIDTH, HEIGHT);
    }
    if (MAG == 3) {
        hq3x_32((uint32_t*)pic_mem_orgl, (uint32_t*)pic_mem_frnt, WIDTH, HEIGHT);
    }
    if (MAG == 4) {
        hq4x_32((uint32_t*)pic_mem_orgl, (uint32_t*)pic_mem_frnt, WIDTH, HEIGHT);
    }
    
    ++frame_counter;
    SDL_Delay(wnd_time_left());
    
    void *p; int pitch;
    SDL_LockTexture(texture, &rect, &p, &pitch);
    SDL_memcpy(p, pic_mem_frnt, HEIGHT*MAG*WIDTH*MAG*4);
    SDL_UnlockTexture(texture);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, &rect, &rect);
    SDL_RenderPresent(renderer);
}

void wnd_play(float com)
{
    SDL_QueueAudio(1, &com, sizeof(float));
}

static uint8_t wnd_key2btn(SDL_KeyboardEvent *key, uint8_t* ctrl)
{
    uint8_t btn = 0;
    switch (key->keysym.sym) {
        case SDLK_q:
            btn = 1 << 7;//A
            break;
        case SDLK_w:
            btn = 1 << 6;//B
            break;
        case SDLK_1:
            btn = 1 << 5;//SELECT
            break;
        case SDLK_RETURN:
            btn = 1 << 4;//START
            break;
        case SDLK_UP:
            btn = 1 << 3;//UP
            break;
        case SDLK_DOWN:
            btn = 1 << 2;//DOWN
            break;
        case SDLK_LEFT:
            btn = 1 << 1;//LEFT
            break;
        case SDLK_RIGHT:
            btn = 1 << 0;//RIGHT
            break;
    }
    if (key->state == SDL_PRESSED){
        ctrl[0] |= btn;
        ctrl[1] |= btn;
    }else if (key->state == SDL_RELEASED){
        ctrl[0] &= ~btn;
        ctrl[1] &= ~btn;
    }
    return 0;
}

int wnd_poll(uint8_t* ctrl)
{
    SDL_Event event;
    if (!SDL_PollEvent(&event)) return -1;
    switch (event.type) {
        case SDL_QUIT:
            printf("shutdown...\n");
            return 1;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (event.key.keysym.sym == SDLK_ESCAPE) return 1;
            if (event.key.keysym.sym == SDLK_f && event.key.state == SDL_RELEASED) {
                full_screen ^= 1;
                SDL_ShowCursor(full_screen ? SDL_DISABLE : SDL_ENABLE);
                SDL_SetWindowFullscreen(window, full_screen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                return 0;
            }
            return wnd_key2btn(&event.key, ctrl);
    }
    return 0;
}

