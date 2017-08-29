
#include "mapper.h"

extern mapper_impl MAPPER_IMPL(cnrom);
extern mapper_impl MAPPER_IMPL(mmc1);
extern mapper_impl MAPPER_IMPL(mmc2);
extern mapper_impl MAPPER_IMPL(mmc3);
extern mapper_impl MAPPER_IMPL(nrom);
extern mapper_impl MAPPER_IMPL(unrom);
extern mapper_impl MAPPER_IMPL(aorom);

static mapper_impl *mappers[] = {
	[0] = &MAPPER_IMPL(nrom),
	[1] = &MAPPER_IMPL(mmc1),
	[2] = &MAPPER_IMPL(unrom),
	[3] = &MAPPER_IMPL(cnrom),
	[4] = &MAPPER_IMPL(mmc3),
    [7] = &MAPPER_IMPL(aorom),
	[9] = &MAPPER_IMPL(mmc2),
};

int mapper_init(mapper_context *m, const uint8_t *data, size_t datalen)
{
	uint8_t mapper_number = (data[6] >> 4) | (data[7] & 0xf0);
    mapper_impl *impl;
	int error;

	if (mapper_number >= sizeof(mappers)/sizeof(mappers[0]) || mappers[mapper_number] == NULL) {
		printf("Mapper #%d not supported\n", mapper_number);
		return -1;
	}

	impl = mappers[mapper_number];
	error = impl->init(m, data, datalen);
	if (error != 0) {
		printf("Mapper #%d (%s) init failed: %d\n", mapper_number, impl->descr, error);
		return error;
	}

	printf("Using mapper #%d (%s)\n", mapper_number, impl->descr);

	return 0;
}
