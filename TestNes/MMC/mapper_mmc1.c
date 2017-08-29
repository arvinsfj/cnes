
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "cnes.h"
#include "mapper.h"

#define	PRG_RAM_SIZE	0x2000
#define	CHR_RAM_SIZE	0x2000

#define	MMC1_REG_CONTROL	0
#define	MMC1_REG_CHR0		1
#define	MMC1_REG_CHR1		2
#define	MMC1_REG_PRG		3

struct mmc1_context {
	uint8_t prg_ram[PRG_RAM_SIZE];
	uint8_t chr_ram[CHR_RAM_SIZE];

	uint8_t sr;

	uint8_t regs[4];
};

static uint8_t
mmc1_cpuread(struct cnes_context *av, uint16_t addr)
{
	struct mmc1_context *mc = av->mmc.priv;
	unsigned int bank;

#ifdef MMC1_DEBUG
	printf("[%s] addr=$%04X\n", __func__, addr);
#endif

	if (addr >= 0x6000 && addr <= 0x7FFF) {
		/* 8KB PRG RAM bank */
		return mc->prg_ram[addr - 0x6000];
	}

	if (addr >= 0x8000 && addr <= 0xFFFF) {
		switch ((mc->regs[MMC1_REG_CONTROL] >> 2) & 0x3) {
		case 0:
		case 1:
			/* Switch 32KB at $8000, ignoring low bit of bank number */
			bank = mc->regs[MMC1_REG_PRG] & 0xe;
			return av->rom_data[av->prg_start + (addr & 0x7FFF) + (bank * 0x8000)];
		case 2:
			/* Fix first bank at $8000 and switch 16KB bank at $C000 */
			if (addr <= 0xBFFF) {
				return av->rom_data[av->prg_start + (addr & 0x3FFF) + 0x0000];
			} else {
				bank = mc->regs[MMC1_REG_PRG] & 0xf;
				return av->rom_data[av->prg_start + (addr & 0x3FFF) + (bank * 0x4000)];
			}
		case 3:
			/* Fix last bank at $C000 and switch 16KB bank at $8000 */
			if (addr <= 0xBFFF) {
				bank = mc->regs[MMC1_REG_PRG] & 0xf;
				return av->rom_data[av->prg_start + (addr & 0x3FFF) + (bank * 0x4000)];
			} else {
				return av->rom_data[av->prg_start + (addr & 0x3FFF) + (av->prg_len - 0x4000)];
			}
		}
	}

	printf("[%s] CPU address $%04X not mapped\n", __func__, addr);
	return 0;
}

static void
mmc1_cpuwrite(struct cnes_context *av, uint16_t addr, uint8_t val)
{
	struct mmc1_context *mc = av->mmc.priv;

#ifdef MMC1_DEBUG
	printf("[%s] addr=$%04X val=$%02X\n", __func__, addr, val);
#endif

	if (addr >= 0x6000 && addr <= 0x7FFF) {
		/* 8KB PRG RAM bank */
		mc->prg_ram[addr - 0x6000] = val;
		return;
	}

	if (addr >= 0x8000 && addr <= 0xFFFF) {
		if (val & 0x80) {
			mc->sr = 0x10;	/* Initial state */
		} else {
			int empty = mc->sr & 1;
			mc->sr = (mc->sr >> 1) | ((val & 1) << 4);
			if (empty) {
#ifdef MMC1_DEBUG
				printf("[%s] set MMC1 reg %d -> %02X\n", __func__, (addr >> 13) & 3, mc->sr);
#endif
				mc->regs[(addr >> 13) & 3] = mc->sr;
				mc->sr = 0x10;
			}
		}
		return;
	}

	printf("[%s] CPU address $%04X not mapped\n", __func__, addr);
}

static uint8_t
mmc1_ppuread(struct cnes_context *av, uint16_t addr)
{
	struct mmc1_context *mc = av->mmc.priv;

#ifdef MMC1_DEBUG
	printf("[%s] addr=$%04X\n", __func__, addr);
#endif

	if (addr >= 0x0000 && addr <= 0x1FFF) {
		unsigned int bank;
		switch ((mc->regs[MMC1_REG_CONTROL] >> 4) & 0x1) {
		case 0:
			/* Switch 8KB at a time */
			bank = mc->regs[MMC1_REG_CHR0] & 0x1e;
			if (av->chr_len) {
				return av->rom_data[av->chr_start + (addr & 0x1FFF) + (bank * 0x2000)];
			} else {
				return mc->chr_ram[(addr & 0x1FFF) + (bank * 0x2000)];
			}
		case 1:
			/* Switch two separate 4KB banks */
			bank = mc->regs[MMC1_REG_CHR0 + ((addr >> 12) & 1)];
			if (av->chr_len) {
				return av->rom_data[av->chr_start + (addr & 0xFFF) + (bank * 0x1000)];
			} else {
				return mc->chr_ram[(addr & 0x1FFF) + (bank * 0x1000)];
			}
		}
	}

	if (addr >= 0x2000 && addr <= 0x3EFF) {
		/* Nametable */
		if (addr >= 0x3000)
			addr -= 0x1000;

		off_t off = 0;
		switch (mc->regs[MMC1_REG_CONTROL] & 0x3) {
		case 0:
			/* One-screen, lower bank */
			off = (addr & 0x3FF) + 0x0000;
			break;
		case 1:
			/* One-screen, upper bank */
			off = (addr & 0x3FF) + 0x0400;
			break;
		case 2:
			/* Vertical */
			off = (addr & 0x7FF);
			break;
		case 3:
			/* Horizontal */
			off = (addr & 0x3FF) + (addr < 0x2800 ? 0 : 0x400);
			break;
		}
		return av->vram[off];
	}

	printf("[%s] PPU address $%04X not mapped\n", __func__, addr);
	return 0;
}

static void
mmc1_ppuwrite(struct cnes_context *av, uint16_t addr, uint8_t val)
{
	struct mmc1_context *mc = av->mmc.priv;

#ifdef MMC1_DEBUG
	printf("[%s] addr=$%04X val=$%02X\n", __func__, addr, val);
#endif

	if (av->chr_len == 0 && addr >= 0x0000 && addr <= 0x1FFF) {
		mc->chr_ram[addr] = val;
		return;
	}

	if (addr >= 0x2000 && addr <= 0x3EFF) {
		/* Nametable */
		if (addr >= 0x3000)
			addr -= 0x1000;

		off_t off = 0;
		switch (mc->regs[MMC1_REG_CONTROL] & 0x3) {
		case 0:
			/* One-screen, lower bank */
			off = (addr & 0x3FF) + 0x0000;
			break;
		case 1:
			/* One-screen, upper bank */
			off = (addr & 0x3FF) + 0x0400;
			break;
		case 2:
			/* Vertical */
			off = (addr & 0x7FF);
			break;
		case 3:
			/* Horizontal */
			off = (addr & 0x3FF) + (addr < 0x2800 ? 0 : 0x400);
			break;
		}
		av->vram[off] = val;
		return;
	}

	printf("[%s] PPU address $%04X not mapped\n", __func__, addr);
}

static int
mmc1_init(mapper_context *m, const uint8_t *data, size_t datalen)
{
	struct mmc1_context *mc;

	mc = calloc(1, sizeof(*mc));
	if (mc == NULL)
		return ENOMEM;

	mc->regs[MMC1_REG_CONTROL] = 0x3 << 2;
	mc->sr = 0x10;

	m->cpuread = mmc1_cpuread;
	m->cpuwrite = mmc1_cpuwrite;
	m->ppuread = mmc1_ppuread;
	m->ppuwrite = mmc1_ppuwrite;
	m->priv = mc;

	return 0;
}

MAPPER_DECL(mmc1, "MMC1", mmc1_init);
