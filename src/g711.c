

#include "g711.h"

#define SIGN_BIT    (0x80)      /* Sign bit for a A-law byte. */
#define QUANT_MASK  (0xf)       /* Quantization field mask. */
#define NSEGS       (8)     /* Number of A-law segments. */
#define SEG_SHIFT   (4)     /* Left shift for segment number. */
#define SEG_MASK    (0x70)      /* Segment field mask. */
static int seg_aend[8] = {0x1F, 0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF};
static int seg_uend[8] = {0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF};

static int search(
    int val,    /* changed from "short" *drago* */
    int    *table,
    int size)   /* changed from "short" *drago* */
{
    int i;      /* changed from "short" *drago* */

    for (i = 0; i < size; i++) {
        if (val <= *table++) {
            return (i);
        }
    }
    return (size);
}

int8_t linear2alaw(int16_t pcm_val)        /* 2's complement (16-bit range) */
/* changed from "short" *drago* */
{
    int     mask;   /* changed from "short" *drago* */
    int     seg;    /* changed from "short" *drago* */
    int     aval;

    pcm_val = pcm_val >> 3;//这里右移3位，因为采样值是16bit，而A-law是13bit，存储在高13位上，低3位被舍弃

    if (pcm_val >= 0) {
        mask = 0xD5;        /* sign (7th) bit = 1 二进制的11010101*/
    } else {
        mask = 0x55;        /* sign bit = 0  二进制的01010101*/
        pcm_val = -pcm_val - 1; //负数转换为正数计算
    }

    /* Convert the scaled magnitude to segment number. */
    seg = search(pcm_val, seg_aend, 8); //查找采样值对应哪一段折线

    /* Combine the sign, segment, and quantization bits. */

    if (seg >= 8) {     /* out of range, return maximum value. */
        return (0x7F ^ mask);
    } else {
//以下按照表格第一二列进行处理，低4位是数据，5~7位是指数，最高位是符号
        aval = seg << SEG_SHIFT;
        if (seg < 2) {
            aval |= (pcm_val >> 1) & QUANT_MASK;
        } else {
            aval |= (pcm_val >> seg) & QUANT_MASK;
        }
        return (aval ^ mask);
    }
}

int16_t alaw2linear(int8_t a_val)
{
    int     t;      /* changed from "short" *drago* */
    int     seg;    /* changed from "short" *drago* */

    a_val ^= 0x55; //异或操作把mask还原

    t = (a_val & QUANT_MASK) << 4;//取低4位，即表中的abcd值，然后左移4位变成abcd0000
    seg = ((unsigned)a_val & SEG_MASK) >> SEG_SHIFT; //取中间3位，指数部分
    switch (seg) {
    case 0: //表中第一行，abcd0000 -> abcd1000
        t += 8;
        break;
    case 1: //表中第二行，abcd0000 -> 1abcd1000
        t += 0x108;
        break;
    default://表中其他行，abcd0000 -> 1abcd1000 的基础上继续左移(按照表格第二三列进行处理)
        t += 0x108;
        t <<= seg - 1;
    }
    return ((a_val & SIGN_BIT) ? t : -t);
}

int8_t ALaw_Encode(int16_t number)
{
    const uint16_t ALAW_MAX = 0xFFF;
    uint16_t mask = 0x800;
    uint8_t sign = 0;
    uint8_t position = 11;
    uint8_t lsb = 0;
    if (number < 0) {
        number = -number;
        sign = 0x80;
    }
    if (number > ALAW_MAX) {
        number = ALAW_MAX;
    }
    for (; ((number & mask) != mask && position >= 5); mask >>= 1, position--);
    lsb = (number >> ((position == 4) ? (1) : (position - 4))) & 0x0f;
    return (sign | ((position - 4) << 4) | lsb) ^ 0x55;
}

int16_t ALaw_Decode(int8_t number)
{
    uint8_t sign = 0x00;
    uint8_t position = 0;
    int16_t decoded = 0;
    number ^= 0x55;
    if (number & 0x80) {
        number &= ~(1 << 7);
        sign = -1;
    }
    position = ((number & 0xF0) >> 4) + 4;
    if (position != 4) {
        decoded = ((1 << position) | ((number & 0x0F) << (position - 4))
                   | (1 << (position - 5)));
    } else {
        decoded = (number << 1) | 1;
    }
    return (sign == 0) ? (decoded) : (-decoded);
}

int8_t MuLaw_Encode(int16_t number)
{
    const uint16_t MULAW_MAX = 0x1FFF;
    const uint16_t MULAW_BIAS = 33;
    uint16_t mask = 0x1000;
    uint8_t sign = 0;
    uint8_t position = 12;
    uint8_t lsb = 0;
    if (number < 0) {
        number = -number;
        sign = 0x80;
    }
    number += MULAW_BIAS;
    if (number > MULAW_MAX) {
        number = MULAW_MAX;
    }
    for (; ((number & mask) != mask && position >= 5); mask >>= 1, position--)
        ;
    lsb = (number >> (position - 4)) & 0x0f;
    return (~(sign | ((position - 5) << 4) | lsb));
}

int16_t MuLaw_Decode(int8_t number)
{
    const uint16_t MULAW_BIAS = 33;
    uint8_t sign = 0, position = 0;
    int16_t decoded = 0;
    number = ~number;
    if (number & 0x80) {
        number &= ~(1 << 7);
        sign = -1;
    }
    position = ((number & 0xF0) >> 4) + 5;
    decoded = ((1 << position) | ((number & 0x0F) << (position - 4))
               | (1 << (position - 5))) - MULAW_BIAS;
    return (sign == 0) ? (decoded) : (-(decoded));
}
