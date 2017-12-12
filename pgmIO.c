/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org>

	File: pgmIo.c
	Author: TPB (Halfwit genius)

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


/**
 * A structure containing a basic image definition.
 */
typedef struct _image_s_
{
    unsigned short cols;   /**< number of columns */
    unsigned short rows;   /**< number of rows */
    unsigned short bpp;    /**< num bytes per pixel - (1, 2)x num_channels */
    unsigned char *cData;  /**< 8-bpp image data in row-major order */
    unsigned int  *sData;  /**< 16-bpp image data in row-major order */
} image_t;


typedef struct _pgm_format_t_
{
    char type[2];  /**< P2/P5/ etc */
    int width;     /**< width in pixel units */
    int height;    /**< height in pixel units */ 
    int maxPixVal; /**< bits/pixel = 1, 2 (3 channel is defined by type */
} pgm_format_s;

typedef enum {
    PGM_TYPE = 0,
    PGM_WIDTH,
    PGM_HEIGHT,
    PGM_MAX_VAL,
    PGM_FRAME,
    PGM_UNKNOWN /* never to be used */
} pgm_read_token;

typedef enum{
    PGM_STATE_READ_TYPE = 0,
    PGM_STATE_READ_WIDTH,
    PGM_STATE_READ_HEIGHT,
    PGM_STATE_READ_MAX_VAL,
    PGM_STATE_READ_FRAME,
} pgm_reader_state;

#define BAD_PGM_FORMAT_STRING  -1
#define BAD_PGM_NUMERIC_VAL    -2
#define BAD_PGM_COMMENT_STRING -3
#define BAD_PGM_DATA_CONTENT   -4
#define PGM_OK 1

char *readline (FILE *fp)
{
    char  *comment = NULL;
    int i;
    int len = 0;
    char c = fgetc (fp);
    char line[80];
    while(c != '\n' && c!= '\r' && !feof (fp))
    {
        for(i=0; i<80 && c != '\n' && c!= '\r' && !feof (fp); i++, c=fgetc (fp))
            line[i] = c;
        if(i)
        {
            comment = realloc (comment/*, len*/, len+i+1);
            memcpy (comment+len, line, i);
            comment[len+i] = 0;
            len = len+i;
        }
    }
    if((c=='\n' && (c = fgetc (fp)) != '\r')
       || (c=='\r' && (c = fgetc (fp)) != '\n'))
        ungetc (c, fp);
    return comment;
}

int getTokenString (
    FILE *fp,
    int  pgmType,
    int  *width,
    int  *height,
    int  *bpp,
    int *numComments,
    char ***pppComments
    )
{
    char c;
    int   num = 0;
    char line[80];
    char *comment = NULL;
    char **comments = NULL;
    int  numTokensFound = 0;
    int result = 0;

    c= fgetc (fp);
    if(c == 'P') c = fgetc (fp);
    else          return BAD_PGM_FORMAT_STRING;

    switch(c)
    {
    case '2':
    case '5':
        *pgmType = c-'0';
        break;
    default:
        return BAD_PGM_FORMAT_STRING;
    }

    for(c = fgetc(fp); c && numTokensFound < 3; c = fgetc(fp))
    {
        while ( c == '#') /* read away a line and one more char */
        {
            (*numComments)++;
            comments = realloc (comments/*, sizeof(char*)*(*numComments-1)*/
                                   , sizeof(char*)**numComments);            
            comments[*numComments-1] = readline (fp);
            c = fgetc (fp);
        }
        if(isspace (c))
        {
            if( num > 0)
            {
                if (*width == 0)       *width  = num;
                else if (*height == 0) *height = num;
                else if (*bpp == 0)    *bpp    = num;
                numTokensFound++;
                num = 0;
            }
            continue;
        }
        if(isdigit(c))
            num = num * 10 + c - '0';
        else
        {
            result = BAD_PGM_NUMERIC_VAL;
            break;
        }        
    }
    while(c == '#')
    {
        (*numComments)++;
        comments = realloc (comments/*, sizeof(char*)*(*numComments-1)*/
                               , sizeof(char*)**numComments);
        comments[*numComments-1] = readline (fp);
        c = fgetc (fp);
    }
    /* we have read an extra character  - put it back */
    ungetc (c, fp);
    if(pppComments)
    {
        *pppComments = comments;
    }
    else
    {
        for(;*numComments;(*numComments)--)
            free (comments[*numComments]);
        free (comments);
    }
    return 1;
}

int readPgm (
    char *filename,
    int *numFrames,
    int *numComments,
    char ***pppComments,
    image_t ***pImgs
    )
{
    int rows = 0;
    int cols = 0;
    int bpp = 0;
    image_t *pImg;
    int i;
    FILE *fp = fopen (filename, "rb");
    int result = PGM_OK;

    getTokenString (fp, &cols, &rows, &bpp, numComments, pppComments);
    printf ("%d %d %d %d\n", rows, cols, bpp, *numComments);
    
    for(i=0; i<*numComments;i++)
    {
        printf ("%s\n", (*pppComments)[i]);
    }
    if(bpp > 0 && bpp <= 65535)
        bpp = (bpp>255 && bpp<65535?2:1);
    else
        return BAD_PGM_NUMERIC_VAL;
    while(!feof (fp))
    {
        int readRows = rows;
        pImg = (image_t*)malloc (sizeof(image_t));
        pImg->cData = (char*)malloc (cols*rows*bpp);
        pImg->rows = rows;
        pImg->cols = cols;
        pImg->bpp = bpp;
        if((readRows = fread (pImg->cData, cols*bpp, rows, fp)) == rows)
        {
            *pImgs = (image_t**)realloc (*pImgs/*, sizeof(image_t*)**numFrames*/,
                                                sizeof(image_t*)*(*numFrames+1));
            (*numFrames)++;
            (*pImgs)[*numFrames - 1] = pImg;
        }
        else
        {
            free (pImg);
            result =  BAD_PGM_DATA_CONTENT;
            goto cleanup;
        }
    }
cleanup:
    if(fp) fclose (fp);
    return result;
}
    
int writePgm(
    char *fn,
    image_t **pImg,
    int numFrames
    )
{
    int result = 0;
    FILE *fp;
    int i;
    int len;
    char *fn_frame = NULL;
    for(len = 0; fn[len]; len++);
    
    fn_frame = malloc (len+3+5);
    if(!fn)
    {
        result = -1;
        goto cleanup;
    }

    for(i = 0; i<numFrames; i++)
    {
        sprintf (fn_frame, "%s_%03d.pgm", fn, i);
        fp = fopen (fn_frame, "wb+");
        if(NULL == fp)
        {
            result = -2;
            goto cleanup;
        }
        fprintf (fp, "P5 %d %d %d ",
                 pImg[i]->cols, pImg[i]->rows, 255*(pImg[i]->bpp==8?1:255));
        fwrite (pImg[i]->cData, pImg[i]->cols, pImg[i]->rows, fp);
        fclose (fp);
        fp = NULL;
    }
cleanup:
    if(fn_frame) free (fn_frame);
    if(fp) fclose (fp);
}

#if 1//def UNIT_TEST
int main()
{
    char *comments[] = {"comment 1", "comment 2", "comment 3"};
    int rows = 10;
    int cols = 10;
    int maxVal = 255;
    int imgCount = 0;
    int i;
    image_t *pImgs[8] = {NULL,};
    image_t **pReImgs = NULL;
    char **ppComments = NULL;

    for(imgCount = 0; imgCount < 8; imgCount++)
    {
        image_t *pImg = (image_t*)malloc (sizeof(image_t));
        pImg->cData = (char*)malloc (rows*cols);
        pImgs[imgCount] = pImg;
        pImg->rows = rows;
        pImg->cols = cols;
        pImg->bpp = 8;
        for(i = 0; i<rows*cols; i++)
        {
            pImg->cData[i] = imgCount+i;
        }
    }
    writePgm ("img", pImgs, imgCount);

    for(imgCount = 0; imgCount<8; imgCount++)
    {
        int numFrames = 0;
        int nComments = 0;
        char *fn_frame = malloc (strlen ("img")+10);
        sprintf (fn_frame, "img_%03d.pgm", imgCount);
        printf ("reading %s...\n", fn_frame);
        readPgm (fn_frame, &numFrames, &nComments, &ppComments, &pReImgs);
        printf ("rxc = %dx%d\n", pReImgs[0]->rows, pReImgs[0]->cols);
        if(numFrames == 1 && pReImgs[0]->rows == 10 && pReImgs[0]->cols == 10)
        {
            for(i = 0 ; i<100; i++)
            {
                if(pImgs[imgCount]->cData[i] != pReImgs[0]->cData[i])
                    printf ("broke at pix: %d %d %d\n", i
                            ,pImgs[imgCount]->cData[i], pReImgs[0]->cData[i]);
            }
        }
        else
        {
            printf ("screwed\n");
        }
        printf ("%s done \n", fn_frame);
    }
    
}
#endif
