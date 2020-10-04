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
#include "cab202_adc.h"

#include "general.h"

#define MAX_PLASMA 30
#define MIN_PLASMA_INTERVAL 0.2;
#define MAX_TURRET_ANGLE 60;

#define ADC_MAX (1023)

const double pot_left_per_angle = 120.0 / 1023.0;
const int plasma_size = 2;
const int fighter_width = 14;
const int fighter_height = 3;
const int turret_width = 4;
const int turret_height = 1;

int fighter_x;
int fd = 0; // -1, 0, 1
const int fighter_y = 45;

double plasma_fire_x;

double px[MAX_PLASMA];
double py[MAX_PLASMA];
double pdx[MAX_PLASMA];
double pdy[MAX_PLASMA];

int turret_angle = 90; // 30 ~ 150
int angle_display = 0;

int current_plasma = 0;

double last_fire = 0.0;

char * turret =
"tttt"
;

char * fighter =
"ffffffffffffff"
" ffffffffffff "
"  ffffffffff  "
;

char * plasma =
"pp"
"pp"
;

void draw_fighter(void){
    draw_pixels(fighter_x, fighter_y, fighter_width, fighter_height, fighter, true);
}

void draw_turret(void){
    double turret_x[4];
    double turret_y[4];
    double base_x = fighter_x + 5.0;
    double base_y = fighter_y - 1.0;
    turret_x[0] = base_x;
    turret_y[0] = base_y;
    for (int a = 1; a < 4; a++){
        turret_x[a] = turret_x[a - 1] + cos(turret_angle * M_PI / 180.0);
        turret_y[a] = turret_y[a - 1] - sin(turret_angle * M_PI / 180.0);
    }
    plasma_fire_x = turret_x[3] + 1;
    for(int t = 0; t < 4; t++){
        draw_pixels(turret_x[t], turret_y[t], turret_width, turret_height, turret, true);
    }
}

void update_plasma_array(int index){
    for (int p = index; p < current_plasma; p++){
        px[p] = px[p + 1];
        py[p] = py[p + 1];
        pdx[p] = pdx[p + 1];
        pdy[p] = pdy[p + 1];
    }
    current_plasma--;
}

void set_turret_angle(void){
    double left_adc = adc_read(0);
    double temp_angle = -(left_adc - 1023) * pot_left_per_angle;
    turret_angle = (int)(temp_angle + 30.0);
    if ((int)temp_angle >= 60){
        angle_display = -(int)temp_angle + 60;
    } else {
        angle_display = -(int)temp_angle + 60;
    }
    
}



void fire_plasma(double time){
    double x = plasma_fire_x;
    double y = fighter_y - 4;
    
    if (current_plasma < MAX_PLASMA && time - last_fire > 0.2){
        px[current_plasma] = x;
        py[current_plasma] = y;
        pdx[current_plasma] = cos(turret_angle * M_PI / 180);
        pdy[current_plasma] = sin(turret_angle * M_PI / 180);
        current_plasma++;
        last_fire = time;
    }
}

void move_plasma(void){
    for (int p = 0; p < current_plasma; p++){
        draw_pixels(px[p], py[p], plasma_size, plasma_size, plasma, true);
        if(px[p] + pdx[p] > LCD_X ||
           px[p] + pdx[p] < 0 ||
           py[p] - pdy[p] < 0){
            update_plasma_array(p);
        } else {
            px[p] += pdx[p];
            py[p] -= pdy[p];
        }
    }
}

int get_current_plasma(){
    return current_plasma;
}

int get_px(int index){
    return px[index];
}

int get_py(int index){
    return py[index];
}

int get_plasma_size(){
    return plasma_size;
}

char *get_plasma(){
    return plasma;
}

int get_turret_angle(){
    return angle_display;
}

void set_current_plasma(int num){
    current_plasma = num;
}

void overwrite_turret_angle(int angle){
    if (angle <= 60 && angle >= -60){
        turret_angle = 180 - (angle + 90);
        angle_display = angle;
    }
}

int get_fighter_x(){
    return fighter_x;
}

void set_fighter_x(int x){
    if (!(x < 0 || x > LCD_X - fighter_width)){
        fighter_x = x;
    }
}

int get_fighter_size(char wh){
    if (wh == 'w'){
        return fighter_width;
    } else {
        return fighter_height;
    }
}

void change_fighter_direction(char dir){
    if (dir == 'r'){
        if(fd == 0){
            fd = 1;
        } else if (fd == -1){
            fd = 0;
        }
    } else {
        if (fd == 0){
            fd = -1;
        } else if (fd == 1) {
            fd = 0;
        }
    }
}

void move_fighter(){
    if (fighter_x + fd >  LCD_X - fighter_width){
        fd = 0;
    } else if (fighter_x + fd < 0){
        fd = 0;
    }
    fighter_x += fd;

}

void setup_fighter(){
    fighter_x = (LCD_X - fighter_width) / 2;
}

void set_last_fire(double last){
    last_fire = last;
}

