#include "memory.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "hardware/regs/addressmap.h"
#include "pico/stdlib.h"
#include "command.h"
#include "device.h"
#include "led.h"

#define VECTOR_TABLE 0x10000100
#define GPIO_IN_REGISTER 0xd0000004

int main(void);

uint32_t data_variable = 100;
uint32_t bss_variable;

extern char __flash_binary_start;
extern char __flash_binary_end;
extern char __boot2_start__;
extern char __boot2_end__;
extern char __etext;
extern char __data_start__;
extern char __data_end__;
extern char __bss_start__;
extern char __bss_end__;
extern char __HeapLimit;
extern char __StackBottom;
extern char __StackTop;

static void row(const char *name, uintptr_t start, uintptr_t end)
{
    printf("%-10s 0x%08x 0x%08x %8u\n",
           name, (unsigned)start, (unsigned)end, (unsigned)(end - start));
}

void mem_info(void)
{
    uintptr_t flash_start = XIP_BASE;
    uintptr_t flash_end = flash_start + PICO_FLASH_SIZE_BYTES;
    uintptr_t sram_start = SRAM_BASE;
    uintptr_t sram_end = sram_start + 264 * 1024;
    uintptr_t rom_start = ROM_BASE;
    uintptr_t rom_end = rom_start + 16 * 1024;

    uintptr_t image_start = (uintptr_t)&__flash_binary_start;
    uintptr_t image_end = (uintptr_t)&__flash_binary_end;
    uintptr_t boot2_start = (uintptr_t)&__boot2_start__;
    uintptr_t boot2_end = (uintptr_t)&__boot2_end__;
    uintptr_t text_end = (uintptr_t)&__etext;
    uintptr_t data_start = (uintptr_t)&__data_start__;
    uintptr_t data_end = (uintptr_t)&__data_end__;
    uintptr_t bss_start = (uintptr_t)&__bss_start__;
    uintptr_t bss_end = (uintptr_t)&__bss_end__;
    uintptr_t heap_end = (uintptr_t)&__HeapLimit;
    uintptr_t stack_start = (uintptr_t)&__StackBottom;
    uintptr_t stack_end = (uintptr_t)&__StackTop;

    uintptr_t data_size = data_end - data_start;
    uintptr_t data_flash_end = text_end + data_size;
    uintptr_t boot2_size = boot2_end - boot2_start;
    uintptr_t text_size = text_end - boot2_end;
    uintptr_t image_size = image_end - image_start;
    uintptr_t flash_free = flash_end - image_end;
    uintptr_t bss_size = bss_end - bss_start;
    uintptr_t heap_size = heap_end - bss_end;
    uintptr_t stack_size = stack_end - stack_start;

    printf("area       start      end        size\n");
    row("flash", flash_start, flash_end);
    row("sram", sram_start, sram_end);
    row("rom", rom_start, rom_end);
    row("image", image_start, image_end);
    row("free", image_end, flash_end);
    row("boot2", boot2_start, boot2_end);
    row("text", boot2_end, text_end);
    row("data flash", text_end, data_flash_end);
    row("data ram", data_start, data_end);
    row("bss", bss_start, bss_end);
    row("heap", bss_end, heap_end);
    row("stack", stack_start, stack_end);

    printf("\ntotal\n");
    printf("  flash image %8u = boot2 %u + text %u + data %u\n",
           (unsigned)image_size, (unsigned)boot2_size,
           (unsigned)text_size, (unsigned)data_size);
    printf("  flash free  %8u of %u\n",
           (unsigned)flash_free, (unsigned)PICO_FLASH_SIZE_BYTES);
    printf("  ram used    %8u = data %u + bss %u\n",
           (unsigned)(data_size + bss_size),
           (unsigned)data_size, (unsigned)bss_size);
    printf("  ram free    %8u for heap and %u for stack\n",
           (unsigned)heap_size, (unsigned)stack_size);
}

void fw_info(void)
{
    data_variable++;
    bss_variable++;

    uint16_t *main_code = (uint16_t *)((uintptr_t)main & ~1u);
    uint16_t *fw_info_code = (uint16_t *)((uintptr_t)fw_info & ~1u);
    uint32_t stack_variable = 1946;
    uint32_t *heap_variable = malloc(sizeof(uint32_t));

    if (heap_variable != NULL)
    {
        *heap_variable = 1951;
    }

    printf("object          address     value\n");
    printf("%-15s 0x%08x  0x%04x\n", "main",
           (unsigned)(uintptr_t)main, (unsigned)*main_code);
    printf("%-15s 0x%08x  0x%04x\n", "fw_info",
           (unsigned)(uintptr_t)fw_info, (unsigned)*fw_info_code);
    printf("%-15s 0x%08x\n", "commands", (unsigned)(uintptr_t)commands);

    for (uint i = 0; i < command_count; i++)
    {
        printf("- %-13s 0x%08x\n", commands[i].name,
               (unsigned)(uintptr_t)commands[i].handler);
    }

    printf("%-15s 0x%08x  %s\n", "DEVICE_PROJECT",
           (unsigned)(uintptr_t)DEVICE_PROJECT, DEVICE_PROJECT);
    printf("%-15s 0x%08x  %s\n", "DEVICE_BOARD",
           (unsigned)(uintptr_t)DEVICE_BOARD, DEVICE_BOARD);
    printf("%-15s 0x%08x  %u\n", "data_variable",
           (unsigned)(uintptr_t)&data_variable, (unsigned)data_variable);
    printf("%-15s 0x%08x  %u\n", "bss_variable",
           (unsigned)(uintptr_t)&bss_variable, (unsigned)bss_variable);
    printf("%-15s 0x%08x  %u\n", "stack_variable",
           (unsigned)(uintptr_t)&stack_variable, (unsigned)stack_variable);

    if (heap_variable != NULL)
    {
        printf("%-15s 0x%08x  %u\n", "heap_variable",
               (unsigned)(uintptr_t)heap_variable, (unsigned)*heap_variable);
        free(heap_variable);
    }
}

void boot_info(void)
{
    const uint32_t *vectors = (const uint32_t *)VECTOR_TABLE;
    volatile uint32_t *gpio_in = (volatile uint32_t *)GPIO_IN_REGISTER;
    uint32_t stack_top = vectors[0];
    uint32_t reset_handler = vectors[1];
    uint32_t level = (*gpio_in >> led_pin()) & 1u;

    printf("vector table   0x%08x\n", VECTOR_TABLE);
    printf("  stack top    0x%08x\n", (unsigned)stack_top);
    printf("  reset        0x%08x\n", (unsigned)reset_handler);
    printf("  reset (even) 0x%08x\n", (unsigned)(reset_handler & ~1u));
    printf("gpio in        0x%08x\n", GPIO_IN_REGISTER);
    printf("  led bit      %u\n", (unsigned)level);
    printf("  gpio_get     %u\n", (unsigned)gpio_get(led_pin()));
}
