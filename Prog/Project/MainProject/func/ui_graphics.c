#include <stdlib.h>
#include <string.h>

#ifdef USE_GLIB_ON_EWARM
    #include "stm32f10x.h"
    #include "stdlib_extension.h"
#endif

#include "typedefs.h"
#include "ui_graphics.h"
#include "graphic_lib.h"

////////////////////////////////////////////////////
//
//   Fonst
//
////////////////////////////////////////////////////


// Static Buffer in data space for font 'font_large_num'
const uint8 font_large_num[]= { 0xC0, 0x16, 0x00, 0x20, 0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0x2F, 0x09, 0xF8, 0x7F, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x08, 0xF7, 0xF7, 0xE7, 0xE7, 0xE7, 0xEF, 0xEF, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x60, 0x86, 0x19, 0x66, 0x98, 0x61, 0xF6, 0xFF, 0xFF, 0xFF,
                              0x33, 0xC3, 0x0C, 0x33, 0xCC, 0xFC, 0xFF, 0xFF, 0xFF, 0xC6, 0x18, 0x63, 0x8C, 0x31, 0xC6, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x0A, 0x78, 0xF8, 0xE7, 0xDF, 0xED, 0xB7, 0xDF, 0x70, 0xC3, 0x0D, 0x3F,
                              0xF8, 0xE1, 0x1F, 0x7F, 0xF0, 0xC3, 0x0F, 0x3B, 0xEC, 0xB7, 0xDF, 0xEE, 0x9B, 0x7F, 0xFC, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x0A, 0x0E, 0x3B, 0xBC, 0xD9, 0x66, 0xDB, 0x6C, 0xE3, 0x86, 0x1B, 0x30,
                              0xC0, 0x80, 0x01, 0x06, 0x18, 0x30, 0xC7, 0x9C, 0xD9, 0x66, 0x9B, 0x3D, 0xF6, 0x70, 0xC3, 0x01,
                              0x00, 0x00, 0x00, 0x00, 0x0A, 0xF8, 0xF0, 0xE7, 0xBF, 0xE3, 0x8E, 0x3B, 0xEE, 0x3D, 0x7F, 0xFC,
                              0xF0, 0xE1, 0x83, 0x0F, 0x77, 0xCC, 0x3B, 0xFE, 0xF0, 0xC3, 0x1D, 0xE7, 0xBF, 0xFF, 0x7C, 0x02,
                              0x00, 0x00, 0x00, 0x00, 0x03, 0xFF, 0xFF, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x05, 0x98, 0x33, 0xE6, 0x9C, 0x33, 0xE7, 0x9C, 0x73, 0x8E, 0x31, 0x8C,
                              0x31, 0x8C, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x05, 0xE3, 0x18, 0xE3, 0x9C, 0x63, 0x9C, 0x73, 0xCE, 0x39, 0x63, 0xC6,
                              0x98, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x08, 0x18, 0x18, 0x99, 0xFF, 0xFF, 0x3C, 0x3C, 0x3C, 0x66, 0x24, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0xC0, 0x01, 0x0E,
                              0x70, 0x80, 0xC3, 0xFF, 0xFF, 0xFF, 0x7F, 0x38, 0xC0, 0x01, 0x0E, 0x70, 0x80, 0x03, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x7F, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF,
                              0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0xED, 0x01, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x06, 0x30, 0x0C, 0x63, 0x18, 0x86, 0x61, 0x0C, 0xC3, 0x30, 0x8C, 0x61,
                              0x18, 0x86, 0x31, 0x0C, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x0A, 0x78, 0xF0, 0xE3, 0x9F, 0x73, 0x87, 0x1F, 0x7E, 0xF8, 0xE1, 0x87,
                              0x1F, 0x7E, 0xF8, 0xE1, 0x87, 0x1F, 0x7E, 0xF8, 0xE1, 0x8F, 0x3B, 0xE7, 0x1F, 0x3F, 0x78, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x07, 0x70, 0x38, 0x9E, 0xEF, 0xFF, 0xDF, 0xE7, 0x71, 0x38, 0x1C, 0x0E,
                              0x87, 0xC3, 0xE1, 0x70, 0x38, 0x1C, 0x0E, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x0A, 0x78, 0xF8, 0xE7, 0xDF, 0xF3, 0x87, 0x1F, 0x0E, 0x38, 0xE0, 0xC0,
                              0x01, 0x07, 0x0E, 0x3C, 0x70, 0xE0, 0xC0, 0x03, 0x07, 0x1E, 0x38, 0xF0, 0xFF, 0xFF, 0xFF, 0x03,
                              0x00, 0x00, 0x00, 0x00, 0x0A, 0x78, 0xF0, 0xE3, 0x9F, 0x73, 0xC7, 0x19, 0x07, 0x1E, 0x3C, 0x70,
                              0xC0, 0x03, 0x1C, 0xE0, 0x80, 0x03, 0x0E, 0xF8, 0xE1, 0x87, 0x3F, 0xE7, 0x1F, 0x3F, 0x78, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x0A, 0xC0, 0x01, 0x07, 0x1E, 0x78, 0xF0, 0xC1, 0x87, 0x1D, 0x76, 0xD8,
                              0x31, 0xC7, 0x9C, 0x71, 0xC6, 0x0D, 0xF7, 0xFF, 0xFF, 0xFF, 0x03, 0x07, 0x1C, 0x70, 0xC0, 0x01,
                              0x00, 0x00, 0x00, 0x00, 0x0A, 0xFC, 0xF1, 0xC7, 0x1F, 0x01, 0x06, 0x18, 0xE0, 0x87, 0x3F, 0xFF,
                              0x1D, 0x37, 0x38, 0xE0, 0x80, 0x03, 0x0E, 0x38, 0xE0, 0x87, 0x3F, 0xE7, 0x9F, 0x3F, 0x78, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x0A, 0xF8, 0xF0, 0xE7, 0x9F, 0xF3, 0x87, 0x1F, 0x70, 0xC0, 0x1D, 0xFF,
                              0xFC, 0xF7, 0xDC, 0xE1, 0x87, 0x1F, 0x7E, 0xF8, 0xE1, 0x87, 0x3B, 0xE7, 0x1F, 0x3F, 0x78, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x0A, 0xFF, 0xFF, 0xFF, 0x3F, 0xE0, 0xC0, 0x01, 0x07, 0x0E, 0x38, 0x70,
                              0xC0, 0x01, 0x07, 0x0E, 0x38, 0xE0, 0x80, 0x03, 0x06, 0x1C, 0x70, 0xC0, 0x01, 0x07, 0x1C, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x0A, 0x78, 0xF8, 0xE7, 0xDF, 0xF3, 0x87, 0x1F, 0x7E, 0xB8, 0x73, 0xFE,
                              0xF1, 0xE3, 0x9F, 0x73, 0x87, 0x1F, 0x7E, 0xF8, 0xE1, 0x87, 0x3F, 0xEF, 0x1F, 0x3F, 0x78, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x0A, 0x78, 0xF0, 0xE3, 0x9F, 0x73, 0x87, 0x1F, 0x7E, 0xF8, 0xE1, 0x87,
                              0x1F, 0xEE, 0xBC, 0xFF, 0xFC, 0xE3, 0x0E, 0x38, 0xE0, 0x87, 0x3F, 0xE7, 0x9F, 0x3F, 0x7C, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x80, 0xFF, 0x07, 0xF0, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00};



// Static Buffer in data space for font 'font_small'
const uint8 font_small[] = { 0x46, 0x08, 0x00, 0x01, 0x5F, 0x00, 0x00, 0x00, 0x00, 0x04, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x04,
                              0x60, 0x6F, 0x6F, 0x00, 0x00, 0x04, 0xE4, 0x61, 0x78, 0x04, 0x00, 0x04, 0x9B, 0x44, 0x92, 0x0D,
                              0x00, 0x05, 0xC4, 0x99, 0x51, 0x93, 0x05, 0x02, 0x06, 0x00, 0x00, 0x00, 0x00, 0x02, 0x56, 0x25,
                              0x00, 0x00, 0x00, 0x02, 0xA9, 0x1A, 0x00, 0x00, 0x00, 0x03, 0x55, 0x01, 0x00, 0x00, 0x00, 0x05,
                              0x80, 0x90, 0x4F, 0x08, 0x00, 0x02, 0x00, 0x68, 0x00, 0x00, 0x00, 0x04, 0x00, 0xF0, 0x00, 0x00,
                              0x00, 0x01, 0x40, 0x00, 0x00, 0x00, 0x00, 0x04, 0x88, 0x44, 0x22, 0x01, 0x00, 0x04, 0x96, 0x9D,
                              0x9B, 0x06, 0x00, 0x03, 0x9A, 0x24, 0x1D, 0x00, 0x00, 0x04, 0x96, 0x48, 0x12, 0x0F, 0x00, 0x04,
                              0x96, 0x48, 0x98, 0x06, 0x00, 0x04, 0x99, 0xE9, 0x88, 0x08, 0x00, 0x04, 0x1F, 0x71, 0x88, 0x07,
                              0x00, 0x04, 0x16, 0x71, 0x99, 0x06, 0x00, 0x04, 0x8F, 0x24, 0x22, 0x02, 0x00, 0x04, 0x96, 0x69,
                              0x99, 0x06, 0x00, 0x04, 0x96, 0xE9, 0x88, 0x06, 0x00, 0x01, 0x24, 0x00, 0x00, 0x00, 0x00, 0x02,
                              0x20, 0x18, 0x00, 0x00, 0x00, 0x03, 0xA0, 0x22, 0x02, 0x00, 0x00, 0x04, 0x00, 0x0F, 0x0F, 0x00,
                              0x00, 0x03, 0x88, 0xA8, 0x00, 0x00, 0x00, 0x03, 0x23, 0x25, 0x08, 0x00, 0x00, 0x04, 0x96, 0xDD,
                              0x61, 0x00, 0x00, 0x04, 0x96, 0xF9, 0x99, 0x09, 0x00, 0x04, 0x97, 0x79, 0x99, 0x07, 0x00, 0x04,
                              0x96, 0x11, 0x91, 0x06, 0x00, 0x04, 0x97, 0x99, 0x99, 0x07, 0x00, 0x04, 0x1F, 0x71, 0x11, 0x0F,
                              0x00, 0x04, 0x1F, 0x71, 0x11, 0x01, 0x00, 0x04, 0x1E, 0xD1, 0x99, 0x0E, 0x00, 0x04, 0x99, 0xF9,
                              0x99, 0x09, 0x00, 0x03, 0x97, 0x24, 0x1D, 0x00, 0x00, 0x04, 0x8C, 0x88, 0x98, 0x06, 0x00, 0x04,
                              0x99, 0x35, 0x95, 0x09, 0x00, 0x04, 0x11, 0x11, 0x11, 0x0F, 0x00, 0x05, 0x71, 0xD7, 0x18, 0x63,
                              0x04, 0x04, 0x99, 0xB9, 0x9D, 0x09, 0x00, 0x04, 0x96, 0x99, 0x99, 0x06, 0x00, 0x04, 0x97, 0x79,
                              0x11, 0x01, 0x00, 0x04, 0x96, 0x99, 0xB9, 0x07, 0x00, 0x04, 0x97, 0x79, 0x99, 0x09, 0x00, 0x04,
                              0x1E, 0x61, 0x88, 0x07, 0x00, 0x05, 0x9F, 0x10, 0x42, 0x08, 0x01, 0x04, 0x99, 0x99, 0x99, 0x06,
                              0x00, 0x04, 0x99, 0x99, 0x69, 0x06, 0x00, 0x05, 0x31, 0xC6, 0x5A, 0xBF, 0x02, 0x04, 0x99, 0x69,
                              0x96, 0x09, 0x00, 0x04, 0x99, 0x69, 0x46, 0x04, 0x00, 0x04, 0x8F, 0x24, 0x12, 0x0F, 0x00, 0x02,
                              0x57, 0x35, 0x00, 0x00, 0x00, 0x04, 0x11, 0x22, 0x44, 0x08, 0x00, 0x02, 0xAB, 0x3A, 0x00, 0x00,
                              0x00, 0x03, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xF0, 0x00, 0x02, 0x09, 0x00,
                              0x00, 0x00, 0x00, 0x03, 0xC0, 0xE8, 0x1E, 0x00, 0x00, 0x03, 0xC9, 0xDA, 0x0E, 0x00, 0x00, 0x03,
                              0x80, 0x93, 0x18, 0x00, 0x00, 0x03, 0xA4, 0xDB, 0x1A, 0x00, 0x00, 0x03, 0x80, 0xFA, 0x18, 0x00,
                              0x00, 0x02, 0xD6, 0x15, 0x00, 0x00, 0x00, 0x03, 0x80, 0xDB, 0x73, 0x00, 0x00, 0x03, 0xC9, 0xDA,
                              0x16, 0x00, 0x00, 0x01, 0x7A, 0x00, 0x00, 0x00, 0x00, 0x02, 0x88, 0x6A, 0x00, 0x00, 0x00, 0x03,
                              0x49, 0xBB, 0x16, 0x00, 0x00, 0x01, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0xAC, 0x5A, 0x63,
                              0x04, 0x03, 0xC0, 0xDA, 0x16, 0x00, 0x00, 0x04, 0x00, 0x96, 0x99, 0x06, 0x00, 0x03, 0xC0, 0xDA,
                              0x25, 0x00, 0x00, 0x03, 0x80, 0x5B, 0x93, 0x00, 0x00, 0x03, 0xC0, 0x9A, 0x04, 0x00, 0x00, 0x03,
                              0x80, 0x23, 0x0E, 0x00, 0x00, 0x03, 0xBA, 0x24, 0x11, 0x00, 0x00, 0x04, 0x00, 0x99, 0x99, 0x0E,
                              0x00, 0x04, 0x00, 0x99, 0xA9, 0x04, 0x00, 0x05, 0x00, 0xC4, 0x58, 0xAB, 0x02, 0x03, 0x40, 0xAB,
                              0x16, 0x00, 0x00, 0x03, 0x40, 0x5B, 0x73, 0x00, 0x00, 0x03, 0xC0, 0xA9, 0x1C, 0x00, 0x00, 0x03,
                              0x94, 0x26, 0x11, 0x00, 0x00, 0x01, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x03, 0x91, 0x2C, 0x05, 0x00,
                              0x00, 0x05, 0x00, 0xC8, 0x9A, 0x00, 0x00};


// Static Buffer in data space for font 'font_small_bold'
const uint8 font_small_bold[] = { 0x49, 0x08, 0x00, 0x02, 0xFF, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x3D, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0xEA, 0x7F, 0xF5, 0xBF, 0x02, 0x00, 0x00, 0x00, 0x05, 0xCA,
                              0xAF, 0xE7, 0xF5, 0x53, 0x00, 0x00, 0x00, 0x06, 0xD3, 0x86, 0x30, 0x84, 0x2D, 0x03, 0x00, 0x00,
                              0x06, 0xCE, 0xE6, 0xBC, 0xFB, 0xEE, 0x02, 0x00, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x03, 0xD4, 0xB6, 0x89, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x91, 0x6D, 0x2B, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x05, 0xA4, 0x7E, 0xF7, 0x2B, 0x01, 0x00, 0x00, 0x00, 0x05, 0x80, 0x90,
                              0x4F, 0x08, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
                              0x00, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x03, 0xA4, 0xB5, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x6E, 0xEF, 0xBD, 0xB7, 0x03,
                              0x00, 0x00, 0x00, 0x03, 0xBE, 0x6D, 0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x0F, 0x63, 0x37,
                              0xC6, 0x07, 0x00, 0x00, 0x00, 0x05, 0x0F, 0x63, 0x87, 0xF1, 0x03, 0x00, 0x00, 0x00, 0x05, 0x98,
                              0xEB, 0xFC, 0x31, 0x06, 0x00, 0x00, 0x00, 0x05, 0x2F, 0x3C, 0x8C, 0xF1, 0x03, 0x00, 0x00, 0x00,
                              0x05, 0x6E, 0xBC, 0xBD, 0xB7, 0x03, 0x00, 0x00, 0x00, 0x05, 0x1F, 0x33, 0x66, 0x8C, 0x01, 0x00,
                              0x00, 0x00, 0x05, 0x6E, 0x6F, 0xB7, 0xB7, 0x03, 0x00, 0x00, 0x00, 0x05, 0x6E, 0xEF, 0xED, 0xB1,
                              0x03, 0x00, 0x00, 0x00, 0x02, 0xF0, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xF0, 0x78,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0xC8, 0x36, 0xC6, 0x08, 0x00, 0x00, 0x00, 0x00, 0x04,
                              0x00, 0xF0, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x31, 0xC6, 0x36, 0x01, 0x00, 0x00, 0x00,
                              0x00, 0x05, 0x0F, 0x33, 0x63, 0x80, 0x01, 0x00, 0x00, 0x00, 0x06, 0x80, 0x37, 0xFF, 0xFB, 0x3F,
                              0x78, 0x00, 0x00, 0x05, 0x6E, 0xEF, 0xFD, 0xF7, 0x06, 0x00, 0x00, 0x00, 0x05, 0x6F, 0xBF, 0xBD,
                              0xF7, 0x03, 0x00, 0x00, 0x00, 0x05, 0x7E, 0x8C, 0x31, 0x86, 0x07, 0x00, 0x00, 0x00, 0x05, 0x6F,
                              0xEF, 0xBD, 0xF7, 0x03, 0x00, 0x00, 0x00, 0x05, 0x7F, 0xBC, 0x31, 0xC6, 0x07, 0x00, 0x00, 0x00,
                              0x05, 0x7F, 0xBC, 0x31, 0xC6, 0x00, 0x00, 0x00, 0x00, 0x05, 0x6E, 0x8C, 0xBD, 0xB7, 0x07, 0x00,
                              0x00, 0x00, 0x05, 0x7B, 0xFF, 0xBD, 0xF7, 0x06, 0x00, 0x00, 0x00, 0x02, 0xFF, 0x3F, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x04, 0xCC, 0xCC, 0xCC, 0x07, 0x00, 0x00, 0x00, 0x00, 0x06, 0xF3, 0xF6,
                              0x1C, 0xCF, 0x36, 0x03, 0x00, 0x00, 0x04, 0x33, 0x33, 0x33, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x07,
                              0xC1, 0xF1, 0xFD, 0xBF, 0x1E, 0x8F, 0x01, 0x00, 0x06, 0xF1, 0x7C, 0xFF, 0xFB, 0x3C, 0x02, 0x00,
                              0x00, 0x06, 0xDE, 0x3C, 0xCF, 0xF3, 0xEC, 0x01, 0x00, 0x00, 0x05, 0x6F, 0xEF, 0xFD, 0xC6, 0x00,
                              0x00, 0x00, 0x00, 0x06, 0xDE, 0x3C, 0xCF, 0xF3, 0xEE, 0xC1, 0x00, 0x00, 0x05, 0x6F, 0xEF, 0xFD,
                              0xD6, 0x06, 0x00, 0x00, 0x00, 0x04, 0x3E, 0x63, 0xCC, 0x07, 0x00, 0x00, 0x00, 0x00, 0x06, 0x3F,
                              0xC3, 0x30, 0x0C, 0xC3, 0x00, 0x00, 0x00, 0x05, 0x7B, 0xEF, 0xBD, 0xB7, 0x03, 0x00, 0x00, 0x00,
                              0x06, 0xF3, 0x3C, 0x7B, 0x1E, 0xC3, 0x00, 0x00, 0x00, 0x07, 0xE3, 0xF1, 0xFA, 0xEF, 0xF3, 0xD9,
                              0x00, 0x00, 0x06, 0xF3, 0xEC, 0x31, 0xDE, 0x3C, 0x03, 0x00, 0x00, 0x06, 0xF3, 0xEC, 0x31, 0x0C,
                              0xC3, 0x00, 0x00, 0x00, 0x05, 0x1F, 0x73, 0x77, 0xC6, 0x07, 0x00, 0x00, 0x00, 0x03, 0xDF, 0xB6,
                              0xED, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xC9, 0x64, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
                              0xB7, 0x6D, 0xFB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x7A, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x03, 0x00, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x05, 0x00, 0x38, 0xEC, 0xB7, 0x07, 0x00, 0x00, 0x00, 0x05, 0x63, 0xBC, 0xBD,
                              0xF7, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x3E, 0x33, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x05, 0x18,
                              0xFB, 0xBD, 0xB7, 0x07, 0x00, 0x00, 0x00, 0x05, 0x00, 0xB8, 0xFD, 0x87, 0x07, 0x00, 0x00, 0x00,
                              0x03, 0xDE, 0xB7, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0xF8, 0xBD, 0x3D, 0x76, 0x00,
                              0x00, 0x00, 0x05, 0x63, 0xBC, 0xBD, 0xF7, 0x06, 0x00, 0x00, 0x00, 0x02, 0xF3, 0x3F, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x03, 0x86, 0x6D, 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x63, 0xEC,
                              0x77, 0xDE, 0x06, 0x00, 0x00, 0x00, 0x02, 0xFF, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08,
                              0x00, 0x00, 0x7F, 0xDB, 0xDB, 0xDB, 0xDB, 0x00, 0x05, 0x00, 0xBC, 0xBD, 0xF7, 0x06, 0x00, 0x00,
                              0x00, 0x05, 0x00, 0xB8, 0xBD, 0xB7, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00, 0xBC, 0xBD, 0xDF, 0x18,
                              0x00, 0x00, 0x00, 0x05, 0x00, 0xF8, 0xBD, 0x3D, 0xC6, 0x00, 0x00, 0x00, 0x04, 0x00, 0xFB, 0x33,
                              0x03, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x3E, 0xCF, 0x07, 0x00, 0x00, 0x00, 0x00, 0x03, 0xDB,
                              0xB7, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0xEC, 0xBD, 0xB7, 0x07, 0x00, 0x00, 0x00,
                              0x05, 0x00, 0xEC, 0xED, 0x1C, 0x01, 0x00, 0x00, 0x00, 0x07, 0x00, 0xC0, 0x78, 0xBD, 0xF6, 0xD9,
                              0x00, 0x00, 0x05, 0x00, 0xEC, 0xED, 0xF6, 0x06, 0x00, 0x00, 0x00, 0x05, 0x00, 0xEC, 0xBD, 0x3D,
                              0x76, 0x00, 0x00, 0x00, 0x05, 0x00, 0x7C, 0x66, 0xC6, 0x07, 0x00, 0x00, 0x00, 0x03, 0xDF, 0xB6,
                              0xED, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
                              0xB7, 0x6D, 0xFB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0xC0, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00};

// Static Buffer in data space for font 'font_micro'
const uint8 font_micro[] = { 0x44, 0x06, 0x00, 0x01, 0x17, 0x00, 0x00, 0x03, 0x2F, 0x00, 0x00, 0x03, 0x7D, 0x5F, 0x00, 0x03,
                              0x5A, 0x2D, 0x00, 0x03, 0xA5, 0x52, 0x00, 0x03, 0xAA, 0x6A, 0x00, 0x01, 0x03, 0x00, 0x00, 0x02,
                              0x56, 0x02, 0x00, 0x02, 0xA9, 0x01, 0x00, 0x03, 0xA8, 0x0A, 0x00, 0x03, 0xD0, 0x05, 0x00, 0x02,
                              0x00, 0x0E, 0x00, 0x03, 0xC0, 0x01, 0x00, 0x01, 0x10, 0x00, 0x00, 0x03, 0x94, 0x94, 0x00, 0x03,
                              0x6F, 0x7B, 0x00, 0x03, 0x9A, 0x74, 0x00, 0x03, 0xA3, 0x72, 0x00, 0x03, 0xA7, 0x79, 0x00, 0x03,
                              0xED, 0x49, 0x00, 0x03, 0xCF, 0x79, 0x00, 0x03, 0xCF, 0x7B, 0x00, 0x03, 0xA7, 0x24, 0x00, 0x03,
                              0xEF, 0x7B, 0x00, 0x03, 0xEF, 0x79, 0x00, 0x01, 0x0A, 0x00, 0x00, 0x02, 0x88, 0x03, 0x00, 0x03,
                              0x54, 0x44, 0x00, 0x03, 0x38, 0x0E, 0x00, 0x03, 0x11, 0x15, 0x00, 0x03, 0xA7, 0x20, 0x00, 0x03,
                              0x6F, 0x73, 0x00, 0x03, 0xEF, 0x5B, 0x00, 0x03, 0xEF, 0x7A, 0x00, 0x03, 0x4F, 0x72, 0x00, 0x03,
                              0x6B, 0x3B, 0x00, 0x03, 0xCF, 0x72, 0x00, 0x03, 0xCF, 0x12, 0x00, 0x03, 0x4F, 0x7B, 0x00, 0x03,
                              0xED, 0x5B, 0x00, 0x03, 0x97, 0x74, 0x00, 0x03, 0x24, 0x7B, 0x00, 0x03, 0xED, 0x5A, 0x00, 0x03,
                              0x49, 0x72, 0x00, 0x03, 0x7D, 0x5B, 0x00, 0x03, 0xED, 0x5F, 0x00, 0x03, 0x6F, 0x7B, 0x00, 0x03,
                              0xEF, 0x13, 0x00, 0x03, 0x6F, 0x7B, 0x02, 0x03, 0xEB, 0x5A, 0x00, 0x03, 0x8E, 0x39, 0x00, 0x03,
                              0x97, 0x24, 0x00, 0x03, 0x6D, 0x7B, 0x00, 0x03, 0x6D, 0x2B, 0x00, 0x03, 0x6D, 0x7F, 0x00, 0x03,
                              0xAD, 0x5A, 0x00, 0x03, 0xAD, 0x24, 0x00, 0x03, 0xA7, 0x72, 0x00, 0x02, 0x57, 0x03, 0x00, 0x03,
                              0x91, 0x44, 0x00, 0x02, 0xAB, 0x03, 0x00, 0x03, 0x2A, 0x00, 0x00, 0x03, 0x00, 0x80, 0x03, 0x01,
                              0x03, 0x00, 0x00, 0x03, 0xD6, 0x64, 0x00, 0x01, 0x3F, 0x00, 0x00, 0x03, 0x93, 0x35, 0x00, 0x03,
                              0x33, 0x00, 0x00};


////////////////////////////////////////////////////
//
//   Bitmaps
//
////////////////////////////////////////////////////


// Graphic data for start_screen_1.1.pbm 
// bitmap size: 118 x 48, data size = 708
const uint8 uibm_start_screen[] = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0xe0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x38, 0x00, 0x00, 0x00, 0x60, 0x00, 0x44, 0x00, 0x00,
            0x00, 0x80, 0x00, 0x44, 0x00, 0x00, 0x00, 0xf0, 0x01, 0x44, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x28,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0xc0,
            0x01, 0x54, 0x00, 0x00, 0x00, 0x30, 0x01, 0x54, 0x00, 0x00, 0x00, 0x98, 0x00, 0x54, 0x00, 0x00,
            0x00, 0x48, 0x00, 0x78, 0x00, 0x00, 0x00, 0xf0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x7c,
            0x00, 0x00, 0x00, 0x00, 0x08, 0x04, 0x00, 0x00, 0x00, 0xc0, 0x11, 0x04, 0x38, 0x00, 0x00, 0x20,
            0x11, 0x7c, 0xc0, 0x01, 0x00, 0x10, 0x11, 0x04, 0x00, 0x06, 0x00, 0x88, 0x18, 0x04, 0x00, 0x06,
            0x00, 0xc8, 0x07, 0x78, 0xc0, 0x01, 0x00, 0x38, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x08, 0x00,
            0x00, 0x00, 0x00, 0xe0, 0x11, 0x00, 0x00, 0x00, 0x00, 0x18, 0x11, 0x7c, 0x00, 0x00, 0x00, 0x00,
            0x11, 0x04, 0x10, 0x00, 0x00, 0x80, 0x0c, 0x00, 0x10, 0x00, 0x00, 0xf8, 0x03, 0x38, 0xf8, 0x07,
            0x00, 0x00, 0x00, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x54, 0x00, 0x04, 0x00, 0xe0, 0x01, 0x54,
            0x00, 0x00, 0x80, 0x19, 0x02, 0x18, 0x00, 0x00, 0x00, 0x08, 0x03, 0x00, 0x10, 0x06, 0x00, 0x88,
            0x01, 0x7c, 0x08, 0x05, 0x00, 0x48, 0x01, 0x04, 0x88, 0x04, 0x00, 0x28, 0x01, 0x04, 0x48, 0x04,
            0x00, 0x18, 0x01, 0x7c, 0x30, 0x04, 0x00, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
            0x00, 0x00, 0x00, 0xf0, 0x00, 0x78, 0x00, 0x00, 0x00, 0x28, 0x01, 0x00, 0x00, 0x00, 0x00, 0x34,
            0x01, 0x38, 0x00, 0x00, 0x00, 0x8c, 0x00, 0x44, 0x00, 0x00, 0x00, 0x40, 0x00, 0x44, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x44, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x38, 0x00, 0x00, 0x00, 0x28, 0x01, 0x00,
            0x00, 0x00, 0x00, 0x34, 0x01, 0x3e, 0x00, 0x00, 0x00, 0x8c, 0x00, 0x44, 0x00, 0x00, 0x00, 0x40,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x80, 0x05, 0x00, 0x54, 0x00, 0x00,
            0x00, 0x07, 0x00, 0x54, 0x00, 0x00, 0x00, 0x00, 0x01, 0x54, 0x00, 0x00, 0x00, 0x18, 0x01, 0x18,
            0x00, 0x00, 0x00, 0x28, 0x01, 0x00, 0x00, 0x00, 0x00, 0x64, 0x01, 0x00, 0x00, 0x00, 0x00, 0xc4,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x3f, 0xe0,
            0x0f, 0x00, 0x00, 0xfc, 0x0f, 0xf8, 0x7f, 0x00, 0x00, 0xff, 0x00, 0xfe, 0xff, 0x01, 0x00, 0x1f,
            0x80, 0x1f, 0xe0, 0x03, 0x80, 0x0f, 0xc0, 0x03, 0x80, 0x07, 0x80, 0x07, 0xe0, 0x01, 0x00, 0x0e,
            0xc0, 0x03, 0x70, 0x40, 0x08, 0x1c, 0xc0, 0x03, 0x30, 0x18, 0x30, 0x38, 0xc0, 0x01, 0x3c, 0x0c,
            0xc0, 0x30, 0xc0, 0xc1, 0x1f, 0x06, 0x80, 0x71, 0xc0, 0x00, 0x1f, 0x03, 0x80, 0x61, 0xc0, 0x00,
            0x0c, 0x03, 0x00, 0xe3, 0xc0, 0x00, 0x8c, 0x01, 0x00, 0xe3, 0xc0, 0x00, 0x8c, 0x01, 0x00, 0xc3,
            0xc0, 0x00, 0x8f, 0x01, 0x00, 0xc3, 0x40, 0x00, 0x8f, 0x01, 0x00, 0xc3, 0x40, 0x80, 0x8f, 0x01,
            0x00, 0xe3, 0x40, 0x80, 0x8e, 0x03, 0x80, 0x63, 0x40, 0xc0, 0x0e, 0x07, 0xc0, 0x63, 0x00, 0x40,
            0x1e, 0xff, 0xff, 0x21, 0x00, 0x43, 0x1e, 0xfe, 0xff, 0x30, 0xc0, 0x60, 0x3e, 0xfc, 0x7f, 0x18,
            0x30, 0x60, 0x6c, 0xf0, 0x3f, 0x08, 0x18, 0x60, 0xdc, 0xc0, 0x07, 0x0c, 0x0c, 0x60, 0xf8, 0x01,
            0x00, 0x03, 0x06, 0x60, 0xf0, 0x07, 0x80, 0x01, 0x03, 0x60, 0xe0, 0x1f, 0x60, 0x00, 0x03, 0xe0,
            0xc0, 0x1f, 0x80, 0x01, 0x01, 0xc0, 0x00, 0xff, 0x7f, 0x00, 0x01, 0xc0, 0x01, 0xfc, 0x1f, 0x00,
            0x01, 0x80, 0x03, 0x00, 0x00, 0x00, 0xf1, 0x81, 0x07, 0x00, 0x40, 0x00, 0xff, 0x03, 0x0f, 0x00,
            0x60, 0x00, 0xbf, 0x07, 0x1e, 0x00, 0x30, 0x00, 0x0f, 0x1e, 0x7c, 0x00, 0x18, 0x00, 0x07, 0x3c,
            0xf8, 0x01, 0x0f, 0x00, 0x07, 0xf0, 0xe1, 0xff, 0x03, 0x00, 0xff, 0x81, 0x8f, 0xff, 0x81, 0x00,
            0xfe, 0x03, 0x00, 0x00, 0x80, 0x00, 0x00, 0x06, 0x00, 0x00, 0x80, 0x00, 0x00, 0x06, 0x00, 0x00,
            0x80, 0x00, 0x20, 0x04, 0x00, 0x00, 0x80, 0x00, 0xc0, 0x04, 0x00, 0x00, 0x80, 0x00, 0x80, 0x08,
            0x00, 0x00, 0xc0, 0x00, 0x00, 0x01, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x03, 0x00, 0xf8, 0xff, 0x00,
            0x00, 0x02, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x06, 0x00, 0x00, 0xe0, 0x00, 0x00, 0x06, 0x00, 0x00,
            0x60, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x78, 0x00, 0x00, 0xf8, 0xff, 0xff, 0x3f, 0x00, 0x00, 0xf0,
            0xff, 0xff, 0x1f, 0x00 };


// Graphic data for icon_lightning.pbm
// bitmap size: 13 x 13
const uint8 uibm_icon_lightning[] = {
            0x1c, 0x00, 0x2e, 0x00, 0x5d, 0x00, 0x6b, 0x00, 0x77, 0x1a, 0x4d, 0x1b, 0xa6, 0x06, 0x55, 0x02,
            0x2a, 0x00, 0x4b, 0x00, 0x22, 0x00, 0x3e, 0x00, 0x18, 0x00 };
// data size = 26


// Graphic data for icon_longexpo.pbm
// bitmap size: 13 x 13
const uint8 uibm_icon_longexpo[] = {
            0xfe, 0x0f, 0x0a, 0x0a, 0x0e, 0x0e, 0x4a, 0x0a, 0x2e, 0x0e, 0x2a, 0x0a, 0x4e, 0x0e, 0x8a, 0x0a,
            0x8e, 0x0e, 0x4a, 0x0a, 0x24, 0x04, 0x22, 0x08, 0x04, 0x05 };
// data size = 26

// Graphic data for icon_sequential.pbm
// bitmap size: 13 x 13
const uint8 uibm_icon_sequential[] = {
            0x00, 0x00, 0xfe, 0x03, 0x04, 0x04, 0xf8, 0x0f, 0xaa, 0x02, 0x04, 0x04, 0xf8, 0x0f, 0xaa, 0x02,
            0x04, 0x04, 0xf8, 0x0f, 0xaa, 0x02, 0x04, 0x04, 0xf8, 0x0f };
// data size = 26

// Graphic data for icon_timer.pbm
// bitmap size: 13 x 13
const uint8 uibm_icon_timer[] = {
            0xe0, 0x00, 0x10, 0x03, 0x04, 0x04, 0x18, 0x08, 0x38, 0x08, 0xf2, 0x10, 0xe7, 0x10, 0xe7, 0x10,
            0x02, 0x08, 0x02, 0x08, 0x04, 0x04, 0x18, 0x03, 0xe0, 0x00 };
// data size = 26

// Graphic data for icon_sound.pbm
// bitmap size: 13 x 13
const uint8 uibm_icon_sound[] = {
            0x88, 0x00, 0x44, 0x00, 0x88, 0x00, 0x44, 0x00, 0x00, 0x00, 0xfe, 0x01, 0x55, 0x13, 0x55, 0x1f,
            0x55, 0x1f, 0x55, 0x1f, 0x55, 0x13, 0xfe, 0x01, 0x00, 0x00 };
// data size = 26


// Graphic data for icon_pretrigger.pbm
// bitmap size: 13 x 13
const uint8 uibm_icon_pretrigger[] = {
            0x00, 0x00, 0x00, 0x00, 0x02, 0x08, 0x1d, 0x16, 0x25, 0x19, 0xc5, 0x18, 0x05, 0x18, 0xc5, 0x18,
            0x25, 0x19, 0x1d, 0x16, 0x02, 0x08, 0x00, 0x00, 0x00, 0x00 };
// data size = 26

// Graphic data for icon_started.pbm
// bitmap size: 13 x 13
const uint8 uibm_icon_started[] = {
            0xff, 0x1f, 0x03, 0x18, 0xfd, 0x17, 0xfd, 0x17, 0xfd, 0x17, 0x05, 0x14, 0x0d, 0x16, 0x1d, 0x17,
            0xbd, 0x17, 0xfd, 0x17, 0xfd, 0x17, 0x03, 0x18, 0xff, 0x1f };
// data size = 26

// Graphic data for icon_mlockup.pbm 
// bitmap size: 13 x 13
const uint8 uibm_icon_mlockup[] = { 
            0xfe, 0x0f, 0x01, 0x10, 0xf9, 0x11, 0x11, 0x10, 0x21, 0x10, 0x11, 0x10, 0xf9, 0x11, 0x01, 0x10,
            0x31, 0x10, 0xf9, 0x11, 0x31, 0x10, 0x01, 0x10, 0xfe, 0x0f };
// data size = 26



// Graphic data for icon_cb_selected.pbm
// bitmap size: 8 x 8
const uint8 uibm_grf_selected[] = {
            0x00, 0x10, 0x60, 0x30, 0x08, 0x04, 0x02, 0x00 };
// data size = 8

// Graphic data for grf_pointer.pbm
// bitmap size: 5 x 8
const uint8 uibm_grf_pointer[] = {
            0x70, 0x3c, 0xff, 0x3c, 0x70 };
// data size = 5

// Graphic data for icon_cablerelease.pbm 
// bitmap size: 13 x 13
const uint8 uibm_grf_cablerelease[] = { 
            0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x09, 0x80, 0x08, 0x70, 0x08, 0x78, 0x04, 0x7c, 0x02,
            0x32, 0x02, 0x12, 0x02, 0x0c, 0x0a, 0x00, 0x04, 0x00, 0x00 };
// data size = 26


// Graphic data for icon_trigger_light.pbm 
// bitmap size: 13 x 13
const uint8 uibm_icon_trigger_light[] = { 
            0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0xc0, 0x00, 0xe0, 0x00, 0xf0, 0x08, 0xd8, 0x06, 0xcc, 0x03,
            0xc2, 0x01, 0xe0, 0x00, 0x60, 0x00, 0x20, 0x00, 0x00, 0x00 };
// data size = 26

// Graphic data for icon_trigger_shade.pbm 
// bitmap size: 13 x 13
const uint8 uibm_icon_trigger_shade[] = { 
            0x00, 0x00, 0x00, 0x18, 0x00, 0x18, 0xb8, 0x18, 0xcc, 0x18, 0x0e, 0x11, 0x1e, 0x12, 0xbe, 0x1e,
            0x6e, 0x13, 0xdc, 0x11, 0xf8, 0x10, 0x00, 0x00, 0x00, 0x00 };
// data size = 26

// Graphic data for icon_bright_light.pbm 
// bitmap size: 13 x 13
const uint8 uibm_icon_bright_light[] = { 
            0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x48, 0x02, 0xf0, 0x01, 0xf0, 0x01, 0xfc, 0x07, 0xf0, 0x01,
            0xf0, 0x01, 0x48, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00 };
// data size = 26




const uint8* bitmap_list[] = { uibm_start_screen, uibm_icon_lightning, uibm_icon_longexpo,      uibm_icon_sequential,
                               uibm_icon_timer,   uibm_icon_sound,     uibm_icon_pretrigger,    uibm_icon_started,
                               uibm_icon_mlockup, uibm_grf_selected,   uibm_grf_pointer,        uibm_grf_cablerelease,
                               uibm_icon_trigger_light, uibm_icon_trigger_shade, uibm_icon_bright_light };
const uint16 bitmap_size[] = { 118,48,   13,13,   13,13,   13,13,
                               13,13,    13,13,   13,13,   13,13,
                               13,13,    8,8,     5,8,     13,13,
                               13,13,    13,13,   13,13 };
const uint16 bitmap_datasz[] = { 708, 26, 26, 26,
                                 26, 26, 26, 26,
                                 26, 8, 5, 26,
                                 26, 26, 26 };








static void internal_display_number( int value, int chnr, char fillchar, uint32 radix, bool show_plus_sign )
{
    char str[9];
    char str1[9];
    int i;

    if ( chnr >= 8 )
        return;

    if ( show_plus_sign && (value>0) )
    {
        str1[0]='+';
        itoa( value, str1+1, radix );
    }
    else
        itoa( value, str1, radix );


    if ( fillchar != 0x00 )
    {
        for ( i=0; i<chnr; i++ )
            str[i]  = fillchar;
        str[i]      = 0x00;

        i = strlen( str1 );
        strncpy( (str + chnr - i), str1, i );
        Gtext_PutText( str );
    }
    else
        Gtext_PutText( str1 );
}






void uibm_put_bitmap( int x, int y, int bmp )
{

    int w;
    int h;
    w = bitmap_size[ bmp*2 ];
    h = bitmap_size[ bmp*2 + 1 ];

    Bitmap_StartDrawing( x, y, w, h, false, gpixformat_1bit );
    Bitmap_DrawStream( bitmap_list[bmp], bitmap_datasz[bmp] );
    Bitmap_StopDrawing();
}


void uigrf_draw_battery( int x, int y, int fullness )
{
    int pixsize;

    Graphic_SetColor( 1 );
    Graphic_Rectangle( x+2, y, x+5, y+1 );
    Graphic_FillRectangle( x, y+2, x+7, y+13, 0 );
    // pixel space inside the indicator zone: 10
    pixsize = fullness / 10;
    if ( pixsize )
    {
        Graphic_FillRectangle( x+1, y+12-pixsize, x+6, y+12, 1 );
    }
}


void uigrf_draw_scale( int y, int thold, int maxptr, int minptr, bool centered, bool redraw )
{
    int w, x;

    if ( redraw )
    {
        Graphic_SetColor( 1 );
        Graphic_FillRectangle( 0, y+4, 126, y+9, 1 );
        for ( w=0; w < 5; w++ )
        {
            x = ((( 1575 ) * w) / 100);
            if ( w & 0x01 )
            {
                Graphic_Line( 63+x, y+2, 63+x, y+3 );
                Graphic_Line( 63-x, y+2, 63-x, y+3 );
            }
            else
            {
                Graphic_Line( 63+x, y, 63+x, y+3 );
                Graphic_Line( 63-x, y, 63-x, y+3 );
            }
        }

        w = 126 * thold / 100;
        if ( w == 0 )
            w = 1;

        Graphic_SetColor( 0 );
        if ( centered )
            Graphic_FillRectangle( 63 - (w >> 1), y+5, 63 + (w >> 1), y+8, 0 );
        else
            Graphic_FillRectangle( 1, y+5, 1 + w, y+8, 0 );
    }

    Graphic_SetColor( 0 );
    Graphic_FillRectangle( 0, y+10, 127, y+17, 0 );
    if ( centered )
    {
        uibm_put_bitmap( 61 + ((maxptr * 63) / 100), y+10, BMP_GRF_POINTER );
        if ( minptr != 0xff )
            uibm_put_bitmap( 61 + ((minptr * 63) / 100), y+10, BMP_GRF_POINTER );
    }
    else
    {
        uibm_put_bitmap( -2 + ((maxptr * 126) / 100), y+10, BMP_GRF_POINTER );
        if ( minptr != 0xff )
            uibm_put_bitmap( -2 + ((minptr * 126) / 100), y+10, BMP_GRF_POINTER );
    }

}






void grf_setup_font( enum Etextstyle style, int color, int backgnd )
{
    bool mono;

    if ( style & uitxt_MONO )
        mono = true;
    else
        mono = false;

    style = (enum Etextstyle)(style & ~uitxt_MONO );

    if ( style == uitxt_small )
        Gtext_SetFontAndColors( font_small, mono, color, backgnd );
    else if ( style == uitxt_smallbold )
        Gtext_SetFontAndColors( font_small_bold, mono, color, backgnd );
    else if ( style == uitxt_micro )
        Gtext_SetFontAndColors( font_micro, mono, color, backgnd );
    else if ( style == uitxt_large_num )
        Gtext_SetFontAndColors( font_large_num, mono, color, backgnd );
    else if ( style == uitxt_system )
        Gtext_SetFontAndColors( NULL, mono, color, backgnd );

}



void uigrf_text( int x, int y, enum Etextstyle style, char *text )
{
    grf_setup_font( style, 1, 0 );
    Gtext_SetCoordinates( x, y );
    Gtext_PutText( text );
}


void uigrf_text_mono( int x, int y, enum Etextstyle style, char *text, bool specialdot )
{
    grf_setup_font( (enum Etextstyle)(style | uitxt_MONO), 1, -1 );
    Gtext_SetCoordinates( x, y );
    if (specialdot == false)
        Gtext_PutText( text );
    else
    {
        int i;
        int len;

        len = strlen(text);
        for (i=0; i<len; i++)
        {
            if ( text[i] != '.' )
                grf_setup_font( (enum Etextstyle)(style | uitxt_MONO), 1, -1 );
            else
                grf_setup_font( style, 1, -1 );
            Gtext_PutChar( text[i] );
        }
    }
}

int poz_2_increment( int poz )
{
    int i;
    int val = 1;
    for (i=0; i<poz; i++)
    {
        val = val * 10;
    }
    return val;
}

void uigrf_putnr( int x, int y, enum Etextstyle style, int nr, int digits, char fill, bool show_plus_sign )
{
    grf_setup_font( style, 1, 0 );
    Gtext_SetCoordinates( x, y );
    internal_display_number( nr, digits, fill, 10, show_plus_sign );
}

void uigrf_puthex( int x, int y, enum Etextstyle style, int nr, int digits, char fill )
{
    grf_setup_font( style, 1, 0 );
    Gtext_SetCoordinates( x, y );
    Gtext_PutText( "0x" );
    internal_display_number( nr, digits, fill, 16, false );
}

// nr of digits - max. digits to display
// fp - how many digits are after the decimal point
void uigrf_putfixpoint( int x, int y, enum Etextstyle style, int nr, int digits, int fp, char fill, bool show_plus_sign )
{
    int len;
    int div = 1;
    grf_setup_font( style, 1, 0 );
    Gtext_SetCoordinates( x, y );
    len = digits - fp;  // length of integer part or whole part
    div = poz_2_increment( fp );    // convert the value
    internal_display_number( nr/div, len, fill, 10, show_plus_sign );
    Gtext_PutChar('.');
    nr = nr % div;
    internal_display_number( nr, fp, '0', 10, false );
}


void uigrf_puttime( int x, int y, enum Etextstyle style, int color, timestruct time, bool minute_only, bool large_border )
{
    int height;
    int width;
    int w_nr;
    int w_spacer;

    grf_setup_font( (enum Etextstyle)style, color, -1 );
    height = Gtext_GetCharacterHeight();

    grf_setup_font( (enum Etextstyle)(style), color, -1 );
    w_spacer = Gtext_GetCharacterWidth(':') + 1;
    w_nr = Gtext_GetCharacterWidth('&') + 1;
    if ( minute_only )
        width = 4*w_nr+w_spacer;
    else
        width = 6*w_nr+2*w_spacer;

    // available formats:   00:00:00

    // make the background
    Graphic_SetColor( 1-color );
    if ( large_border )
        Graphic_FillRectangle( x-1, y-1, x+width+1, y+height+1, 1-color );
    else
        Graphic_FillRectangle( x, y, x+width-2, y+height-2, 1-color );
    Gtext_SetCoordinates( x, y );

    // print the time in hh:mm:ss
    grf_setup_font( (enum Etextstyle)(style | uitxt_MONO), color, -1 );
    if ( minute_only == false )
    {
        internal_display_number( time.hour, 2, '0', 10, false );
        grf_setup_font( (enum Etextstyle)(style), color, -1 );
        Gtext_PutChar( ':' );
        grf_setup_font( (enum Etextstyle)(style | uitxt_MONO), color, -1 );
    }
    internal_display_number( time.minute, 2, '0', 10, false );
    grf_setup_font( (enum Etextstyle)(style), color, -1 );
    Gtext_PutChar( ':' );
    grf_setup_font( (enum Etextstyle)(style | uitxt_MONO), color, -1 );
    internal_display_number( time.second, 2, '0', 10, false );

}







