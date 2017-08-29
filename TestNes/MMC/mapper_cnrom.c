
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "cnes.h"
#include "mapper.h"

struct cnrom_context {
	uint8_t mirror;
	uint32_t banksel;	/* CHR bank */
};

static uint8_t
cnrom_cpuread(struct cnes_context *av, uint16_t addr)
{
	//struct cnrom_context *mc = av->m.priv;

#ifdef CNROM_DEBUG
	printf("[%s] addr=$%04X\n", __func__, addr);
#endif

	if (addr >= 0x8000 && addr <= 0xFFFF) {
		/* Fixed PRG ROM */
		return av->rom_data[av->prg_start + ((addr - 0x8000) & (av->prg_len - 1))];
	}

	printf("[%s] CPU address $%04X not mapped\n", __func__, addr);
	return 0;
}

static void
cnrom_cpuwrite(struct cnes_context *av, uint16_t addr, uint8_t val)
{
	struct cnrom_context *mc = av->mmc.priv;

#ifdef CNROM_DEBUG
	printf("[%s] addr=$%04X val=$%02X\n", __func__, addr, val);
#endif

	if (addr >= 0x8000 && addr <= 0xFFFF) {
		/* Bank select */
		mc->banksel = val;
		return;
	}

	printf("[%s] CPU address $%04X not mapped\n", __func__, addr);
}

static uint8_t
cnrom_ppuread(struct cnes_context *av, uint16_t addr)
{
	struct cnrom_context *mc = av->mmc.priv;
	//uint8_t val;

#ifdef CNROM_DEBUG
	printf("[%s] addr=$%04X\n", __func__, addr);
#endif

	if (addr >= 0x0000 && addr <= 0x1FFF) {
		return av->rom_data[av->chr_start + (0x2000 * (mc->banksel & 0x3)) + addr];
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
cnrom_ppuwrite(struct cnes_context *av, uint16_t addr, uint8_t val)
{
	struct cnrom_context *mc = av->mmc.priv;

#ifdef CNROM_DEBUG
	printf("[%s] addr=$%04X val=$%02X\n", __func__, addr, val);
#endif

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
cnrom_init(mapper_context *m, const uint8_t *data, size_t datalen)
{
	struct cnrom_context *mc;

	mc = calloc(1, sizeof(*mc));
	if (mc == NULL)
		return ENOMEM;

	mc->mirror = data[6] & ROM_MIRROR_MASK;

	m->cpuread = cnrom_cpuread;
	m->cpuwrite = cnrom_cpuwrite;
	m->ppuread = cnrom_ppuread;
	m->ppuwrite = cnrom_ppuwrite;
	m->priv = mc;

	return 0;
}

MAPPER_DECL(cnrom, "CNROM", cnrom_init);
