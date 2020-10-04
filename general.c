#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <cpu_speed.h>
#include <macros.h>
#include <graphics.h>
#include <lcd.h>
#include "lcd_model.h"
#include "uart.h"
#include "cab202_adc.h"

/**
 * draw pixels at specified place
 * Parameters:
 *      left, top -left and top coordinate of image to be drawn
 *      width, height -width and height of image
 *      bitmap[] -pixel image of object to be drawn
 *      space_is_transparent -true if you want to treat space as space
 */
void draw_pixels(int left, int top, int width, int height, char bitmap[], bool space_is_transparent){
    
    for (int j=0; j<height; j++){
        for(int i=0; i<width; i++){
            if (bitmap[i + j * width] != ' '){
                draw_pixel(left + i, top + j, FG_COLOUR);
            } else if (space_is_transparent == false){
                draw_pixel(left + i, top + j, FG_COLOUR);
            } // end of if - else if
        } // end of for(i)
    } // end of for(j)
} // end of draw_pixels


/**
 * Return true if 2 objects specified by parameters collides each other
 * Parameters:
 *      x0, y0, w0, h0 -first image's x and y coordinate (top left) and width and height
 *      pixels0[] -first image's pixel image
 *      x1, y1, w1, h1 -second image's x and y coordinate (top left) and width and height
 *      pixels1[] -second image's pixel image
 */
bool pixel_collision(int x0, int y0, int w0, int h0, char pixels0[], int x1, int y1, int w1, int h1, char pixels1[]){
    
    for (int i=x0; i<x0+w0; i++){
        for(int j=y0; j<y0+h0; j++){
            if (i >= x1 &&
                i < x1 + w1 &&
                j >= y1 &&
                j < y1 + h1 &&
                pixels0[(i - x0) + (j - y0)*w0] != ' ' &&
                i >= x0 &&
                i < x0 + w0 &&
                j >= y0 &&
                j < y0 + h0 &&
                pixels1[(i - x1) + (j - y1)*w1] != ' '){
                return true;
            } // end of if
        } // end of for(i)
    } // end of for(j)
    
    return false;
}


void draw_formatted(int x, int y, char * buffer, int buffer_size, const char * format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, buffer_size, format, args);
    draw_string(x, y, buffer, FG_COLOUR);
}
