#ifndef apu_h
#define apu_h

#include "ccpu.h"

#define CPU_RATE    1789773//NTSC 6502频率(cycles per second)
#define FRAME_RATE  (CPU_RATE/240.0)//NES音频帧率(240Hz)(cycles per frame_counter)
#define SAMPLE_RATE (CPU_RATE/44100.0)//SDL2采样率(samples per second)(cycles per sample)

// First order filters are defined by the following parameters.
// y[n] = B0*x[n] + B1*x[n-1] - A1*y[n-1]
typedef struct {
    float B0;
    float B1;
    float A1;
    float prevX;
    float prevY;
    
} FirstOrderFilter;

// Pulse
typedef struct {
    char enabled;
    uint8_t channel;
    char lengthEnabled;
    uint8_t lengthValue;
    uint16_t timerPeriod;
    uint16_t timerValue;
    uint8_t dutyMode;
    uint8_t dutyValue;
    char sweepReload;
    char sweepEnabled;
    char sweepNegate;
    uint8_t sweepShift;
    uint8_t sweepPeriod;
    uint8_t sweepValue;
    char envelopeEnabled;
    char envelopeLoop;
    char envelopeStart;
    uint8_t envelopePeriod;
    uint8_t envelopeValue;
    uint8_t envelopeVolume;
    uint8_t constantVolume;
} Pulse;

// Triangle
typedef struct {
    char enabled;
    char lengthEnabled;
    uint8_t lengthValue;
    uint16_t timerPeriod;
    uint16_t timerValue;
    uint8_t dutyValue;
    uint8_t counterPeriod;
    uint8_t counterValue;
    char counterReload;
} Triangle;

// Noise
typedef struct {
    char enabled;
    char mode;
    uint16_t shiftRegister;
    char lengthEnabled;
    uint8_t lengthValue;
    uint16_t timerPeriod;
    uint16_t timerValue;
    char envelopeEnabled;
    char envelopeLoop;
    char envelopeStart;
    uint8_t envelopePeriod;
    uint8_t envelopeValue;
    uint8_t envelopeVolume;
    uint8_t constantVolume;
} Noise;

// DMC
typedef struct {
    cpu_context* cpu;
    char enabled;
    uint8_t value;
    uint16_t sampleAddress;
    uint16_t sampleLength;
    uint16_t currentAddress;
    uint16_t currentLength;
    uint8_t shiftRegister;
    uint8_t bitCount;
    uint8_t tickPeriod;
    uint8_t tickValue;
    char loop;
    char irq;
} DMC;

// APU
typedef struct
{
    Pulse pulse1;
    Pulse pulse2;
    Triangle triangle;
    Noise noise;
    DMC dmc;
    uint64_t cycle;
    uint8_t framePeriod;
    uint8_t frameValue;
    char frameIRQ;
    FirstOrderFilter filterChain[3];
    
    cpu_context *cpu;
    void (*play)(float);
    uint8_t (*read8)(uint16_t);
    void (*write8)(uint16_t, uint8_t);
    
} apu_context;

void    apu_init(apu_context *);
void    apu_step(apu_context *);
uint8_t apu_read(apu_context *, uint16_t);
void    apu_write(apu_context *, uint16_t, uint8_t);

#endif // apu_h
