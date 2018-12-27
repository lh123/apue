/*
 * 在C语言中，指针通常指向低地址（内存的起始地址）
 * 大端：低地址存放变量的高有效字节。
 * 小端：低地址存放变量的低有效字节。
 */

#include <stdio.h>
#include <stdint.h>

int main(void) {
    uint16_t test = 0xFF00;
    uint8_t t = *(uint8_t *)&test; // 指针指向变量的起始地址
    if (t == 0) {
        printf("little\n");
    } else if (t == 0xFF) {
        printf("big\n");
    } else {
        printf("unknown\n");
    }
}
