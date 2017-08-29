//
//  cnes.h
//  TestNes
//
//  Created by arvin on 2017/8/16.
//  Copyright © 2017年 com.fuwo. All rights reserved.
//

#ifndef cnes_h
#define cnes_h

#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ccpu.h"
#include "cppu.h"
#include "mapper.h"
#include "capu.h"

struct cnes_context
{
    cpu_context cpu;
    ppu_context ppu;
    mapper_context mmc;
    apu_context apu;
    
    uint8_t *rom_data;
    size_t rom_len;
    off_t prg_start;
    size_t prg_len;
    off_t chr_start;
    size_t chr_len;
    
    uint8_t ctrl[4];//手柄
    uint8_t ram[0x800];//2k ram
    uint8_t vram[0x800];//2k vram (Nametable和Attributes）(共2个，其他是镜像)
    uint8_t palette[0x20];//32字节 调色板  2个调色板，每个调色板16字节，分4组，每组4字节
} ;

int cnes_init(const char* romname);

#endif /* cnes_h */
