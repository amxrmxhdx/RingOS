#include "../include/stdint.h"

void uint32_t_to_str(uint32_t num, char* str) {
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    int i = 0;
    char rev[11];

    while (num > 0) {
        rev[i++] = '0' + (num % 10);
        num /= 10;
    }

    int j;
    for (j = 0; j < i; j++) {
        str[j] = rev[i - 1 - j];
    }
    str[j] = '\0';
}