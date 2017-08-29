#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "cnes.h"
#include "mapper.h"

#define	PRG_RAM_SIZE	0x2000
#define	CHR_RAM_SIZE	0x2000

struct aorom_context {
    uint8_t mirror;
    uint32_t banksel;	/* PRG bank */
    
    uint8_t prg_ram[PRG_RAM_SIZE];
    uint8_t chr_ram[CHR_RAM_SIZE];
};

static uint8_t
aorom_cpuread(struct cnes_context *av, uint16_t addr)
{
    struct aorom_context *mc = av->mmc.priv;
    
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        /* 8KB PRG RAM bank */
        return mc->prg_ram[addr - 0x6000];
    }
    
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        /* 16KB switchable PRG ROM bank */
        return av->rom_data[av->prg_start + (addr - 0x8000) + (mc->banksel * 0x8000)];
    }
    
    printf("[%s] CPU address $%04X not mapped\n", __func__, addr);
    return 0;
}

static void
aorom_cpuwrite(struct cnes_context *av, uint16_t addr, uint8_t val)
{
    struct aorom_context *mc = av->mmc.priv;
    
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        /* 8KB PRG RAM bank */
        mc->prg_ram[addr - 0x6000] = val;
        return;
    }
    
    if (addr >= 0x8000 && addr <= 0xFFFF){
        mc->banksel = val & 0x7;
        if ((val & 0x10) == 0x00) {
            mc->mirror = 2;
        }
        if ((val & 0x10) == 0x10) {
            mc->mirror = 3;
        }
        return;
    }
    
    printf("[%s] CPU address $%04X not mapped\n", __func__, addr);
}

static uint8_t
aorom_ppuread(struct cnes_context *av, uint16_t addr)
{
    struct aorom_context *mc = av->mmc.priv;
    
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        /* Pattern table */
        return mc->chr_ram[addr];
    }
    
    if (addr >= 0x2000 && addr <= 0x3EFF) {
        /* Nametable */
        if (addr >= 0x3000)
            addr -= 0x1000;
        
        off_t off;
        switch (mc->mirror) {
            case 2:
                off = (addr & 0x3FF);
                break;
            case 3:
                off = (addr & 0x3FF) + 0x400;
                break;
            default:
                assert("Unsupported mirror mode" == NULL);
        }
        return av->vram[off];
    }
    
    printf("[%s] PPU address $%04X not mapped\n", __func__, addr);
    return 0;
}

static void
aorom_ppuwrite(struct cnes_context *av, uint16_t addr, uint8_t val)
{
    struct aorom_context *mc = av->mmc.priv;
    
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        /* Pattern table */
        mc->chr_ram[addr] = val;
        return;
    }
    
    if (addr >= 0x2000 && addr <= 0x3EFF) {
        /* Nametable */
        if (addr >= 0x3000)
            addr -= 0x1000;
        
        off_t off;
        switch (mc->mirror) {
            case 2:
                off = (addr & 0x3FF);
                break;
            case 3:
                off = (addr & 0x3FF) + 0x400;
                break;
            default:
                assert("Unsupported mirror mode" == NULL);
        }
        av->vram[off] = val;
        return;
    }
    
    printf("[%s] PPU address $%04X not mapped\n", __func__, addr);
}

static int
aorom_init(mapper_context *m, const uint8_t *data, size_t datalen)
{
    struct aorom_context *mc;
    
    mc = calloc(1, sizeof(*mc));
    if (mc == NULL)
        return ENOMEM;
    
    mc->mirror = data[6] & ROM_MIRROR_MASK;
    
    m->cpuread = aorom_cpuread;
    m->cpuwrite = aorom_cpuwrite;
    m->ppuread = aorom_ppuread;
    m->ppuwrite = aorom_ppuwrite;
    m->priv = mc;
    
    return 0;
}

MAPPER_DECL(aorom, "AOROM", aorom_init);
