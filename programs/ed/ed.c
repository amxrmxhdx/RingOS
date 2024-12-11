void _start() {
    volatile unsigned char* video = (volatile unsigned char*)0xB8000;
    const char* msg = "ED editor";

    for(int i = 0; msg[i] != '\0'; i++) {
        video[i * 2] = msg[i];
        video[i * 2 + 1] = 0x07;
    }

    __asm__ volatile(
        "ret"
    );
}
