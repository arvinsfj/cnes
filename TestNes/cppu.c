//
//  cppu.c
//  TestNes
//
//  Created by arvin on 2017/8/16.
//  Copyright © 2017年 com.fuwo. All rights reserved.
//

#include "cnes.h"
#include "cppu.h"

#define  PPU_TICKS_PER_FRAME    89342 // = 341 * 262

#define  PPU_PRIO_NONE     0
#define  PPU_PRIO_BEHIND   1
#define  PPU_PRIO_BG       2
#define  PPU_PRIO_FRONT    3

#define     REG_PPUCTRL    0
#define     PPUCTRL_V      (1 << 7)    /* Vertical blank NMI enable */
#define     PPUCTRL_H      (1 << 5)    /* Sprite size (0: 8x8; 1: 8x16) */
#define     PPUCTRL_B      (1 << 4)    /* Background palette table address */
#define     PPUCTRL_S      (1 << 3)    /* Sprite pattern table address for 8x8 sprites */
#define     PPUCTRL_I      (1 << 2)    /* VRAM address increment per CPU read/write of PPUDATA */
#define     PPUCTRL_N      (3 << 0)    /* Base nametable address */
#define     PPUCTRL_N_X    (1 << 0)    /* Add 256 to the X scroll position */
#define     PPUCTRL_N_Y    (2 << 0)    /* Add 240 to the Y scroll position */
#define     REG_PPUMASK    1
#define     PPUMASK_s      (1 << 4)    /* Show sprites */
#define     PPUMASK_b      (1 << 3)    /* Show background */
#define     PPUMASK_m      (1 << 1)    /* Show background in leftmost 8 pixels */
#define     REG_PPUSTATUS  2
#define     PPUSTATUS_V    (1 << 7)    /* Vertical blank */
#define     PPUSTATUS_S    (1 << 6)    /* Sprite 0 Hit */
#define     REG_OAMADDR    3
#define     REG_OAMDATA    4
#define     REG_PPUSCROLL  5
#define     REG_PPUADDR    6
#define     REG_PPUDATA    7

#define     PPU_TILE_ADDR(v)    (0x2000 | ((v) & 0x0fff))
#define     PPU_ATTR_ADDR(v)    (0x23c0 | ((v) & 0x0c00) | (((v) >> 4) & 0x38) | (((v) >> 2) & 0x07))

static uint16_t ppu_incr_x(ppu_context *p)
{
    uint16_t v = p->reg_v;
    if ((v&0x001F) == 31) { v&=0xFFE0; v^=0x0400; return v; }
    return ++v;
}

static uint16_t ppu_incr_y(ppu_context *p)
{
    uint16_t v = p->reg_v;
    if ((v & 0x7000) != 0x7000) { v += 0x1000; return v; }
    uint16_t y = ((v&=0x8FFF) & 0x03E0) >> 5;
    if (y == 29) { y = 0;  v ^= 0x0800; }
    else if (y == 31) { y = 0; }
    else { y += 1; }
    return (v & 0xFC1F) | (y << 5);
}

//PPU寄存器读写逻辑开始
uint8_t ppu_read_r(ppu_context *p, uint16_t addr)
{
    uint16_t vaddr = 0; uint8_t reg = (addr-0x2000)&0x07, val = 0; int incr = 0;
    switch (reg) {
        case REG_PPUSTATUS:
            val = p->regs[reg]; p->regs[reg] &= ~PPUSTATUS_V;
            p->reg_w = 0;
            break;
        case REG_OAMDATA:
            val = p->oam[p->oamaddr];
            break;
        case REG_PPUDATA:
            vaddr = p->reg_v & 0x3FFF;
            if (vaddr <= 0x3EFF) { val = p->ppudata; p->ppudata = p->read8(vaddr); }
            else { val = p->read8(vaddr); }
            incr = (p->regs[REG_PPUCTRL] & PPUCTRL_I) ? 32 : 1;
            p->reg_v = (p->reg_v + incr) & 0x3FFF;
            break;
        default:
            val = p->regs[reg];
            break;
    }
    return val;
}

void ppu_write_r(ppu_context *p, uint16_t addr, uint8_t val)
{
    uint16_t vaddr = 0; uint8_t reg = (addr-0x2000)&0x07; int incr = 0;
    switch (reg) {
        case REG_PPUCTRL:
            p->regs[reg] = val;
            p->reg_t = (p->reg_t & 0xF3FF) | (((uint16_t)val & 0x3) << 10);//取val的低2位（Name Table Address）
            break;
        case REG_OAMADDR:
            p->oamaddr = val;
            p->regs[reg] = val;
            break;
        case REG_OAMDATA:
            p->oam[p->oamaddr] = val;
            p->oamaddr = (p->oamaddr + 1) & 255;//地址加1，为第二次写入做准备
            p->regs[reg] = val;
            break;
        case REG_PPUSCROLL:
            if (!p->reg_w) {//第一次写入
                p->reg_t = (p->reg_t & 0xFFE0) | ((uint16_t)(val) >> 3);
                p->reg_x = (val & 0x07);
            } else {//第二次写入
                p->reg_t = (p->reg_t & 0x8FFF) | (((uint16_t)(val) & 0x07) << 12);
                p->reg_t = (p->reg_t & 0xFC1F) | (((uint16_t)(val) & 0xF8) << 2);
            }
            p->reg_w ^= 1;//写2次的跳动,0->1->0
            p->regs[reg] = val;//regs中是第二次写的val数值
            break;
        case REG_PPUADDR:
            if (!p->reg_w) {
                p->reg_t = (p->reg_t & 0x80FF) | (((uint16_t)(val) & 0x3F) << 8);
            } else {
                p->reg_t = (p->reg_t & 0xFF00) | (uint16_t)(val);
                p->reg_v = p->reg_t;
            }
            p->reg_w ^= 1;//写2次的跳动,0->1->0
            p->regs[reg] = val;
            break;
        case REG_PPUDATA:
            vaddr = p->reg_v & 0x3FFF;
            p->write8(vaddr, val);
            incr = (p->regs[REG_PPUCTRL] & PPUCTRL_I) ? 32 : 1;
            p->reg_v = (p->reg_v + incr) & 0x3FFF;
            break;
        default:
            p->regs[reg] = val;
            break;
    }
}
//PPU寄存器读写逻辑结束

//=========================================================================//
//处理像素绘制逻辑
//获取某条扫描线需要绘制的精灵
static void ppu_sprites(ppu_context *p, unsigned int y)
{
    p->scanline_num_sprites = 0;
    const uint16_t sprite_height = (p->regs[REG_PPUCTRL] & PPUCTRL_H) ? 16 : 8;
    for (int n = 0; n < 64 && p->scanline_num_sprites < 8; n++) {
        const uint8_t sprite_y = p->oam[n * 4 + 0] + 1;//精灵的左上角坐标y值+1
        if (sprite_y == 0 || sprite_y >= 0xf0) continue;
        if (y>=sprite_y && y<sprite_y+sprite_height) p->scanline_sprites[p->scanline_num_sprites++] = n;
    }
}

//像素绘制函数
static void ppu_pixel(ppu_context *p, unsigned int x, unsigned int y)
{
    const int show_background = (p->regs[REG_PPUMASK] & PPUMASK_b) != 0;
    const int show_sprites = (p->regs[REG_PPUMASK] & PPUMASK_s) != 0;
    const uint16_t pal_start = 0x3f00;//调色板基地址
    if (show_background) {
        const uint8_t cs = (p->attr >> 30) & 3;//调色板编号
        const int bit = 1 << (15 - p->reg_x);
        const uint8_t pal = ((p->tile_l & bit) ? 1 : 0) | ((p->tile_h & bit) ? 2 : 0);//调色板内的颜色编号
        //删除对应的缓存数据位
        p->tile_l <<= 1; p->tile_h <<= 1; p->attr <<= 2;
        if (x <= 0xff) {//绘制可视区域扫描线
            p->pixels[y][x].pal = pal;
            p->pixels[y][x].has_sprite = 0;
            if (pal) {
                p->pixels[y][x].priority = PPU_PRIO_BG;
                p->pixels[y][x].c = p->read8(pal_start + (cs<<2) + pal) & 0x3f;//真实的系统调色板颜色索引
            } else {
                p->pixels[y][x].priority = PPU_PRIO_NONE;
                p->pixels[y][x].c = p->read8(pal_start) & 0x3f;//系统默认背景颜色
            }
            if (x < 8 && ((p->regs[REG_PPUMASK] & PPUMASK_m) == 0)) {//应该是等于0
                p->pixels[y][x].priority = PPU_PRIO_NONE;
                p->pixels[y][x].pal = 0;
                p->pixels[y][x].c = p->read8(pal_start) & 0x3f;
            }
        }
    } else {
        if (x <= 0xff) {
            p->pixels[y][x].priority = PPU_PRIO_NONE;
            p->pixels[y][x].pal = 0;
            p->pixels[y][x].c = p->read8(pal_start) & 0x3f;
            p->pixels[y][x].has_sprite = 0;
        }
    }
    if (x > 0xff) return;
    if (show_sprites) {
        const uint16_t sprite_height = (p->regs[REG_PPUCTRL] & PPUCTRL_H) ? 16 : 8;
        for (int i = 0; i < p->scanline_num_sprites; i++) {
            const int n = p->scanline_sprites[i];//获取精灵oam中的下标索引
            const uint8_t sprite_y = p->oam[n * 4 + 0] + 1;
            const uint8_t sprite_x = p->oam[n * 4 + 3];
            //过滤Y显示不全的精灵
            if (sprite_y == 0 || sprite_y >= 0xf0) continue;
            //过滤不在精灵X范围的像素绘制
            if (x < sprite_x || x >= sprite_x + 8) continue;
            const unsigned int xrel = x - sprite_x;
            const unsigned int yrel = y - sprite_y;
            const uint8_t sprite_tile = p->oam[n * 4 + 1];
            const uint8_t sprite_attr = p->oam[n * 4 + 2];
            const int flip_h = (sprite_attr & 0x40) != 0;
            const int flip_v = (sprite_attr & 0x80) != 0;
            const int priority = (sprite_attr & 0x20) != 0 ? PPU_PRIO_BEHIND : PPU_PRIO_FRONT;
            
            const uint16_t pat_start = sprite_height == 8 ? ((p->regs[REG_PPUCTRL] & PPUCTRL_S) ? 0x1000 : 0x0000) : (sprite_tile & 1) << 12;
            const uint16_t sprite_tile_start = sprite_height == 8 ? sprite_tile : (sprite_tile & 0xfe);
            
            uint16_t pat_off = sprite_tile_start * 16 + (flip_v ? ((sprite_height - 1) - (yrel & (sprite_height - 1))) : (yrel & (sprite_height - 1)));
            if (yrel >= 8) pat_off += 8;
            if (sprite_height == 16 && flip_v) (yrel >= 8) ? (pat_off -= 8) : (pat_off += 8);
            
            const uint8_t pat_l = p->read8(pat_start + pat_off);
            const uint8_t pat_h = p->read8(pat_start + pat_off + 8);
            const int bit = flip_h ? (xrel & 7) : 7 - (xrel & 7);
            //获取真实的像素点深度,调色板内的颜色编号
            const uint8_t pal = ((pat_l & (1 << bit)) ? 1 : 0) | ((pat_h & (1 << bit)) ? 2 : 0);
            
            if (n == 0 && p->pixels[y][x].pal != 0 && pal != 0) {
                if (!(p->regs[REG_PPUSTATUS] & PPUSTATUS_S)) {
                    p->regs[REG_PPUSTATUS] |= PPUSTATUS_S;//sprite0 hit
                }
            }
            
            if (p->pixels[y][x].has_sprite) continue;
            if (pal) p->pixels[y][x].has_sprite = 1;
            if (priority <= p->pixels[y][x].priority || pal == 0) continue;
            const uint8_t cs = (sprite_attr & 0x3) + 4;
            p->pixels[y][x].priority = priority;
            p->pixels[y][x].pal = pal;
            p->pixels[y][x].c = p->read8(pal_start + (cs * 4) + pal) & 0x3f;//获取真实的系统调色板索引数值
            break;
        }
    }
}

static uint8_t ppu_bg_cs(uint8_t attr, unsigned int x, unsigned int y)
{
    //获取背景4个BLOCK块（4*4个tile）中坐标（x， y）的attr数值（颜色索引的高2位，决定了调色板）
    if ((x & 0x1f) < 16 && (y & 0x1f) < 16) { return (attr >> 0) & 0x3; }
    else if ((x & 0x1f) < 16 && (y & 0x1f) >= 16) { return (attr >> 4) & 0x3; }
    else if ((x & 0x1f) >= 16 && (y & 0x1f) < 16) { return (attr >> 2) & 0x3; }
    else { return (attr >> 6) & 0x3; }
}
//渲染资源拿取函数
static void ppu_fetch(ppu_context *p)
{
    const uint8_t nt = p->read8(PPU_TILE_ADDR(p->reg_v));//reg_v地址的低12位为nametable地址，首位加上2
    const uint8_t fine_y = p->reg_v >> 12;//reg_v地址的高4位为fine_y数值(3)
    const uint8_t fine_x = p->reg_x;//reg_x存放fine_x
    const int xscroll = ((p->reg_v & 0x1f) << 3) | fine_x;
    const int yscroll = (((p->reg_v >> 5) & 0x1f) << 3) | fine_y;
    uint16_t attr = 0;
    uint8_t curattr = p->read8(PPU_ATTR_ADDR(p->reg_v));
    for (int i = 0; i < 8; i++) {
        if (i == (8 - p->reg_x)) curattr = p->read8(PPU_ATTR_ADDR(ppu_incr_x(p)));
        attr <<= 2; attr |= ppu_bg_cs(curattr, xscroll + i, yscroll);
    }
    p->attr |= attr;//设置attr
    const uint16_t pat_start = (p->regs[REG_PPUCTRL] & PPUCTRL_B) ? 0x1000 : 0x0000;
    const uint16_t pat_off = (uint16_t)nt * 16 + (p->reg_v >> 12);
    p->tile_l |= p->read8(pat_start + pat_off);//设置tile
    p->tile_h |= p->read8(pat_start + pat_off + 8);
    p->reg_v = ppu_incr_x(p);//vram地址增加
}

int ppu_step_i(ppu_context *p)
{
    if (p->frame_ticks == PPU_TICKS_PER_FRAME - 1) {//时钟周期用完了
        if (p->draw){//绘图
            uint8_t* index = (uint8_t*)p->pallete_index; pixel* pixels = (pixel*)p->pixels;
            for (unsigned long i = 0; i < PPU_HEIGHT*PPU_WIDTH; i++) *index++ = (pixels++)->c;
            p->draw((uint8_t*)p->pallete_index);
        }
        p->frame_ticks = 0;//重新分配时钟周期
        if ((p->frame & 1) || (p->regs[REG_PPUMASK]&PPUMASK_b)) p->frame_ticks += 1;
        p->frame++;//帧数加1
        return 1;
    } else {
        //每行需要341个tick，总共262行(262条扫描线) 341 * 262
        const int scanline = (p->frame_ticks / 341) - 1;//当前扫描线，扫描线从-1开始，Y
        const unsigned int scanline_cycle = p->frame_ticks % 341;//当前扫描线下的第几个tick，X
        const int show_background = (p->regs[REG_PPUMASK] & PPUMASK_b) != 0;//0:不显示背景，1:显示背景
        const int show_sprites = (p->regs[REG_PPUMASK] & PPUMASK_s) != 0;//0:不显示精灵，1:显示精灵
        const int render_enable = show_background || show_sprites;//只要需要显示背景或者精灵，就需要重新渲染
        
        if ((render_enable && scanline >= -1) && (scanline <= 239)) {//可视区域的扫描线
            if (scanline == -1) {
                if (scanline_cycle == 1) { p->regs[REG_PPUSTATUS] &= ~(PPUSTATUS_S | PPUSTATUS_V); }
                if (scanline_cycle == 280) { p->reg_v &= ~0x7be0; p->reg_v |= (p->reg_t & 0x7be0); }
            }else{
                if (scanline_cycle == 0) { ppu_sprites(p, scanline); }
                if ((scanline_cycle >= 1) && (scanline_cycle <= 256)) { ppu_pixel(p, scanline_cycle-1, scanline); }
                if (scanline_cycle > 0 && scanline_cycle < 256 && (scanline_cycle & 7) == 0) ppu_fetch(p);
            }
            if (scanline_cycle == 256) p->reg_v = ppu_incr_y(p);//扫描线换行
            if (scanline_cycle == 257) { p->reg_v &= ~0x41f; p->reg_v |= (p->reg_t & 0x41f); }//0x41f和0x7be0相对
            if (scanline_cycle == 321) { p->attr = p->tile_l = p->tile_h = 0; ppu_fetch(p); p->tile_l <<= 8; p->tile_h <<= 8; p->attr <<= 16; }
            if (scanline_cycle == 329) ppu_fetch(p);
            
        } else if ((scanline >= 240) && (scanline <= 260)) {//21个tick
            if ((scanline == 241) && (scanline_cycle == 1)) {//vblank:回到屏幕左上角
                p->regs[REG_PPUSTATUS] |= PPUSTATUS_V;
                if (p->regs[REG_PPUCTRL] & PPUCTRL_V) cpu_nmi(p->cpu);
            }
        }
        
        p->frame_ticks++;//tick每次加1
        return 0;
    }
}

//对外接口函数
void ppu_init(ppu_context *p)
{
    p->frame = 0; p->frame_ticks = 0;
}

int ppu_step(ppu_context *p)
{
    return ppu_step_i(p);
}

uint8_t ppu_read(ppu_context *p, uint16_t addr)
{
    return ppu_read_r(p, addr);
}

void ppu_write(ppu_context *p, uint16_t addr, uint8_t value)
{
    ppu_write_r(p, addr, value);
}

