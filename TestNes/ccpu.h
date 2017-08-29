//
//  ccpu.h
//  TestNes
//
//  Created by arvin on 2017/8/16.
//  Copyright © 2017年 com.fuwo. All rights reserved.
//

#ifndef ccpu_h
#define ccpu_h

#include <stdint.h>

typedef struct {
    uint16_t address;
    uint16_t pc;
    uint8_t mode;
} stepInfo;

typedef struct
{
    uint16_t    PC;    /* Program Counter */
    uint8_t     SP;    /* Stack Pointer */
    uint8_t     A;    /* Accumulator */
    uint8_t     X;    /* Index Register X */
    uint8_t     Y;    /* Index Register Y */
    uint8_t     P;    /* Processor Status */
#define        P_C    (1 << 0)    /* Carry Flag */
#define        P_Z    (1 << 1)    /* Zero Flag */
#define        P_I    (1 << 2)    /* Interrupt Disable */
#define        P_D    (1 << 3)    /* Decimal Mode */
#define        P_B    (1 << 4)    /* Break Command */
#define        P_U    (1 << 5)    /* Unused */
#define        P_V    (1 << 6)    /* Overflow Flag */
#define        P_N    (1 << 7)    /* Negative Flag */
    
    uint8_t (*read8)(uint16_t);
    void    (*write8)(uint16_t, uint8_t);
    uint64_t cycles, stall;
    uint8_t ipttype;
    
} cpu_context;

void    cpu_init(cpu_context *);
uint8_t cpu_step(cpu_context *);
void    cpu_nmi(cpu_context *);
void    cpu_irq(cpu_context *);

#endif /* ccpu_h */
