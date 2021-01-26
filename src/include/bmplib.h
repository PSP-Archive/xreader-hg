#ifndef _BMPLIB_H
#define _BMPLIB_H

#define TRUE  1

#define FALSE 0

typedef struct tagRGBQUAD {
    unsigned char rgbBlue;
    unsigned char rgbGreen;
    unsigned char rgbRed;
    unsigned char rgbReserved;
} RGBQUAD;

typedef unsigned (*t_bmp_fread)( void *buf, unsigned r, unsigned n, void *stream );
extern t_bmp_fread bmp_fread;

#define BI_RGB        0L
#define BI_RLE8       1L
#define BI_RLE4       2L
#define BI_BITFIELDS  3L

typedef struct tagBITMAPFILEHEADER {
    unsigned short bfType;
    unsigned long  bfSize;
    unsigned short bfReserved1;
    unsigned short bfReserved2;
    unsigned long  bfOffBits;
} BITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER{
    unsigned long  biSize;
    long           biWidth;
    long           biHeight;
    unsigned short biPlanes;
    unsigned short biBitCount;
    unsigned long  biCompression;
    unsigned long  biSizeImage;
    long           biXPelsPerMeter;
    long           biYPelsPerMeter;
    unsigned long  biClrUsed;
    unsigned long  biClrImportant;
} BITMAPINFOHEADER;

typedef unsigned char * DIB;

extern DIB bmp_read_dib_file( FILE *fp, t_bmp_fread readfn );
extern DIB bmp_read_dib_filename( char *filename );
extern int bmp_save_dib_file( FILE *fp, DIB dib );
extern int bmp_save_dib_filename( char *filename, DIB dib );
extern DIB bmp_expand_dib_rle( DIB dib );
extern DIB bmp_compress_dib_rle( DIB dib );

#endif
