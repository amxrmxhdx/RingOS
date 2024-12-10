#include <vga.h>
#include <keyboard.h>
#include <ata.h>
#include "libc/fileio.h"
#include "shell.h"
#include "idt.h"
#include "gdt.h"

void kernel_main(void) {
    // Initialize basic hardware
    vga_init();
    keyboard_init();

    vga_writestr("GDT init starting...\n");
    init_gdt();
    vga_writestr("GDT init done.\n");

    vga_writestr("IDT init starting...\n");
    init_idt();
    vga_writestr("IDT init done.\n");

    
    vga_writestr("Initializing hardware...\n");
    
    // Initialize ATA with retry
    int retries = 3;
    bool ata_ok = false;
    
    while (retries-- && !ata_ok) {
        vga_writestr("Initializing ATA drive... ");
        ata_ok = ata_init();
        if (!ata_ok) {
            vga_writestr("Failed, retrying...\n");
        }
    }
    
    if (!ata_ok) {
        vga_writestr("\nFATAL: Could not initialize ATA drive!\n");
        vga_writestr("System halted.\n");
        while(1);
    }
    
    vga_writestr("OK\n");
    
    // Initialize filesystem
    vga_writestr("Initializing filesystem... ");
    if (!fs_init()) {
        vga_writestr("Failed.\n");
        vga_writestr("System halted.\n");
        while(1);
    }
    
    // Start shell
    shell_init();
    shell_run();
}