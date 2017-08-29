#include "capu.h"

uint8_t lengthTable[] = {
    10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30,
};

uint8_t dutyTable[4][8] = {
    {0, 1, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 0, 0, 0, 0, 0},
    {0, 1, 1, 1, 1, 0, 0, 0},
    {1, 0, 0, 1, 1, 1, 1, 1},
};

uint8_t triangleTable[] = {
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
};

uint16_t noiseTable[] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068,
};

uint8_t dmcTable[] = {
    214, 190, 170, 160, 143, 127, 113, 107, 95, 80, 71, 64, 53, 42, 36, 27,
};

float pulseTable[31];//float32
float tndTable [203];
//初始化ptnd表格数据
void init()
{
    for (int i = 0; i < 31; i++) {
        pulseTable[i] = 95.52 / (8128.0/i + 100);
    }
    for (int i = 0; i < 203; i++) {
        tndTable[i] = 163.67 / (24329.0/i + 100);
    }
}

//定义Filters
// First order filters are defined by the following parameters.
// y[n] = B0*x[n] + B1*x[n-1] - A1*y[n-1]
float filter_step(FirstOrderFilter *f, float x)
{
    f->prevY = f->B0*x + f->B1*f->prevX - f->A1*f->prevY;
    f->prevX = x;
    return f->prevY;
}

// sampleRate: samples per second
// cutoffFreq: oscillations per second
void filter_lowPassFilter(FirstOrderFilter *filter, float sampleRate, float cutoffFreq)
{
    float c = sampleRate / 3.1415926 / cutoffFreq;
    float a0i = 1 / (1 + c);
    filter->B0 = a0i;
    filter->B1 = a0i;
    filter->A1 = (1 - c) * a0i;
}

void filter_highPassFilter(FirstOrderFilter *filter, float sampleRate, float cutoffFreq)
{
    float c = sampleRate / 3.1415926 / cutoffFreq;
    float a0i = 1 / (1 + c);
    filter->B0 = c * a0i;
    filter->B1 = -c * a0i;
    filter->A1 = (1 - c) * a0i;
}

float filter_chain_step(FirstOrderFilter fc[], uint8_t fc_len, float x)
{
    if ((fc != NULL) && fc_len) {
        for (int i = 0; i < fc_len; i++) {
            x = filter_step(&fc[i], x);
        }
    }
    return x;
}
//Filter定义结束

// DMC 函数

void dmc_writeControl(DMC *d, uint8_t value)
{
    d->irq = (value&0x80) == 0x80;
    d->loop = (value&0x40) == 0x40;
    d->tickPeriod = dmcTable[(value&0x0F)];
}

void dmc_writeValue(DMC *d, uint8_t value)
{
    d->value = value & 0x7F;
}

void dmc_writeAddress(DMC *d, uint8_t value)
{
    // Sample address = %11AAAAAA.AA000000
    d->sampleAddress = 0xC000 | (((uint16_t)(value)) << 6);
}

void dmc_writeLength(DMC *d, uint8_t value)
{
    // Sample length = %0000LLLL.LLLL0001
    d->sampleLength = (((uint16_t)(value)) << 4) | 1;
}

void dmc_restart(DMC *d)
{
    d->currentAddress = d->sampleAddress;
    d->currentLength = d->sampleLength;
}

void dmc_stepReader(DMC *d)
{
    if ((d->currentLength > 0) && (d->bitCount == 0)) {
        d->cpu->stall += 4;
        d->shiftRegister = d->cpu->read8(d->currentAddress);
        d->bitCount = 8;
        d->currentAddress++;
        if (d->currentAddress == 0) {
            d->currentAddress = 0x8000;
        }
        d->currentLength--;
        if ((d->currentLength == 0) && d->loop) {
            dmc_restart(d);
        }
    }
}

void dmc_stepShifter(DMC *d)
{
    if (d->bitCount == 0) {
        return;
    }
    if ((d->shiftRegister&1) == 1) {
        if (d->value <= 125) {
            d->value += 2;
        }
    } else {
        if (d->value >= 2) {
            d->value -= 2;
        }
    }
    d->shiftRegister >>= 1;
    d->bitCount--;
}

void dmc_stepTimer(DMC *d)
{
    if (!(d->enabled)) {
        return;
    }
    dmc_stepReader(d);
    if (d->tickValue == 0) {
        d->tickValue = d->tickPeriod;
        dmc_stepShifter(d);
    } else {
        d->tickValue--;
    }
}

uint8_t dmc_output(DMC *d)
{
    return d->value;
}


// Noise 函数

void noise_writeControl(Noise *n, uint8_t value)
{
    n->lengthEnabled = ((value>>5)&1) == 0;
    n->envelopeLoop = ((value>>5)&1) == 1;
    n->envelopeEnabled = ((value>>4)&1) == 0;
    n->envelopePeriod = value & 15;
    n->constantVolume = value & 15;
    n->envelopeStart = 1;
}

void noise_writePeriod(Noise *n, uint8_t value)
{
    n->mode = (value&0x80) == 0x80;
    n->timerPeriod = noiseTable[(value&0x0F)];
}

void noise_writeLength(Noise *n, uint8_t value)
{
    n->lengthValue = lengthTable[((value>>3)&31)];
    n->envelopeStart = 1;
}

void noise_stepTimer(Noise *n)
{
    if (n->timerValue == 0) {
        n->timerValue = n->timerPeriod;
        uint8_t shift = 0;
        if (n->mode) {
            shift = 6;
        } else {
            shift = 1;
        }
        uint16_t b1 = n->shiftRegister & 1;
        uint16_t b2 = (n->shiftRegister >> shift) & 1;
        n->shiftRegister >>= 1;
        n->shiftRegister |= (b1 ^ b2) << 14;
    } else {
        n->timerValue--;
    }
}

void noise_stepEnvelope(Noise *n)
{
    if (n->envelopeStart) {
        n->envelopeVolume = 15;
        n->envelopeValue = n->envelopePeriod;
        n->envelopeStart = 0;
    } else if (n->envelopeValue > 0) {
        n->envelopeValue--;
    } else {
        if (n->envelopeVolume > 0) {
            n->envelopeVolume--;
        } else if (n->envelopeLoop) {
            n->envelopeVolume = 15;
        }
        n->envelopeValue = n->envelopePeriod;
    }
}

void noise_stepLength(Noise *n)
{
    if (n->lengthEnabled && (n->lengthValue > 0)) {
        n->lengthValue--;
    }
}

uint8_t noise_output(Noise *n)
{
    if (!(n->enabled)) {
        return 0;
    }
    if (n->lengthValue == 0) {
        return 0;
    }
    if ((n->shiftRegister&1) == 1) {
        return 0;
    }
    if (n->envelopeEnabled) {
        return n->envelopeVolume;
    } else {
        return n->constantVolume;
    }
}

// Triangle 函数

void triangle_writeControl(Triangle *t, uint8_t value)
{
    t->lengthEnabled = ((value>>7)&1) == 0;
    t->counterPeriod = value & 0x7F;
}

void triangle_writeTimerLow(Triangle *t, uint8_t value)
{
    t->timerPeriod = (t->timerPeriod & 0xFF00) | ((uint16_t)(value));
}

void triangle_writeTimerHigh(Triangle *t, uint8_t value)
{
    t->lengthValue = lengthTable[((value>>3)&31)];
    t->timerPeriod = (t->timerPeriod & 0x00FF) | (((uint16_t)(value&7)) << 8);
    t->timerValue = t->timerPeriod;
    t->counterReload = 1;
}

void triangle_stepTimer(Triangle *t)
{
    if (t->timerValue == 0) {
        t->timerValue = t->timerPeriod;
        if ((t->lengthValue > 0) && (t->counterValue > 0)) {
            t->dutyValue = (t->dutyValue + 1) % 32;
        }
    } else {
        t->timerValue--;
    }
}

void triangle_stepLength(Triangle *t)
{
    if (t->lengthEnabled && (t->lengthValue > 0)) {
        t->lengthValue--;
    }
}

void triangle_stepCounter(Triangle *t)
{
    if (t->counterReload) {
        t->counterValue = t->counterPeriod;
    } else if (t->counterValue > 0) {
        t->counterValue--;
    }
    if (t->lengthEnabled) {
        t->counterReload = 0;
    }
}

uint8_t triangle_output(Triangle *t)
{
    if (!(t->enabled)) {
        return 0;
    }
    if (t->lengthValue == 0) {
        return 0;
    }
    if (t->counterValue == 0) {
        return 0;
    }
    return triangleTable[t->dutyValue];
}

// Pulse 函数

void pulse_writeControl(Pulse *p, uint8_t value)
{
    p->dutyMode = (value >> 6) & 3;
    p->lengthEnabled = ((value>>5)&1) == 0;
    p->envelopeLoop = ((value>>5)&1) == 1;
    p->envelopeEnabled = ((value>>4)&1) == 0;
    p->envelopePeriod = value & 15;
    p->constantVolume = value & 15;
    p->envelopeStart = 1;
}

void pulse_writeSweep(Pulse *p, uint8_t value)
{
    p->sweepEnabled = ((value>>7)&1) == 1;
    p->sweepPeriod = ((value>>4)&7) + 1;
    p->sweepNegate = ((value>>3)&1) == 1;
    p->sweepShift = value & 7;
    p->sweepReload = 1;
}

void pulse_writeTimerLow(Pulse *p, uint8_t value)
{
    p->timerPeriod = (p->timerPeriod & 0xFF00) | ((uint16_t)(value));
}

void pulse_writeTimerHigh(Pulse *p, uint8_t value)
{
    p->lengthValue = lengthTable[((value>>3)&31)];
    p->timerPeriod = (p->timerPeriod & 0x00FF) | (((uint16_t)(value&7)) << 8);
    p->envelopeStart = 1;
    p->dutyValue = 0;
}

void pulse_stepTimer(Pulse *p)
{
    if (p->timerValue == 0) {
        p->timerValue = p->timerPeriod;
        p->dutyValue = (p->dutyValue + 1) % 8;
    } else {
        p->timerValue--;
    }
}

void pulse_stepEnvelope(Pulse *p)
{
    if (p->envelopeStart) {
        p->envelopeVolume = 15;
        p->envelopeValue = p->envelopePeriod;
        p->envelopeStart = 0;
    } else if (p->envelopeValue > 0) {
        p->envelopeValue--;
    } else {
        if (p->envelopeVolume > 0) {
            p->envelopeVolume--;
        } else if (p->envelopeLoop) {
            p->envelopeVolume = 15;
        }
        p->envelopeValue = p->envelopePeriod;
    }
}

void pulse_sweep(Pulse *p)
{
    uint16_t delta = p->timerPeriod >> p->sweepShift;
    if (p->sweepNegate) {
        p->timerPeriod -= delta;
        if (p->channel == 1) {
            p->timerPeriod--;
        }
    } else {
        p->timerPeriod += delta;
    }
}

void pulse_stepSweep(Pulse *p)
{
    if (p->sweepReload) {
        if (p->sweepEnabled && p->sweepValue == 0) {
            pulse_sweep(p);
        }
        p->sweepValue = p->sweepPeriod;
        p->sweepReload = 0;
    } else if (p->sweepValue > 0) {
        p->sweepValue--;
    } else {
        if (p->sweepEnabled) {
            pulse_sweep(p);
        }
        p->sweepValue = p->sweepPeriod;
    }
}

void pulse_stepLength(Pulse *p)
{
    if (p->lengthEnabled && (p->lengthValue > 0)) {
        p->lengthValue--;
    }
}

uint8_t pulse_output(Pulse *p)
{
    if (!(p->enabled)) {
        return 0;
    }
    if (p->lengthValue == 0) {
        return 0;
    }
    if (dutyTable[p->dutyMode][p->dutyValue] == 0) {
        return 0;
    }
    if ((p->timerPeriod < 8) || (p->timerPeriod > 0x7FF)) {
        return 0;
    }
    // if !p.sweepNegate && p.timerPeriod+(p.timerPeriod>>p.sweepShift) > 0x7FF {
    //     return 0
    // }
    if (p->envelopeEnabled) {
        return p->envelopeVolume;
    } else {
        return p->constantVolume;
    }
}


// APU 函数

void stepEnvelope(apu_context *);
void stepSweep(apu_context *);
void stepLength(apu_context *);
void writeFrameCounter(apu_context *apu, uint8_t value)
{
    apu->framePeriod = 4 + ((value>>7)&1);
    apu->frameIRQ = ((value>>6)&1) == 0;
    // apu.frameValue = 0
    if (apu->framePeriod == 5) {
        stepEnvelope(apu);
        stepSweep(apu);
        stepLength(apu);
    }
}

void writeControl(apu_context *apu, uint8_t value)
{
    apu->pulse1.enabled = (value&1) == 1;
    apu->pulse2.enabled = (value&2) == 2;
    apu->triangle.enabled = (value&4) == 4;
    apu->noise.enabled = (value&8) == 8;
    apu->dmc.enabled = (value&16) == 16;
    if (!(apu->pulse1.enabled)) {
        apu->pulse1.lengthValue = 0;
    }
    if (!(apu->pulse2.enabled)) {
        apu->pulse2.lengthValue = 0;
    }
    if (!(apu->triangle.enabled)) {
        apu->triangle.lengthValue = 0;
    }
    if (!(apu->noise.enabled)) {
        apu->noise.lengthValue = 0;
    }
    if (!(apu->dmc.enabled)) {
        apu->dmc.currentLength = 0;
    } else {
        if (apu->dmc.currentLength == 0) {
            dmc_restart(&(apu->dmc));
        }
    }
}

void apu_write_r(apu_context *apu, uint16_t addr, uint8_t value)
{
    switch (addr) {
        case 0x4000:
            pulse_writeControl(&(apu->pulse1), value);
            break;
        case 0x4001:
            pulse_writeSweep(&(apu->pulse1), value);
            break;
        case 0x4002:
            pulse_writeTimerLow(&(apu->pulse1), value);
            break;
        case 0x4003:
            pulse_writeTimerHigh(&(apu->pulse1), value);
            break;
        case 0x4004:
            pulse_writeControl(&(apu->pulse2), value);
            break;
        case 0x4005:
            pulse_writeSweep(&(apu->pulse2), value);
            break;
        case 0x4006:
            pulse_writeTimerLow(&(apu->pulse2), value);
            break;
        case 0x4007:
            pulse_writeTimerHigh(&(apu->pulse2), value);
            break;
        case 0x4008:
            triangle_writeControl(&(apu->triangle), value);
            break;
        case 0x4009:
            break;
        case 0x4010:
            dmc_writeControl(&(apu->dmc), value);
            break;
        case 0x4011:
            dmc_writeValue(&(apu->dmc), value);
            break;
        case 0x4012:
            dmc_writeAddress(&(apu->dmc), value);
            break;
        case 0x4013:
            dmc_writeLength(&(apu->dmc), value);
            break;
        case 0x400A:
            triangle_writeTimerLow(&(apu->triangle), value);
            break;
        case 0x400B:
            triangle_writeTimerHigh(&(apu->triangle), value);
            break;
        case 0x400C:
            noise_writeControl(&(apu->noise), value);
            break;
        case 0x400D:
            break;
        case 0x400E:
            noise_writePeriod(&(apu->noise), value);
            break;
        case 0x400F:
            noise_writeLength(&(apu->noise), value);
            break;
        case 0x4015:
            writeControl(apu, value);
            break;
        case 0x4017:
            writeFrameCounter(apu, value);
            break;
        default:
            break;
    }
}

uint8_t readStatus(apu_context *apu)
{
    uint8_t result = 0;
    if (apu->pulse1.lengthValue > 0) {
        result |= 1;
    }
    if (apu->pulse2.lengthValue > 0) {
        result |= 2;
    }
    if (apu->triangle.lengthValue > 0) {
        result |= 4;
    }
    if (apu->noise.lengthValue > 0) {
        result |= 8;
    }
    if (apu->dmc.currentLength > 0) {
        result |= 16;
    }
    return result;
}

uint8_t apu_read_r(apu_context *apu, uint16_t addr)
{
    switch (addr) {
        case 0x4015:
            return readStatus(apu);
        default:
            return 0;
    }
    return 0;
}

void stepEnvelope(apu_context *apu)
{
    pulse_stepEnvelope(&(apu->pulse1));
    pulse_stepEnvelope(&(apu->pulse2));
    triangle_stepCounter(&(apu->triangle));
    noise_stepEnvelope(&(apu->noise));
}

void stepSweep(apu_context *apu)
{
    pulse_stepSweep(&(apu->pulse1));
    pulse_stepSweep(&(apu->pulse2));
}

void stepLength(apu_context *apu)
{
    pulse_stepLength(&(apu->pulse1));
    pulse_stepLength(&(apu->pulse2));
    triangle_stepLength(&(apu->triangle));
    noise_stepLength(&(apu->noise));
}

void fireIRQ(apu_context *apu)
{
    if (apu->frameIRQ) {
        cpu_irq(apu->cpu);
    }
}

float output(apu_context *apu)
{
    uint8_t p1 = pulse_output(&(apu->pulse1));
    uint8_t p2 = pulse_output(&(apu->pulse2));
    uint8_t t = triangle_output(&(apu->triangle));
    uint8_t n = noise_output(&(apu->noise));
    uint8_t d = dmc_output(&(apu->dmc));
    float pulseOut = pulseTable[p1+p2];
    float tndOut = tndTable[3*t+2*n+d];
    return (float)(pulseOut + tndOut);
}

void sendSample(apu_context *apu)
{
    if (apu->play) {
        apu->play(filter_chain_step(apu->filterChain, 3, output(apu)));
    }
}

// mode 0:    mode 1:       function
// ---------  -----------  -----------------------------
//  - - - f    - - - - -    IRQ (if bit 6 is clear)
//  - l - l    l - l - -    Length counter and sweep
//  e e e e    e e e e -    Envelope and linear counter
void stepFrameCounter(apu_context *apu)
{
    switch (apu->framePeriod) {
        case 4:{
            apu->frameValue = (apu->frameValue + 1) % 4;
            switch (apu->frameValue) {
                case 0:
                case 2:
                    stepEnvelope(apu);
                    break;
                case 1:
                    stepEnvelope(apu);
                    stepSweep(apu);
                    stepLength(apu);
                    break;
                case 3:
                    stepEnvelope(apu);
                    stepSweep(apu);
                    stepLength(apu);
                    fireIRQ(apu);
                    break;
            }
            break;
        }
        case 5:{
            apu->frameValue = (apu->frameValue + 1) % 5;
            switch (apu->frameValue) {
                case 1:
                case 3:
                    stepEnvelope(apu);
                    break;
                case 0:
                case 2:
                    stepEnvelope(apu);
                    stepSweep(apu);
                    stepLength(apu);
                    break;
            }
            break;
        }
    }
}

void stepTimer(apu_context *apu)
{
    if ((apu->cycle%2) == 0) {
        pulse_stepTimer(&(apu->pulse1));
        pulse_stepTimer(&(apu->pulse2));
        noise_stepTimer(&(apu->noise));
        dmc_stepTimer(&(apu->dmc));
    }
    triangle_stepTimer(&(apu->triangle));
}

void apu_step_i(apu_context *apu)
{
    uint64_t cycle1 = apu->cycle;
    apu->cycle++;
    uint64_t cycle2 = apu->cycle;
    stepTimer(apu);
    int f1 = (int)(((double)(cycle1)) / FRAME_RATE);
    int f2 = (int)(((double)(cycle2)) / FRAME_RATE);
    if (f1 != f2) {
        stepFrameCounter(apu);
    }
    int s1 = (int)(((double)(cycle1)) / SAMPLE_RATE);
    int s2 = (int)(((double)(cycle2)) / SAMPLE_RATE);
    if (s1 != s2) {
        sendSample(apu);
    }
}

//对外接口函数
void apu_init(apu_context *apu)
{
    filter_highPassFilter(&(apu->filterChain[0]), 44100.0, 90);
    filter_highPassFilter(&(apu->filterChain[1]), 44100.0, 440);
    filter_lowPassFilter(&(apu->filterChain[2]), 44100.0, 14000);
    init();
    apu->noise.shiftRegister = 1;
    apu->pulse1.channel = 1;
    apu->pulse2.channel = 2;
    apu->dmc.cpu = apu->cpu;
}

void apu_step(apu_context *apu)
{
    apu_step_i(apu);
}

uint8_t apu_read(apu_context *apu, uint16_t addr)
{
    return apu_read_r(apu, addr);
}

void apu_write(apu_context *apu, uint16_t addr, uint8_t value)
{
    apu_write_r(apu, addr, value);
}




