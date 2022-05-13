/*
 * decr_and_flash.c
 *
 *  Created on: 21.12.2021
 *      Author: Thanks to Tom!
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "main.h"

uint8_t key[16] = {0xFE,0x80,0x1C,0xB2,0xD1,0xEF,0x41,0xA6,0xA4,0x17,0x31,0xF5,0xA0,0x68,0x24,0xF0};
uint8_t iv[8] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
int offset = 0;
static uint32_t update_startaddress = 0x08008400;

void update_key()
{
    for (int i = 0; i < 16; i = i + 1) {
        key[i] = (key[i] + i) & 0xFF;
    }
    return;
}

void decrypt_ecb(uint8_t *block, uint8_t *out)
{
    uint32_t y, z;
    memcpy(&y, block, 4);
    memcpy(&z, block + 4, 4);

    uint32_t k[4];
    memcpy(&k[0], key, 4);
    memcpy(&k[1], key + 4, 4);
    memcpy(&k[2], key + 8, 4);
    memcpy(&k[3], key + 12, 4);

    uint32_t s = 0xC6EF3720;

    for (int i = 0; i < 32; i = i + 1) {
        z = (z - (((y << 4) + k[2]) ^ (y + s) ^ ((y >> 5) + k[3]))) & 0xFFFFFFFF;
        y = (y - (((z << 4) + k[0]) ^ (z + s) ^ ((z >> 5) + k[1]))) & 0xFFFFFFFF;
        s = (s - 0x9E3779B9) & 0xFFFFFFFF;
    }

    memcpy(out, &y, 4);
    memcpy(out + 4, &z, 4);

    return;
}

void xor(uint8_t *s1, uint8_t *s2){
    for (int i = 0; i < 8; i+=1) {
        s1[i] = s1[i] ^ s2[i];
    }
}

void decrypt(uint8_t *out, uint8_t *data, int len)
{
    int i;
    uint8_t tmp[8];

    for (i = 0; i < len; i = i + 8) {
        uint8_t ct[8];
        memcpy(ct, data + i, 8);

        decrypt_ecb(ct, tmp);
        xor(tmp, iv);

        memcpy(out + i, tmp, 8);
        memcpy(iv, ct, 8);
        offset += 8;
        if (offset % 1024 == 0) { update_key(); }
    }

    // unpad > remove last 8 bytes (could also verify checksum)

    return;
}

int decr_and_flash(uint8_t enc[], uint32_t flash_address, uint16_t update_size, uint8_t packetsize)
{
   // uint8_t enc[128] = {0x80, 0x97, 0x66, 0xD3, 0x37, 0xD2, 0xB3, 0xB5, 0x79, 0x3E, 0x4C, 0xD0, 0xC9, 0x99, 0xB7, 0xCD, 0x37, 0xA8, 0xA7, 0xC8, 0xEF, 0x8F, 0x40, 0x08, 0xCB, 0xB2, 0x88, 0xFF, 0x72, 0x98, 0xAB, 0x78, 0xEE, 0xBE, 0xCD, 0x70, 0xA5, 0xB6, 0xDD, 0xB4, 0x1C, 0x05, 0x88, 0x85, 0xB5, 0x3D, 0xB3, 0xDD, 0x8C, 0xD1, 0x92, 0x2A, 0x1E, 0xF2, 0xF7, 0xD0, 0xA2, 0x2A, 0x59, 0xB6, 0x80, 0x90, 0xD1, 0x93, 0x92, 0xF5, 0xAF, 0x9B, 0x59, 0xC4, 0x0F, 0x19, 0x6F, 0xA6, 0x13, 0x2E, 0xAD, 0x01, 0x12, 0xF3, 0x65, 0x3B, 0x7A, 0x7C, 0xAB, 0x60, 0xE8, 0xD5, 0xCF, 0xE7, 0xE6, 0xE9, 0x79, 0x32, 0x9F, 0x66, 0xAA, 0x8D, 0x81, 0xC2, 0x32, 0x53, 0x37, 0x55, 0xBF, 0xF8, 0xB1, 0x57, 0x15, 0xDA, 0xCA, 0xFE, 0x1B, 0xF8, 0xEE, 0x60, 0x7F, 0x85, 0xBB, 0x7B, 0x9F, 0x95, 0x54, 0x20, 0xC0, 0x6B, 0x4F, 0x73};
    uint8_t dec[128];
    uint32_t data;
    decrypt(dec, enc, packetsize);
    HAL_FLASH_Unlock();
    char *source = (char *)&dec;
    char *target = (char *)&data;

    for(int i =0 ; i<packetsize; i+=4){
		memcpy(target,source+i,4);
		if(flash_address+i<update_startaddress+update_size){
    	HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, flash_address+i, data);
		}
    }
    HAL_FLASH_Lock();

    return 0;
}
