//
//  ccpu.c
//  TestNes
//
//  Created by arvin on 2017/8/16.
//  Copyright © 2017年 com.fuwo. All rights reserved.
//

#include "ccpu.h"

#define   CPU_READ8(c, addr)           ((c)->read8((addr)))
#define   CPU_WRITE8(c, addr, v)       ((c)->write8((addr), (v)))
#define   CPU_READ16(c, addr)          (CPU_READ8((c), (addr)) | ((uint16_t)CPU_READ8((c), (addr)+1))<<8)
#define   CPU_READ16BUG(c, addr)       (CPU_READ8((c), (addr)) | (uint16_t)(CPU_READ8((c), ((addr)&0xFF00)|((uint16_t)(((uint8_t)(addr))+1)))<<8))

#define CROSSPAGE(a, b) ((a&0xFF00)!=(b&0xFF00))

// interrupt types
enum {
    iptNone = 0,
    iptNMI,
    iptIRQ,
};

// addressing modes
enum {
    modeAbsolute = 1,
    modeAbsoluteX,
    modeAbsoluteY,
    modeAccumulator,
    modeImmediate,
    modeImplied,
    modeIndexedIndirect,
    modeIndirect,
    modeIndirectIndexed,
    modeRelative,
    modeZeroPage,
    modeZeroPageX,
    modeZeroPageY,
};

// instructionModes indicates the addressing mode for each instruction
uint8_t instModes[256] = {
    6, 7, 6, 7, 11, 11, 11, 11, 6, 5, 4, 5, 1, 1, 1, 1,
    10, 9, 6, 9, 12, 12, 12, 12, 6, 3, 6, 3, 2, 2, 2, 2,
    1, 7, 6, 7, 11, 11, 11, 11, 6, 5, 4, 5, 1, 1, 1, 1,
    10, 9, 6, 9, 12, 12, 12, 12, 6, 3, 6, 3, 2, 2, 2, 2,
    6, 7, 6, 7, 11, 11, 11, 11, 6, 5, 4, 5, 1, 1, 1, 1,
    10, 9, 6, 9, 12, 12, 12, 12, 6, 3, 6, 3, 2, 2, 2, 2,
    6, 7, 6, 7, 11, 11, 11, 11, 6, 5, 4, 5, 8, 1, 1, 1,
    10, 9, 6, 9, 12, 12, 12, 12, 6, 3, 6, 3, 2, 2, 2, 2,
    5, 7, 5, 7, 11, 11, 11, 11, 6, 5, 6, 5, 1, 1, 1, 1,
    10, 9, 6, 9, 12, 12, 13, 13, 6, 3, 6, 3, 2, 2, 3, 3,
    5, 7, 5, 7, 11, 11, 11, 11, 6, 5, 6, 5, 1, 1, 1, 1,
    10, 9, 6, 9, 12, 12, 13, 13, 6, 3, 6, 3, 2, 2, 3, 3,
    5, 7, 5, 7, 11, 11, 11, 11, 6, 5, 6, 5, 1, 1, 1, 1,
    10, 9, 6, 9, 12, 12, 12, 12, 6, 3, 6, 3, 2, 2, 2, 2,
    5, 7, 5, 7, 11, 11, 11, 11, 6, 5, 6, 5, 1, 1, 1, 1,
    10, 9, 6, 9, 12, 12, 12, 12, 6, 3, 6, 3, 2, 2, 2, 2,
};

// instructionSizes indicates the size of each instruction in bytes
uint8_t instSizes[256] = {
    1, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
    2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 3, 3, 3, 0,
    3, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
    2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 3, 3, 3, 0,
    1, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
    2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 3, 3, 3, 0,
    1, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
    2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 3, 3, 3, 0,
    2, 2, 0, 0, 2, 2, 2, 0, 1, 0, 1, 0, 3, 3, 3, 0,
    2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 0, 3, 0, 0,
    2, 2, 2, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
    2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 3, 3, 3, 0,
    2, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
    2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 3, 3, 3, 0,
    2, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
    2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 3, 3, 3, 0,
};

// instructionCycles indicates the number of cycles used by each instruction, not including conditional cycles
uint8_t instCycles[256] = {
    0, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 4, 4, 6, 6,
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 4, 4, 6, 6,
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    6, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 3, 4, 6, 6,
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 5, 4, 6, 6,
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
    2, 6, 2, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5,
    2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
    2, 5, 2, 5, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4,
    2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
};

// instructionPageCycles indicates the number of cycles used by each instruction when a page is crossed
uint8_t instPageCycles[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0,
};

// addBranchCycles adds a cycle for taking a branch and adds another cycle if the branch jumps to a new page
void addBranchCycles(cpu_context *c, stepInfo* info)
{
    CROSSPAGE(info->pc, info->address)?(c->cycles+=2):(c->cycles+=1);
}

// branch set the PC if the argument is true
void branch(cpu_context *c, stepInfo* info, char value)
{
    if (value) { c->PC = info->address; addBranchCycles(c, info); }
}

// setC sets the carry flag if the argument is true
void setC(cpu_context *c, char value)
{
    value?(c->P|=P_C):(c->P&=~P_C);
}

// setV sets the overflow flag if the argument is true
void setV(cpu_context *c, char value)
{
    value?(c->P|=P_V):(c->P&=~P_V);
}

// setZ sets the zero flag if the argument is zero
void setZ(cpu_context *c, uint8_t value)
{
    value?(c->P&=~P_Z):(c->P|=P_Z);
}

// setN sets the negative flag if the argument is negative (high bit is set)
void setN(cpu_context *c, uint8_t value)
{
    (value&0x80)?(c->P|=P_N):(c->P&=~P_N);
}

// setZN sets the zero flag and the negative flag
void setZN(cpu_context *c, uint8_t value)
{
    setZ(c, value); setN(c, value);
}

void compare(cpu_context *c, uint8_t a, uint8_t b)
{
    setZN(c, a - b); setC(c, a >= b);
}

// push pushes a byte onto the stack
void push(cpu_context *c, uint8_t value)
{
    CPU_WRITE8(c, 0x100|(uint16_t)(c->SP), value); c->SP--;
}

// pull pops a byte from the stack
uint8_t pull(cpu_context *c)
{
    c->SP++; return CPU_READ8(c, 0x100|(uint16_t)(c->SP));
}

// push16 pushes two bytes onto the stack
void push16(cpu_context *c, uint16_t value)
{
    //小端，高位字节放高地址，NES栈是Top-Down模式
    push(c, (uint8_t)(value >> 8)); push(c, (uint8_t)(value & 0xFF));
}

// pull16 pops two bytes from the stack
uint16_t pull16(cpu_context *c)
{
    //小端，高位字节放高地址，NES栈是Top-Down模式，C语言按位或“|”是从前面向后面依次计算的。
    return pull(c) | (uint16_t)pull(c) << 8;
}

//以下是指令的实现
// ADC - Add with Carry
void adc(cpu_context *c, stepInfo* info)
{
    uint8_t a = c->A, b = CPU_READ8(c, info->address), cy = (c->P & P_C) >> 0;
    setZN(c, c->A = a + b + cy);
    setC(c, (int)(a)+(int)(b)+(int)(cy) > 0xFF);
    setV(c, (((a^b)&0x80) == 0) && (((a^c->A)&0x80) != 0));
}

// AND - Logical AND
void and(cpu_context *c, stepInfo* info)
{
    setZN(c, c->A &= CPU_READ8(c, info->address));
}

// ASL - Arithmetic Shift Left
void asl(cpu_context *c, stepInfo* info)
{
    if (info->mode == modeAccumulator) {
        setC(c, ((c->A >> 7) & 1));//设置进位信息
        setZN(c, c->A <<= 1);//算术左移等价于逻辑左移,右边全为0，算术右移左边全为1，逻辑右移左边全为0
    } else {
        uint8_t value = CPU_READ8(c, info->address);
        setC(c, ((value >> 7) & 1));
        setZN(c, value <<= 1);
        CPU_WRITE8(c, info->address, value);
    }
}

// BCC - Branch if Carry Clear
void bcc(cpu_context *c, stepInfo* info)
{
    branch(c, info, !(c->P&P_C));
}

// BCS - Branch if Carry Set
void bcs(cpu_context *c, stepInfo* info)
{
    branch(c, info, c->P&P_C);
}

// BEQ - Branch if Equal
void beq(cpu_context *c, stepInfo* info)
{
    branch(c, info, c->P&P_Z);
}

// BIT - Bit Test
void bit(cpu_context *c, stepInfo* info)
{
    uint8_t value = CPU_READ8(c, info->address);
    setV(c, ((value >> 6) & 1));
    setZ(c, value & c->A);
    setN(c, value);
}

// BMI - Branch if Minus
void bmi(cpu_context *c, stepInfo* info)
{
    branch(c, info, c->P&P_N);
}

// BNE - Branch if Not Equal
void bne(cpu_context *c, stepInfo* info)
{
    branch(c, info, !(c->P&P_Z));
}

// BPL - Branch if Positive
void bpl(cpu_context *c, stepInfo* info)
{
    branch(c, info, !(c->P&P_N));
}

// PHP - Push Processor Status
void php(cpu_context *c, stepInfo* info)
{
    push(c, c->P | P_U);
}

// SEI - Set Interrupt Disable
void sei(cpu_context *c, stepInfo* info)
{
    c->P |= P_I;
}

// BRK - Force Interrupt
void iptr(cpu_context*, uint16_t);
void bke(cpu_context *c, stepInfo* info)
{
    iptr(c, 0xFFFE);
}

// BVC - Branch if Overflow Clear
void bvc(cpu_context *c, stepInfo* info)
{
    branch(c, info, !(c->P&P_V));
}

// BVS - Branch if Overflow Set
void bvs(cpu_context *c, stepInfo* info)
{
    branch(c, info, c->P&P_V);
}

// CLC - Clear Carry Flag
void clc(cpu_context *c, stepInfo* info)
{
    c->P &= ~P_C;
}

// CLD - Clear Decimal Mode
void cld(cpu_context *c, stepInfo* info)
{
    c->P &= ~P_D;
}

// CLI - Clear Interrupt Disable
void cli(cpu_context *c, stepInfo* info)
{
    c->P &= ~P_I;
}

// CLV - Clear Overflow Flag
void clv(cpu_context *c, stepInfo* info)
{
    c->P &= ~P_V;
}

// CMP - Compare
void cmp(cpu_context *c, stepInfo* info)
{
    compare(c, c->A, CPU_READ8(c, info->address));
}

// CPX - Compare X Register
void cpx(cpu_context *c, stepInfo* info)
{
    compare(c, c->X, CPU_READ8(c, info->address));
}

// CPY - Compare Y Register
void cpy(cpu_context *c, stepInfo* info)
{
    compare(c, c->Y, CPU_READ8(c, info->address));
}

// DEC - Decrement Memory
void dec(cpu_context *c, stepInfo* info)
{
    uint8_t value = CPU_READ8(c, info->address) - 1;
    CPU_WRITE8(c, info->address, value); setZN(c, value);
}

// DEX - Decrement X Register
void dex(cpu_context *c, stepInfo* info)
{
    setZN(c, --c->X);
}

// DEY - Decrement Y Register
void dey(cpu_context *c, stepInfo* info)
{
    setZN(c, --c->Y);
}

// EOR - Exclusive OR
void eor(cpu_context *c, stepInfo* info)
{
    setZN(c, c->A ^= CPU_READ8(c, info->address));
}

// INC - Increment Memory
void inc(cpu_context *c, stepInfo* info)
{
    uint8_t value = CPU_READ8(c, info->address) + 1;
    CPU_WRITE8(c, info->address, value); setZN(c, value);
}

// INX - Increment X Register
void inx(cpu_context *c, stepInfo* info)
{
    setZN(c, ++c->X);
}

// INY - Increment Y Register
void iny(cpu_context *c, stepInfo* info)
{
    setZN(c, ++c->Y);
}

// JMP - Jump
void jmp(cpu_context *c, stepInfo* info)
{
    c->PC = info->address;
}

// JSR - Jump to Subroutine
void jsr(cpu_context *c, stepInfo* info)
{
    push16(c, c->PC - 1); jmp(c, info);
}

// LDA - Load Accumulator
void lda(cpu_context *c, stepInfo* info)
{
    setZN(c, c->A = CPU_READ8(c, info->address));
}

// LDX - Load X Register
void ldx(cpu_context *c, stepInfo* info)
{
    setZN(c, c->X = CPU_READ8(c, info->address));
}

// LDY - Load Y Register
void ldy(cpu_context *c, stepInfo* info)
{
    setZN(c, c->Y = CPU_READ8(c, info->address));
}

// LSR - Logical Shift Right
void lsr(cpu_context *c, stepInfo* info)
{
    if (info->mode == modeAccumulator) {
        setC(c, (c->A&0x1));
        setZN(c, c->A >>= 1);
    } else {
        uint8_t value = CPU_READ8(c, info->address);
        setC(c, (value&0x1));
        setZN(c, value >>= 1);
        CPU_WRITE8(c, info->address, value);
    }
}

// NOP - No Operation
void nop(cpu_context *c, stepInfo* info)
{
    //nop
}

// ORA - Logical Inclusive OR
void ora(cpu_context *c, stepInfo* info)
{
    setZN(c, c->A |= CPU_READ8(c, info->address));
}

// PHA - Push Accumulator
void pha(cpu_context *c, stepInfo* info)
{
    push(c, c->A);
}

// PLA - Pull Accumulator
void pla(cpu_context *c, stepInfo* info)
{
    setZN(c, c->A = pull(c));
}

// PLP - Pull Processor Status
void plp(cpu_context *c, stepInfo* info)
{
    c->P = (pull(c)&0xEF) | 0x20;
}

// ROL - Rotate Left
void rol(cpu_context *c, stepInfo* info)
{
    if (info->mode == modeAccumulator) {
        uint8_t cy = (c->P & P_C)>>0;
        setC(c, ((c->A >> 7) & 1));
        c->A = (c->A << 1) | cy; setZN(c, c->A);
    } else {
        uint8_t cy = (c->P & P_C)>>0;
        uint8_t value = CPU_READ8(c, info->address);
        setC(c, ((value >> 7) & 1));
        value = (value << 1) | cy;
        CPU_WRITE8(c, info->address, value); setZN(c, value);
    }
}

// ROR - Rotate Right
void ror(cpu_context *c, stepInfo* info)
{
    if (info->mode == modeAccumulator) {
        uint8_t cy = (c->P & P_C)>>0;
        setC(c, (c->A & 1));
        c->A = (c->A >> 1) | (cy << 7); setZN(c, c->A);
    } else {
        uint8_t cy = (c->P & P_C)>>0;
        uint8_t value = CPU_READ8(c, info->address);
        setC(c, (value & 1));
        value = (value >> 1) | (cy << 7);
        CPU_WRITE8(c, info->address, value); setZN(c, value);
    }
}

// RTI - Return from Interrupt
void rti(cpu_context *c, stepInfo* info)
{
    c->P = (pull(c)&0xEF) | 0x20; c->PC = pull16(c);
}

// RTS - Return from Subroutine
void rts(cpu_context *c, stepInfo* info)
{
    c->PC = pull16(c) + 1;
}

// SBC - Subtract with Carry
void sbc(cpu_context *c, stepInfo* info)
{
    uint8_t a = c->A, b = CPU_READ8(c, info->address), cy = (c->P&P_C)>>0;
    setZN(c, c->A = a - b - (1 - cy));
    setC(c, (int)(a)-(int)(b)-(int)(1-cy) >= 0);
    setV(c, ((a^b)&0x80) != 0 && ((a^c->A)&0x80) != 0);
}

// SEC - Set Carry Flag
void sec(cpu_context *c, stepInfo* info)
{
    c->P |= P_C;
}

// SED - Set Decimal Flag
void sed(cpu_context *c, stepInfo* info)
{
    c->P |= P_D;
}

// STA - Store Accumulator
void sta(cpu_context *c, stepInfo* info)
{
    CPU_WRITE8(c, info->address, c->A);
}

// STX - Store X Register
void stx(cpu_context *c, stepInfo* info)
{
    CPU_WRITE8(c, info->address, c->X);
}

// STY - Store Y Register
void sty(cpu_context *c, stepInfo* info)
{
    CPU_WRITE8(c, info->address, c->Y);
}

// TAX - Transfer Accumulator to X
void tax(cpu_context *c, stepInfo* info)
{
    setZN(c, c->X = c->A);
}

// TAY - Transfer Accumulator to Y
void tay(cpu_context *c, stepInfo* info)
{
    setZN(c, c->Y = c->A);
}

// TSX - Transfer Stack Pointer to X
void tsx(cpu_context *c, stepInfo* info)
{
    setZN(c, c->X = c->SP);
}

// TXA - Transfer X to Accumulator
void txa(cpu_context *c, stepInfo* info)
{
    setZN(c, c->A = c->X);
}

// TXS - Transfer X to Stack Pointer
void txs(cpu_context *c, stepInfo* info)
{
    c->SP = c->X;
}

// TYA - Transfer Y to Accumulator
void tya(cpu_context *c, stepInfo* info)
{
    setZN(c, c->A = c->Y);
}

// illegal opcodes below
void ahx(cpu_context *c, stepInfo* info) {}
void alr(cpu_context *c, stepInfo* info) {}
void anc(cpu_context *c, stepInfo* info) {}
void arr(cpu_context *c, stepInfo* info) {}
void axs(cpu_context *c, stepInfo* info) {}
void dcp(cpu_context *c, stepInfo* info) {}
void isc(cpu_context *c, stepInfo* info) {}
void kil(cpu_context *c, stepInfo* info) {}
void las(cpu_context *c, stepInfo* info) {}
void lax(cpu_context *c, stepInfo* info) {}
void rla(cpu_context *c, stepInfo* info) {}
void rra(cpu_context *c, stepInfo* info) {}
void sax(cpu_context *c, stepInfo* info) {}
void shx(cpu_context *c, stepInfo* info) {}
void shy(cpu_context *c, stepInfo* info) {}
void slo(cpu_context *c, stepInfo* info) {}
void sre(cpu_context *c, stepInfo* info) {}
void tas(cpu_context *c, stepInfo* info) {}
void xaa(cpu_context *c, stepInfo* info) {}

void (*table[256])(cpu_context*, stepInfo*) = {
    bke,  ora,  kil,  slo,  nop,  ora,  asl,  slo,
    php,  ora,  asl,  anc,  nop,  ora,  asl,  slo,
    bpl,  ora,  kil,  slo,  nop,  ora,  asl,  slo,
    clc,  ora,  nop,  slo,  nop,  ora,  asl,  slo,
    jsr,  and,  kil,  rla,  bit,  and,  rol,  rla,
    plp,  and,  rol,  anc,  bit,  and,  rol,  rla,
    bmi,  and,  kil,  rla,  nop,  and,  rol,  rla,
    sec,  and,  nop,  rla,  nop,  and,  rol,  rla,
    rti,  eor,  kil,  sre,  nop,  eor,  lsr,  sre,
    pha,  eor,  lsr,  alr,  jmp,  eor,  lsr,  sre,
    bvc,  eor,  kil,  sre,  nop,  eor,  lsr,  sre,
    cli,  eor,  nop,  sre,  nop,  eor,  lsr,  sre,
    rts,  adc,  kil,  rra,  nop,  adc,  ror,  rra,
    pla,  adc,  ror,  arr,  jmp,  adc,  ror,  rra,
    bvs,  adc,  kil,  rra,  nop,  adc,  ror,  rra,
    sei,  adc,  nop,  rra,  nop,  adc,  ror,  rra,
    nop,  sta,  nop,  sax,  sty,  sta,  stx,  sax,
    dey,  nop,  txa,  xaa,  sty,  sta,  stx,  sax,
    bcc,  sta,  kil,  ahx,  sty,  sta,  stx,  sax,
    tya,  sta,  txs,  tas,  shy,  sta,  shx,  ahx,
    ldy,  lda,  ldx,  lax,  ldy,  lda,  ldx,  lax,
    tay,  lda,  tax,  lax,  ldy,  lda,  ldx,  lax,
    bcs,  lda,  kil,  lax,  ldy,  lda,  ldx,  lax,
    clv,  lda,  tsx,  las,  ldy,  lda,  ldx,  lax,
    cpy,  cmp,  nop,  dcp,  cpy,  cmp,  dec,  dcp,
    iny,  cmp,  dex,  axs,  cpy,  cmp,  dec,  dcp,
    bne,  cmp,  kil,  dcp,  nop,  cmp,  dec,  dcp,
    cld,  cmp,  nop,  dcp,  nop,  cmp,  dec,  dcp,
    cpx,  sbc,  nop,  isc,  cpx,  sbc,  inc,  isc,
    inx,  sbc,  nop,  sbc,  cpx,  sbc,  inc,  isc,
    beq,  sbc,  kil,  isc,  nop,  sbc,  inc,  isc,
    sed,  sbc,  nop,  isc,  nop,  sbc,  inc,  isc,
};
stepInfo info;

void iptt(cpu_context *c, uint8_t type)
{
    if (type == iptNMI) { c->ipttype = type; }
    if (type == iptIRQ && !(c->P&P_I)) { c->ipttype = iptIRQ; }
}

void iptr(cpu_context* c, uint16_t ivector)
{
    push16(c, c->PC); php(c, NULL); sei(c, NULL);
    c->PC = CPU_READ16(c, ivector);
    c->cycles += 7;
}

void reset(cpu_context* c)
{
    c->PC = CPU_READ16(c, 0xFFFC); c->SP = 0xFD; c->P = 0x24;
}

uint8_t cpu_step_i(cpu_context *c)
{
    if (c->stall > 0) { c->stall--; return 1; }
    uint64_t cycles = c->cycles;
    c->ipttype==iptNMI?(iptr(c, 0xFFFA)):(c->ipttype==iptIRQ?(iptr(c, 0xFFFE)):(0));
    c->ipttype = iptNone;
    uint8_t opcode = CPU_READ8(c, c->PC), mode = instModes[opcode];
    uint16_t address = 0, pageCrossed = 0;
    switch (mode) {
        case modeAbsolute:
            address = CPU_READ16(c, c->PC + 1); break;
        case modeAbsoluteX:{
            address = CPU_READ16(c, c->PC + 1) + c->X;
            pageCrossed = CROSSPAGE(address - c->X, address);
            break;
        }
        case modeAbsoluteY:{
            address = CPU_READ16(c, c->PC + 1) + c->Y;
            pageCrossed = CROSSPAGE(address - c->Y, address);
            break;
        }
        case modeAccumulator:
            address = 0; break;
        case modeImmediate:
            address = c->PC + 1; break;
        case modeImplied:
            address = 0; break;
        case modeIndexedIndirect:
            address = CPU_READ16BUG(c, CPU_READ8(c, c->PC + 1) + c->X); break;
        case modeIndirect:
            address = CPU_READ16BUG(c, CPU_READ16(c, c->PC + 1)); break;
        case modeIndirectIndexed:{
            address = CPU_READ16BUG(c, CPU_READ8(c, c->PC + 1)) + c->Y;
            pageCrossed = CROSSPAGE(address - c->Y, address);
            break;
        }
        case modeRelative:{
            address=c->PC+2;
            uint16_t offset = CPU_READ8(c, c->PC + 1);
            (offset<0x80)?(address+=offset):(address+=offset-0x100);
            break;
        }
        case modeZeroPage:
            address = CPU_READ8(c, c->PC + 1); break;
        case modeZeroPageX:
            address = CPU_READ8(c, c->PC + 1) + c->X; break;
        case modeZeroPageY:
            address = CPU_READ8(c, c->PC + 1) + c->Y; break;
    }
    c->PC += instSizes[opcode];
    c->cycles += pageCrossed?instCycles[opcode]+instPageCycles[opcode]:instCycles[opcode];
    info.address=address;info.pc=c->PC;info.mode=mode; table[opcode](c, &info);
    return (c->cycles - cycles);
}

//下面是对外接口
void cpu_init(cpu_context *c)
{
    reset(c);
}

uint8_t cpu_step(cpu_context *c)
{
    return cpu_step_i(c);
}

void cpu_nmi(cpu_context *c)
{
    iptt(c, iptNMI);
}

void cpu_irq(cpu_context *c)
{
    iptt(c, iptIRQ);
}

