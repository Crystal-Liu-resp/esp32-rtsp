/*
 * This source code is a product of Sun Microsystems, Inc. and is provided
 * for unrestricted use.  Users may copy or modify this source code without
 * charge.
 *
 * SUN SOURCE CODE IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING
 * THE WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun source code is provided with no support and without any obligation on
 * the part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY THIS SOFTWARE
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
/*
 * g711.c
 *
 * u-law, A-law and linear PCM conversions.
 */

/*
 * December 30, 1994:
 * Functions linear2alaw, linear2ulaw have been updated to correctly
 * convert unquantized 16 bit values.
 * Tables for direct u- to A-law and A- to u-law conversions have been
 * corrected.
 * Borge Lindberg, Center for PersonKommunikation, Aalborg University.
 * bli@cpk.auc.dk
 *
 */
//audio coding
#include "g711.h"

#define SIGN_BIT    (0x80)      /* Sign bit for a A-law byte. */
#define QUANT_MASK  (0xf)       /* Quantization field mask. */
#define NSEGS       (8)     /* Number of A-law segments. */
#define SEG_SHIFT   (4)     /* Left shift for segment number. */
#define SEG_MASK    (0x70)      /* Segment field mask. */
static int seg_aend[8] = {0x1F, 0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF};  //alaw编码解码预制
static int seg_uend[8] = {0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF}; //ulaw编码解码预制


/* copy from CCITT G.711 specifications */
static const unsigned char u2a[128] = {			/* u- to A-law conversions */
	1,	1,	2,	2,	3,	3,	4,	4,
	5,	5,	6,	6,	7,	7,	8,	8,
	9,	10,	11,	12,	13,	14,	15,	16,
	17,	18,	19,	20,	21,	22,	23,	24,
	25,	27,	29,	31,	33,	34,	35,	36,
	37,	38,	39,	40,	41,	42,	43,	44,
	46,	48,	49,	50,	51,	52,	53,	54,
	55,	56,	57,	58,	59,	60,	61,	62,
	64,	65,	66,	67,	68,	69,	70,	71,
	72,	73,	74,	75,	76,	77,	78,	79,
/* corrected:
	81,	82,	83,	84,	85,	86,	87,	88, 
   should be: */
	80,	82,	83,	84,	85,	86,	87,	88,
	89,	90,	91,	92,	93,	94,	95,	96,
	97,	98,	99,	100,	101,	102,	103,	104,
	105,	106,	107,	108,	109,	110,	111,	112,
	113,	114,	115,	116,	117,	118,	119,	120,
	121,	122,	123,	124,	125,	126,	127,	128};

static const unsigned char a2u[128] = {			/* A- to u-law conversions */
	1,	3,	5,	7,	9,	11,	13,	15,
	16,	17,	18,	19,	20,	21,	22,	23,
	24,	25,	26,	27,	28,	29,	30,	31,
	32,	32,	33,	33,	34,	34,	35,	35,
	36,	37,	38,	39,	40,	41,	42,	43,
	44,	45,	46,	47,	48,	48,	49,	49,
	50,	51,	52,	53,	54,	55,	56,	57,
	58,	59,	60,	61,	62,	63,	64,	64,
	65,	66,	67,	68,	69,	70,	71,	72,
/* corrected:
	73,	74,	75,	76,	77,	78,	79,	79,
   should be: */
	73,	74,	75,	76,	77,	78,	79,	80,

	80,	81,	82,	83,	84,	85,	86,	87,
	88,	89,	90,	91,	92,	93,	94,	95,
	96,	97,	98,	99,	100,	101,	102,	103,
	104,	105,	106,	107,	108,	109,	110,	111,
	112,	113,	114,	115,	116,	117,	118,	119,
	120,	121,	122,	123,	124,	125,	126,	127};

//Find which polyline corresponds to the sampled value
//find the Paragraph code
static int search(
    int val,    /* changed from "short" *drago* */
    int    *table,
    int size)   /* changed from "short" *drago* */
{
	int		i;		/* changed from "short" *drago* */

	for (i = 0; i < size; i++) {
		if (val <= *table++)
			return (i);
	}
	return (size);
}

/*
 * linear2alaw() - Convert a 16-bit linear PCM value to 8-bit A-law
 *
 * linear2alaw() accepts an 16-bit integer and encodes it as A-law data.
 *
 
 *		Linear Input Code	Compressed Code
 *	------------------------	---------------
 *	0000000wxyza			000wxyz
 *	0000001wxyza			001wxyz
 *	000001wxyzab			010wxyz
 *	00001wxyzabc			011wxyz
 *	0001wxyzabcd			100wxyz
 *	001wxyzabcde			101wxyz
 *	01wxyzabcdef			110wxyz
 *	1wxyzabcdefg			111wxyz
 *
 * For further information see John C. Bellamy's Digital Telephony, 1982,
 * John Wiley & Sons, pps 98-111 and 472-476.
 * 
 * g711a input：13bits(the high 13bits of S16)，easy for fast calculations

A-law is calculated in the following table, the first column is the sampling point, a total of 13 bits, 
and the highest bit is the sign bit. For the first two rows, the slope of the polyline is both 1/2, 
which is located on the same polyline as the corresponding area of ​​the negative half.
For rows 3 to 8, the slope is from 1/4 to 1/128, a total of 6 polylines, 
plus 6 polylines corresponding to the negative half, a total of 13 polylines, 
this is the so-called A-law 13-segment polyline method .

Example：
The input PCM data is 1234, and the binary corresponding is （0000 0100 1101 0010）
      Permutation and Combination Mode under Binary Transformation（0 00001 0011 010010）
    1、Take the sign bit (0) and negate it to get s=1
    2、00001,look up the table， get the intensity bit eee=011
    3、Get high sample bit wxyz=0011
    4、The combination is seeewxyz=10110011， and the even number of seeewxyz is taken as complement=11100110，namely E6
 */
int linear2alaw(int	pcm_val)        /* 2's complement (16-bit range) */
/* changed from "short" *drago* */
{
    int     mask;   /* changed from "short" *drago* */
    int     seg;    /* changed from "short" *drago* */
    int     aval;
	
	/*
	 *shifted 3 bits to the right, because the sample value is 16bit, and the A-law is 13bit, 
	 *which is stored in the upper 13 bits, and the lower 3 bits are discarded
	 */
	pcm_val = pcm_val >> 3;

    if (pcm_val >= 0) {
        mask = 0xD5;        /* sign (7th) bit = 1 */
    } else {
        mask = 0x55;        /* sign bit = 0 */
        pcm_val = -pcm_val - 1; //Conversion of negative numbers to positive calculations
    }

    /* Convert the scaled magnitude to segment number. */
    seg = search(pcm_val, seg_aend, 8); //Find which polyline of the sampled value corresponds to alaw

    /* Combine the sign, segment, and quantization bits. */

/*
 * The following is processed according to the first and second columns of the table, 
 * the lower 4 bits are data, 5~7 bits are exponents, and the highest bit is sign.
 * Combine the obtained sign bit, paragraph number, 
 * and quantized value in the paragraph into an 8-bit number output.
 */
    if (seg >= 8) {     /* out of range, return maximum value. */
        return (0x7F ^ mask);
    } else {

        aval = seg << SEG_SHIFT;
        if (seg < 2) {
            aval |= (pcm_val >> 1) & QUANT_MASK;//For the first two rows, the slope of the broken line is both 1/2.
        } else {
            aval |= (pcm_val >> seg) & QUANT_MASK;//For rows 3 to 8, the slopes are 1/4 to 1/128, and there are 6 polylines in total.
        }
        return (aval ^ mask);
    }
}

/*
 * alaw2linear() - Convert an A-law value to 16-bit linear PCM
 *
 */
int16_t alaw2linear(int8_t a_val)
{
	int		t;      /* changed from "short" *drago* */
	int		seg;    /* changed from "short" *drago* */

	a_val ^= 0x55;

	t = (a_val & QUANT_MASK) << 4;
	seg = ((unsigned)a_val & SEG_MASK) >> SEG_SHIFT;
	switch (seg) {
	case 0:
		t += 8;
		break;
	case 1:
		t += 0x108;
		break;
	default:
		t += 0x108;
		t <<= seg - 1;
	}
	return ((a_val & SIGN_BIT) ? t : -t);
}

#define	BIAS		(0x84)		/* Bias for linear code. */
#define CLIP            8159

/*
 * linear2ulaw() - Convert a linear PCM value to u-law
 *
 * In order to simplify the encoding process, the original linear magnitude
 * is biased by adding 33 which shifts the encoding range from (0 - 8158) to
 * (33 - 8191). The result can be seen in the following encoding table:
 *
 *	Biased Linear Input Code	Compressed Code
 *	------------------------	---------------
 *	00000001wxyza			000wxyz
 *	0000001wxyzab			001wxyz
 *	000001wxyzabc			010wxyz
 *	00001wxyzabcd			011wxyz
 *	0001wxyzabcde			100wxyz
 *	001wxyzabcdef			101wxyz
 *	01wxyzabcdefg			110wxyz
 *	1wxyzabcdefgh			111wxyz
 *
 * Each biased linear code has a leading 1 which identifies the segment
 * number. The value of the segment number is equal to 7 minus the number
 * of leading 0's. The quantization interval is directly available as the
 * four bits wxyz.  * The trailing bits (a - h) are ignored.
 *
 * Ordinarily the complement of the resulting code word is used for
 * transmission, and so the code word is complemented before it is returned.
 *
 * For further information see John C. Bellamy's Digital Telephony, 1982,
 * John Wiley & Sons, pps 98-111 and 472-476.
 */
int linear2ulaw( int	pcm_val)	/* 2's complement (16-bit range) */
{
	int		mask;
	int		seg;
	int		uval;

	/* Get the sign and the magnitude of the value. */
	pcm_val = pcm_val >> 2;
	if (pcm_val < 0) {
		pcm_val = -pcm_val;
		mask = 0x7F;
	} else {
		mask = 0xFF;
	}
        if ( pcm_val > CLIP ) pcm_val = CLIP;		/* clip the magnitude */
	pcm_val += (BIAS >> 2);

	/* Convert the scaled magnitude to segment number. */
	seg = search(pcm_val, seg_uend, 8);

	/*
	 * Combine the sign, segment, quantization bits;
	 * and complement the code word.
	 */
	if (seg >= 8)		/* out of range, return maximum value. */
		return (0x7F ^ mask);
	else {
		uval = (seg << 4) | ((pcm_val >> (seg + 1)) & 0xF);
		return (uval ^ mask);
	}

}

/*
 * ulaw2linear() - Convert a u-law value to 16-bit linear PCM
 *
 * First, a biased linear code is derived from the code word. An unbiased
 * output can then be obtained by subtracting 33 from the biased code.
 *
 * Note that this function expects to be passed the complement of the
 * original code word. This is in keeping with ISDN conventions.
 */
int ulaw2linear( int	u_val)
{
	int t;

	/* Complement to obtain normal u-law value. */
	u_val = ~u_val;

	/*
	 * Extract and bias the quantization bits. Then
	 * shift up by the segment number and subtract out the bias.
	 */
	t = ((u_val & QUANT_MASK) << 3) + BIAS;
	t <<= (u_val & SEG_MASK) >> SEG_SHIFT;

	return ((u_val & SIGN_BIT) ? (BIAS - t) : (t - BIAS));
}

/*
Input PCM data is 1234

1. Obtain the range value, look up the table and get +2014 to +991 in 16 intervals of 64
2. The base value is 0xA0
3. The number of intervals is 64
4. Obtain the basic value of the interval 2014
5. The difference between the current value 1234 and the basic value of the interval 2014-1234=780
6. Offset value =780/ interval number =780/64, rounded to get 12
The output is 0xA0+12=0xAC
*/

/* A-law to u-law conversion */
static int alaw2ulaw (int	aval)
{
	aval &= 0xff;
	return ((aval & 0x80) ? (0xFF ^ a2u[aval ^ 0xD5]) :
	    (0x7F ^ a2u[aval ^ 0x55]));
}

/* u-law to A-law conversion */
static int ulaw2alaw (int	uval)
{
	uval &= 0xff;
	return ((uval & 0x80) ? (0xD5 ^ (u2a[0xFF ^ uval] - 1)) :
	    (0x55 ^ (u2a[0x7F ^ uval] - 1)));
}
