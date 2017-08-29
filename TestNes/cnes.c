//
//  cnes.c
//  TestNes
//
//  Created by arvin on 2017/8/16.
//  Copyright © 2017年 com.fuwo. All rights reserved.
//

#include "cnes.h"

//NES对象，全局唯一
struct cnes_context cnes;

//下面是cpu和ppu的按字节读写函数
//cpu按字节读取
static uint8_t nes_cpuread8(uint16_t addr)
{
    /* 2k 内部 RAM */
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        return cnes.ram[addr & 0x07FF];//&作为镜像地址映射使用
    }
    
    /* PPU 寄存器 */
    if (addr >= 0x2000 && addr <= 0x3FFF) {
        return ppu_read(&cnes.ppu, addr);//读取ppu寄存器字节数据
    }
    
    /* 手柄 */
    if (addr == 0x4016 || addr == 0x4017) {
        int index = addr - 0x4016;
        uint8_t val = (cnes.ctrl[index] >> cnes.ctrl[2+index]) & 1; //得到相应位置的布尔值，循环扫描state字节对应的位
        if (cnes.ctrl[2+index] == 0){
            cnes.ctrl[2+index] = 7;//设置移位
        }else{
            cnes.ctrl[2+index]--;//每次移动一位
        }
        return val;
    }
    
    /* APU 寄存器处理，我们没有实现apu，故直接返回 */
    if (addr >= 0x4000 && addr <= 0x4017) {
        return apu_read(&cnes.apu, addr);
    }
    
    /* ROM 读取 */
    if (addr >= 0x4020 && addr <= 0xFFFF) {
        return cnes.mmc.cpuread(&cnes, addr);
    }
    
    printf("[%s] addr %04X not mapped\n", __func__, addr);
    abort();
}

//cpu按字节写入
static void nes_cpuwrite8(uint16_t addr, uint8_t val)
{
    /* 2KB 内部 RAM */
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        cnes.ram[addr & 0x07FF] = val;
        return;
    }
    
    /* PPU 寄存器 */
    if (addr >= 0x2000 && addr <= 0x3FFF) {
        ppu_write(&cnes.ppu, addr, val);
        return;
    }
    if (addr == 0x4014) {//dma批量移动数据
        for (int i = 0; i < 0x100; i++) {//256字节，SPR-RAM
            cnes.ppu.oam[cnes.ppu.oamaddr] = nes_cpuread8(((uint16_t)val << 8) + i);
            cnes.ppu.oamaddr = (cnes.ppu.oamaddr + 1) & (0x100 - 1);
        }
        cnes.cpu.stall += 513;
        if (cnes.cpu.cycles%2 == 1) {
            cnes.cpu.stall++;
        }
        return;
    }
    
    /* APU 寄存器处理，我们没有实现apu，故直接返回*/
    if ((addr >= 0x4000 && addr <= 0x4013) || (addr == 0x4015) || (addr == 0x4017)) {
        apu_write(&cnes.apu, addr, val);
        return;
    }
    
    /* 手柄 */
    if (addr == 0x4016) {
        if ((val & 1) == 0) {
            cnes.ctrl[2] = cnes.ctrl[3] = 7;
        }
        return;
    }
    
    /* ROM */
    if (addr >= 0x4020 && addr <= 0xFFFF) {
        cnes.mmc.cpuwrite(&cnes, addr, val);
        return;
    }
    
    printf("  [%s] addr %04X not mapped\n", __func__, addr);
    abort();
}

//ppu按字节读取
static uint8_t nes_ppuread8(uint16_t addr)
{
    if (addr >= 0x3F00 && addr <= 0x3FFF) {//PPU地址空间，调色板地址范围
        switch (addr) {
            case 0x3F10:
            case 0x3F14:
            case 0x3F18:
            case 0x3F1C:
                addr -= 0x10;
                break;
        }
        return cnes.palette[addr & (0x20 - 1)];//调色板
    }
    
    return cnes.mmc.ppuread(&cnes, addr);
}

//ppu按字节写入
static void nes_ppuwrite8(uint16_t addr, uint8_t val)
{
    if (addr >= 0x3F00 && addr <= 0x3FFF) {
        switch (addr) {
            case 0x3F10:
            case 0x3F14:
            case 0x3F18:
            case 0x3F1C:
                addr -= 0x10;
                break;
        }
        cnes.palette[addr & (0x20 - 1)] = val;//调色板，放置系统调色板的索引
        return;
    }
    
    cnes.mmc.ppuwrite(&cnes, addr, val);
}

//下面是加载ROM并初始化cnes对象和SDL2模块，最后运行模拟器的代码
//加载rom
static int load_rom(const char *path)
{
    //ROM文件（nes文件）结构地址和大小
#define    ROM_HEADER_LENGTH        16  //nes文件头的16字节
#define    ROM_TRAINER_LENGTH       512 //考虑TRAINER段长度512字节，好像都没有
#define    ROM_HAS_TRAINER(data)    ((data[6] & 0x04) != 0)
#define    ROM_PRG_START(data)      (ROM_HEADER_LENGTH + (ROM_HAS_TRAINER(data) ? ROM_TRAINER_LENGTH : 0))
#define    ROM_PRG_LENGTH(data)     (data[4] * 0x4000) //每块代码段16k
#define    ROM_CHR_START(data)      (ROM_PRG_START(data) + ROM_PRG_LENGTH(data))
#define    ROM_CHR_LENGTH(data)     (data[5] * 0x2000) //每块角色资源8k
    //ROM文件结束
    struct stat st;
    int rom_fd = open(path, O_RDONLY);
    if (rom_fd == -1){
        printf("Failed to open %s\n", path);
        return  -1;
    }
    if (fstat(rom_fd, &st) == -1){
        printf("Failed to fstat %s\n", path);
        return -1;
    }
    //内存映射
    cnes.rom_data = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, rom_fd, 0);
    if (cnes.rom_data == MAP_FAILED){
        printf("Failed to mmap %s\n", path);
        return -1;
    }
    if (memcmp(cnes.rom_data, "NES\x1a", 4) != 0){
        printf("Bad iNES header\n");
        return -1;
    }
    cnes.prg_start = ROM_PRG_START(cnes.rom_data);
    cnes.prg_len = ROM_PRG_LENGTH(cnes.rom_data);
    cnes.chr_start = ROM_CHR_START(cnes.rom_data);
    cnes.chr_len = ROM_CHR_LENGTH(cnes.rom_data);
    
    return mapper_init(&cnes.mmc, cnes.rom_data, cnes.rom_len);
}

int wnd_init(const char *);
void wnd_draw(uint8_t*);
int wnd_poll(uint8_t *);
void wnd_play(float com);

int cnes_init(const char* romname)
{
    char* title = malloc(strlen(romname));
    memset(title, 0, strlen(romname));
    strncpy(title, romname, strlen(romname)-4);
    if (load_rom(romname)){//默认ROM文件跟模拟器可执行文件在同一个目录
        printf("Couldn't load ROM %s\n", romname);
        return -1;
    }
    if (wnd_init(title) != 0){
        return -1;
    }
    
    memset(&cnes.cpu, 0, sizeof(cnes.cpu));
    cnes.cpu.read8 = nes_cpuread8;
    cnes.cpu.write8 = nes_cpuwrite8;
    cpu_init(&cnes.cpu);
    
    memset(&cnes.ppu, 0, sizeof(cnes.ppu));
    cnes.ppu.cpu = &cnes.cpu;
    cnes.ppu.draw = wnd_draw;//窗口绘制函数
    cnes.ppu.read8 = nes_ppuread8;
    cnes.ppu.write8 = nes_ppuwrite8;
    ppu_init(&cnes.ppu);
    
    memset(&cnes.apu, 0, sizeof(cnes.apu));
    cnes.apu.cpu = &cnes.cpu;
    cnes.apu.play = wnd_play;//窗口声音播放函数
    cnes.apu.read8 = nes_cpuread8;
    cnes.apu.write8 = nes_cpuwrite8;
    apu_init(&cnes.apu);
    
    //执行模拟器循环
    for (int ffend = 0; !(ffend&&wnd_poll(cnes.ctrl)==1); ) {
        cpu_step(&cnes.cpu);
        ffend = 0; for (int i = 0; i < 3; i++) ffend+=ppu_step(&cnes.ppu);
        apu_step(&cnes.apu);
    }
    
    return 0;
}

