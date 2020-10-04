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
void draw_pixels(int left, int top, int width, int height, char bitmap[], bool space_is_transparent);

/**
 * Return true if 2 objects specified by parameters collides each other
 * Parameters:
 *      x0, y0, w0, h0 -first image's x and y coordinate (top left) and width and height
 *      pixels0[] -first image's pixel image
 *      x1, y1, w1, h1 -second image's x and y coordinate (top left) and width and height
 *      pixels1[] -second image's pixel image
 */
bool pixel_collision(int x0, int y0, int w0, int h0, char pixels0[], int x1, int y1, int w1, int h1, char pixels1[]);

void draw_formatted(int x, int y, char * buffer, int buffer_size, const char * format, ...);
