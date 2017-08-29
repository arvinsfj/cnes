
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "cnes.h"
#include "mapper.h"

#define	PRG_RAM_SIZE	0x2000

struct mmc2_context {
	uint8_t mirror;
	uint8_t prg_ram[PRG_RAM_SIZE];
	uint32_t pb;		/* PRG bank */
	uint32_t *cbptr[2];	/* CHR bank */
	uint32_t cbsel[2][2];	/* CHR bank select */
};

static uint8_t
mmc2_cpuread(struct cnes_context *av, uint16_t addr)
{
	struct mmc2_context *mc = av->mmc.priv;

#ifdef MMC2_DEBUG
	printf("[%s] addr=$%04X\n", __func__, addr);
#endif

	if (addr >= 0x6000 && addr <= 0x7FFF) {
		/* 8KB PRG RAM bank */
		return mc->prg_ram[addr - 0x6000];
	}

	if (addr >= 0x8000 && addr <= 0x9FFF) {
		/* 8KB switchable PRG ROM bank */
		return av->rom_data[av->prg_start + (addr - 0x8000) + mc->pb];
	}

	if (addr >= 0xA000 && addr <= 0xFFFF) {
		/* Three 8KB PRG ROM banks, fixed to the last three banks */
		return av->rom_data[av->prg_start + (av->prg_len - 0x6000) + (addr - 0xA000)];
	}

	printf("[%s] CPU address $%04X not mapped\n", __func__, addr);
	return 0;
}

static void
mmc2_cpuwrite(struct cnes_context *av, uint16_t addr, uint8_t val)
{
	struct mmc2_context *mc = av->mmc.priv;

#ifdef MMC2_DEBUG
	printf("[%s] addr=$%04X val=$%02X\n", __func__, addr, val);
#endif

	if (addr >= 0xA000 && addr <= 0xAFFF) {
		/* PRG ROM bank select */
		mc->pb = (val & 0xf) * 0x2000;
		return;
	}

	if (addr >= 0xB000 && addr <= 0xBFFF) {
		/* CHR ROM $FD/0000 bank select */
		mc->cbsel[0][0] = (val & 0x1f) * 0x1000;
		return;
	}
	if (addr >= 0xC000 && addr <= 0xCFFF) {
		/* CHR ROM $FE/0000 bank select */
		mc->cbsel[0][1] = (val & 0x1f) * 0x1000;
		return;
	}
	if (addr >= 0xD000 && addr <= 0xDFFF) {
		/* CHR ROM $FD/1000 bank select */
		mc->cbsel[1][0] = (val & 0x1f) * 0x1000;
		return;
	}
	if (addr >= 0xE000 && addr <= 0xEFFF) {
		/* CHR ROM $FE/1000 bank select */
		mc->cbsel[1][1] = (val & 0x1f) * 0x1000;
		return;
	}

	if (addr >= 0xF000 && addr <= 0xFFFF) {
		/* Nametable mirror select */
		mc->mirror = (val & 1) ? ROM_MIRROR_H : ROM_MIRROR_V;
		return;
	}

	printf("[%s] CPU address $%04X not mapped\n", __func__, addr);
}

static uint8_t
mmc2_ppuread(struct cnes_context *av, uint16_t addr)
{
	struct mmc2_context *mc = av->mmc.priv;
	uint8_t val;

#ifdef MMC2_DEBUG
	printf("[%s] addr=$%04X\n", __func__, addr);
#endif

	if (addr >= 0x0000 && addr <= 0x0FFF) {
		/* 4KB switchable CHR ROM bank 0 */
		val = av->rom_data[av->chr_start + (addr - 0x0000) + *mc->cbptr[0]];
		if (addr == 0x0FD8)
			mc->cbptr[0] = &mc->cbsel[0][0];
		else if (addr == 0x0FE8)
			mc->cbptr[0] = &mc->cbsel[0][1];
		return val;
	}
	if (addr >= 0x1000 && addr <= 0x1FFF) {
		/* 4KB switchable CHR ROM bank 1 */
		val = av->rom_data[av->chr_start + (addr - 0x1000) + *mc->cbptr[1]];
		if (addr >= 0x1FD8 && addr <= 0x1FDF)
			mc->cbptr[1] = &mc->cbsel[1][0];
		else if (addr >= 0x1FE8 && addr <= 0x1FEF)
			mc->cbptr[1] = &mc->cbsel[1][1];
		return val;
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
mmc2_ppuwrite(struct cnes_context *av, uint16_t addr, uint8_t val)
{
	struct mmc2_context *mc = av->mmc.priv;

#ifdef MMC2_DEBUG
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
mmc2_init(mapper_context *m, const uint8_t *data, size_t datalen)
{
	struct mmc2_context *mc;

	mc = calloc(1, sizeof(*mc));
	if (mc == NULL)
		return ENOMEM;

	mc->mirror = data[6] & ROM_MIRROR_MASK;
	mc->pb = 0x2000;
	mc->cbptr[0] = &mc->cbsel[0][1];
	mc->cbptr[1] = &mc->cbsel[1][1];

	m->cpuread = mmc2_cpuread;
	m->cpuwrite = mmc2_cpuwrite;
	m->ppuread = mmc2_ppuread;
	m->ppuwrite = mmc2_ppuwrite;
	m->priv = mc;

	return 0;
}

MAPPER_DECL(mmc2, "MMC2", mmc2_init);
