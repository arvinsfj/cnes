
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "cnes.h"
#include "mapper.h"

#define	PRG_RAM_SIZE	0x2000

struct nrom_context {
	uint8_t mirror;

	uint8_t prg_ram[PRG_RAM_SIZE];
};

static uint8_t
nrom_cpuread(struct cnes_context *av, uint16_t addr)
{
	if (addr >= 0x8000 && addr <= 0xFFFF)
		return av->rom_data[av->prg_start + ((addr - 0x8000) & (av->prg_len - 1))];

	printf("[%s] CPU address $%04X not mapped\n", __func__, addr);
	return 0;
}

static void
nrom_cpuwrite(struct cnes_context *av, uint16_t addr, uint8_t val)
{
	printf("[%s] CPU address $%04X not mapped\n", __func__, addr);
}

static uint8_t
nrom_ppuread(struct cnes_context *av, uint16_t addr)
{
	struct nrom_context *mc = av->mmc.priv;

	if (addr >= 0x0000 && addr <= 0x1FFF) {
		/* Pattern table */
		if (av->chr_len) {
			return av->rom_data[av->chr_start + (addr & (av->chr_len - 1))];
		} else {
			return mc->prg_ram[addr];
		}
	}

	if (addr >= 0x2000 && addr <= 0x3EFF) {
		/* Nametable */
		if (addr >= 0x3000)
			addr -= 0x1000;

		off_t off;
		switch (mc->mirror) {
		case ROM_MIRROR_H:
			off = (addr & 0x3FF) + (addr < 0x2800 ? 0 : 0x400);
			break;
		case ROM_MIRROR_V:
			off = (addr & 0x7FF);
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
nrom_ppuwrite(struct cnes_context *av, uint16_t addr, uint8_t val)
{
	struct nrom_context *mc = av->mmc.priv;

	if (av->chr_len == 0 && addr >= 0x0000 && addr <= 0x1FFF) {
		mc->prg_ram[addr] = val;
		return;
	}

	if (addr >= 0x2000 && addr <= 0x3EFF) {
		/* Nametable */
		if (addr >= 0x3000)
			addr -= 0x1000;

		off_t off;
		switch (mc->mirror) {
		case ROM_MIRROR_H:
			off = (addr & 0x3FF) + (addr < 0x2800 ? 0 : 0x400);
			break;
		case ROM_MIRROR_V:
			off = (addr & 0x7FF);
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
nrom_init(mapper_context *m, const uint8_t *data, size_t datalen)
{
	struct nrom_context *mc;

	mc = calloc(1, sizeof(*mc));
	if (mc == NULL)
		return ENOMEM;

	mc->mirror = data[6] & ROM_MIRROR_MASK;

	m->cpuread = nrom_cpuread;
	m->cpuwrite = nrom_cpuwrite;
	m->ppuread = nrom_ppuread;
	m->ppuwrite = nrom_ppuwrite;
	m->priv = mc;

	return 0;
}

MAPPER_DECL(nrom, "NROM", nrom_init);
