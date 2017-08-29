
#ifndef _MAPPER_H
#define _MAPPER_H

#include <stdint.h>
#include <stdio.h>

#define ROM_MIRROR_H        0x0
#define ROM_MIRROR_V        0x1
#define	ROM_MIRROR_MASK		0x9

struct cnes_context;

typedef struct {
	uint8_t	(*cpuread)(struct cnes_context *, uint16_t);
	void	(*cpuwrite)(struct cnes_context *, uint16_t, uint8_t);
	uint8_t	(*ppuread)(struct cnes_context *, uint16_t);
	void	(*ppuwrite)(struct cnes_context *, uint16_t, uint8_t);
	void	*priv;
} mapper_context;

typedef struct {
	const char	*descr;
	int		(*init)(mapper_context *, const uint8_t *, size_t);
} mapper_impl;

#define	MAPPER_DECL(n, d, i)					\
    mapper_impl mapper_ ## n ## _impl = {		\
		.descr = (d),					\
		.init = (i)					\
	}

#define	MAPPER_IMPL(name)					\
	mapper_ ## name ## _impl

int	mapper_init(mapper_context *, const uint8_t *, size_t);

#endif /* !_MAPPER_H */
