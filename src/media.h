
#ifndef _RTP_MEDIA_H_
#define _RTP_MEDIA_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Defined in RFC3551
 * https://datatracker.ietf.org/doc/html/rfc3551#page-33
 *
 */

/**
   PT   encoding    media type  clock rate   channels
        name                    (Hz)
   ___________________________________________________
   0    PCMU        A            8,000       1
   1    reserved    A
   2    reserved    A
   3    GSM         A            8,000       1
   4    G723        A            8,000       1
   5    DVI4        A            8,000       1
   6    DVI4        A           16,000       1
   7    LPC         A            8,000       1
   8    PCMA        A            8,000       1
   9    G722        A            8,000       1
   10   L16         A           44,100       2
   11   L16         A           44,100       1
   12   QCELP       A            8,000       1
   13   CN          A            8,000       1
   14   MPA         A           90,000       (see text)
   15   G728        A            8,000       1
   16   DVI4        A           11,025       1
   17   DVI4        A           22,050       1
   18   G729        A            8,000       1
   19   reserved    A
   20   unassigned  A
   21   unassigned  A
   22   unassigned  A
   23   unassigned  A
   dyn  G726-40     A            8,000       1
   dyn  G726-32     A            8,000       1
   dyn  G726-24     A            8,000       1
   dyn  G726-16     A            8,000       1
   dyn  G729D       A            8,000       1
   dyn  G729E       A            8,000       1
   dyn  GSM-EFR     A            8,000       1
   dyn  L8          A            var.        var.
   dyn  RED         A                        (see text)
   dyn  VDVI        A            var.        1

Table 4: Payload types (PT) for audio encodings

   PT      encoding    media type  clock rate
           name                    (Hz)
   _____________________________________________
   24      unassigned  V
   25      CelB        V           90,000
   26      JPEG        V           90,000
   27      unassigned  V
   28      nv          V           90,000
   29      unassigned  V
   30      unassigned  V
   31      H261        V           90,000
   32      MPV         V           90,000
   33      MP2T        AV          90,000
   34      H263        V           90,000
   35-71   unassigned  ?
   72-76   reserved    N/A         N/A
   77-95   unassigned  ?
   96-127  dynamic     ?
   dyn     H263-1998   V           90,000

   Table 5: Payload types (PT) for video and combined encodings
 */

typedef enum {
    RTP_PT_PCMU = 0,
    RTP_PT_GSM = 3,
    RTP_PT_G723 = 4,
    RTP_PT_DVI4_8000 = 5,
    RTP_PT_DVI4_16000 = 6,
    RTP_PT_LPC = 7,
    RTP_PT_PCMA = 8,
    RTP_PT_G722 = 9,
    RTP_PT_JPEG = 26,
    RTP_PT_H264 = 96,
    RTP_PT_AAC  = 37,
    RTP_PT_H265 = 265,
} MediaType_t;

typedef enum {
    VIDEO_FRAME_I = 0x01,
    VIDEO_FRAME_P = 0x02,
    VIDEO_FRAME_B = 0x03,
    AUDIO_FRAME   = 0x11,
} FrameType;

#ifdef __cplusplus
}
#endif

#endif
