/* Facet firmware (firmware/fw_imgdec.h): TJpgDec R0.03 by ChaN, copied from LVGL 9 src/libs/tjpgd (its LV_USE_TJPGD
 * guard and lv_conf include removed, the config inlined with descaling on). NB LVGL's copy writes RGB888 as B, G, R. License: ChaN's, kept below. */
/*----------------------------------------------------------------------------/
/ TJpgDec - Tiny JPEG Decompressor R0.03 include file         (C)ChaN, 2021
/----------------------------------------------------------------------------*/
#ifndef FW_TJPGD_H
#define FW_TJPGD_H

#ifdef __cplusplus
extern "C" {
#endif

/* configuration (was tjpgdcnf.h; Facet: RGB888 out, 1/2 1/4 1/8 descaling on, 32-bit fast decode) */
#define JD_SZBUF        512
#define JD_FORMAT       0
#define JD_USE_SCALE    1
#define JD_TBLCLIP      1
#define JD_FASTDECODE   1
/* Facet: own names, so this copy never meets LVGL's (libs/tjpgd) or a chip ROM's tjpgd at link time */
#define jd_prepare fw_jd_prepare
#define jd_decomp fw_jd_decomp


#include <string.h>
#include <stdint.h>

#if JD_FASTDECODE >= 1
typedef int16_t jd_yuv_t;
#else
typedef uint8_t jd_yuv_t;
#endif


/* Error code */
typedef enum {
    JDR_OK = 0, /* 0: Succeeded */
    JDR_INTR,   /* 1: Interrupted by output function */
    JDR_INP,    /* 2: Device error or wrong termination of input stream */
    JDR_MEM1,   /* 3: Insufficient memory pool for the image */
    JDR_MEM2,   /* 4: Insufficient stream input buffer */
    JDR_PAR,    /* 5: Parameter error */
    JDR_FMT1,   /* 6: Data format error (may be broken data) */
    JDR_FMT2,   /* 7: Right format but not supported */
    JDR_FMT3    /* 8: Not supported JPEG standard */
} JRESULT;



/* Rectangular region in the output image */
typedef struct {
    uint16_t left;      /* Left end */
    uint16_t right;     /* Right end */
    uint16_t top;       /* Top end */
    uint16_t bottom;    /* Bottom end */
} JRECT;



/* Decompressor object structure */
typedef struct JDEC JDEC;
struct JDEC {
    size_t dctr;                /* Number of bytes available in the input buffer */
    uint8_t * dptr;             /* Current data read ptr */
    uint8_t * inbuf;            /* Bit stream input buffer */
    uint8_t dbit;               /* Number of bits available in wreg or reading bit mask */
    uint8_t scale;              /* Output scaling ratio */
    uint8_t msx, msy;           /* MCU size in unit of block (width, height) */
    uint8_t qtid[3];            /* Quantization table ID of each component, Y, Cb, Cr */
    uint8_t ncomp;              /* Number of color components 1:grayscale, 3:color */
    int16_t dcv[3];             /* Previous DC element of each component */
    uint16_t nrst;              /* Restart interval */
    uint16_t rst;              /* Restart count*/
    uint16_t rsc;               /* Expected restart sequence ID*/
    uint16_t width, height;     /* Size of the input image (pixel) */
    uint8_t * huffbits[2][2];   /* Huffman bit distribution tables [id][dcac] */
    uint16_t * huffcode[2][2];  /* Huffman code word tables [id][dcac] */
    uint8_t * huffdata[2][2];   /* Huffman decoded data tables [id][dcac] */
    int32_t * qttbl[4];         /* Dequantizer tables [id] */
#if JD_FASTDECODE >= 1
    uint32_t wreg;              /* Working shift register */
    uint8_t marker;             /* Detected marker (0:None) */
#if JD_FASTDECODE == 2
    uint8_t longofs[2][2];      /* Table offset of long code [id][dcac] */
    uint16_t * hufflut_ac[2];   /* Fast huffman decode tables for AC short code [id] */
    uint8_t * hufflut_dc[2];    /* Fast huffman decode tables for DC short code [id] */
#endif
#endif
    void * workbuf;             /* Working buffer for IDCT and RGB output */
    jd_yuv_t * mcubuf;          /* Working buffer for the MCU */
    void * pool;                /* Pointer to available memory pool */
    void * pool_original;       /* Pointer to original pool */
    size_t sz_pool;             /* Size of memory pool (bytes available) */
    size_t (*infunc)(JDEC *, uint8_t *, size_t); /* Pointer to jpeg stream input function */
    void * device;              /* Pointer to I/O device identifier for the session */
};



/* TJpgDec API functions */
JRESULT jd_prepare(JDEC * jd, size_t (*infunc)(JDEC *, uint8_t *, size_t), void * pool, size_t sz_pool, void * dev);

JRESULT jd_decomp(JDEC * jd, int (*outfunc)(JDEC *, void *, JRECT *), uint8_t scale);

JRESULT jd_mcu_load(JDEC * jd);

JRESULT jd_mcu_output(JDEC * jd, int (*outfunc)(JDEC *, void *, JRECT *), unsigned int x, unsigned int y);

JRESULT jd_restart(JDEC * jd, uint16_t rstn);


#ifdef __cplusplus
}
#endif

#endif /* _TJPGDEC */
