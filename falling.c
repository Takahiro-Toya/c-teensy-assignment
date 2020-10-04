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

#define ASTEROID_SIZE 7
#define BOULDER_SIZE 5
#define FRAGMENT_SIZE 3

#define MAX_ASTEROID 3
#define MAX_BOULDER 6
#define MAX_FRAGMENT 12

const int asteroid_score = 1;
const int boulder_score = 2;
const int fragment_score = 4;

double ax[MAX_ASTEROID];
double ay[MAX_ASTEROID];

double bx[MAX_BOULDER];
double by[MAX_BOULDER];
double bdx[MAX_BOULDER];
double bdy[MAX_BOULDER];

double fx[MAX_FRAGMENT];
double fy[MAX_FRAGMENT];
double fdx[MAX_FRAGMENT];
double fdy[MAX_FRAGMENT];

int current_asteroid = 0;
int current_boulder = 0;
int current_fragment = 0;

char * asteroid =
" aaaaa "
"aaaaaaa"
"aaaaaaa"
"aaaaaaa"
" aaaaa "
"  aaa  "
"   a   "
;

char * boulder =
" bbb "
"bbbbb"
"bbbbb"
" bbb "
"  b  "
;

char * fragment =
" f "
"fff"
" f "
;

void update_asteroid_array(int index){
    for (int a = index; a < current_asteroid; a++){
        ax[a] = ax[a + 1];
        ay[a] = ay[a + 1];
    }
    current_asteroid--;
}

void update_boulder_array(int index){
    for (int b = index; b < current_boulder; b++){
        bx[b] = bx[b + 1];
        by[b] = by[b + 1];
        bdx[b] = bdx[b + 1];
        bdy[b] = bdy[b + 1];
    }
    current_boulder--;
}

void update_fragment_array(int index){
    for (int f = index; f < current_fragment; f++){
        fx[f] = fx[f + 1];
        fy[f] = fy[f + 1];
        fdx[f] = fdx[f + 1];
        fdy[f] = fdy[f + 1];
    }
    current_fragment--;
}



bool check_asteroid_overwrap(int x, int y){
    for (int a = 0; a < current_asteroid; a++){
        if(pixel_collision(ax[a], ay[a], ASTEROID_SIZE, ASTEROID_SIZE, asteroid, x, y, ASTEROID_SIZE, ASTEROID_SIZE, asteroid)){
            return true;
        }
    }
    return false;
}

void flush_led(int num){
    if (num >= 2){
        for(int i = 0; i < 3; i++){
            SET_BIT(PORTB, 3);
            _delay_ms(500);
            CLEAR_BIT(PORTB, 3);
            _delay_ms(500);
        }
    } else {
        for(int i = 0; i < 3; i++){
            SET_BIT(PORTB, 2);
            _delay_ms(500);
            CLEAR_BIT(PORTB, 2);
            _delay_ms(500);
        }
    }
}

void spawn_asteroid(void){
    int left = 0;
    int right = LCD_X - ASTEROID_SIZE;
    double y = -ASTEROID_SIZE - 1;
    int more = 0;
    ax[0] = rand() % (right + 1 - left) + left;
    ay[0] = y;
    current_asteroid++;
    if(ax[0] > right / 2){
        more++;
    }
    
    while(current_asteroid < MAX_ASTEROID){
        double x = rand() % (right + 1 - left) + left;
        if (!check_asteroid_overwrap(x, y)){
            ax[current_asteroid] = x;
            ay[current_asteroid] = -ASTEROID_SIZE - 1;
            current_asteroid++;
            if (x > right / 2){
                more++;
            }
        }
    }
    flush_led(more);
}

void draw_asteroid(void){
    for (int a = 0; a < current_asteroid; a++){
        draw_pixels(ax[a], ay[a], ASTEROID_SIZE, ASTEROID_SIZE, asteroid, true);
    }
}

void draw_boulder(void){
    for (int b = 0; b < current_boulder; b++){
        draw_pixels(bx[b], by[b], BOULDER_SIZE, BOULDER_SIZE, boulder, true);
    }
}

void draw_fragment(void){
    for (int f = 0; f < current_fragment; f++){
        draw_pixels(fx[f], fy[f], FRAGMENT_SIZE, FRAGMENT_SIZE, fragment, true);
    }
}



int move_asteroid(double elapsed, int shield, double speed){
    int life = 0;
    for (int a = 0; a < current_asteroid; a++){
        if (ay[a] > shield - ASTEROID_SIZE - 1){
            life -= 1;
            update_asteroid_array(a);
        } else {
            ay[a] += speed;
        }
//        draw_pixels(ax[a], ay[a], ASTEROID_SIZE, ASTEROID_SIZE, asteroid, true);
    }
    return life;
}

int move_boulder(int shield, double speed){
    int life = 0;
    for (int b = 0; b < current_boulder; b++){
        if(bx[b] + speed * bdx[b] > LCD_X - BOULDER_SIZE ||
           bx[b] + speed * bdx[b] < 0){
            bdx[b] = - speed * bdx[b];
        } else if (by[b] + speed * bdy[b] >= shield - BOULDER_SIZE + 1){
            update_boulder_array(b);
            life -= 1;
        } else {
            bx[b] += speed * bdx[b];
            by[b] += speed * bdy[b];
        }
//        draw_pixels(bx[b], by[b], BOULDER_SIZE, BOULDER_SIZE, boulder, true);
    }
    return life;
}


int move_fragment(int shield, double speed){
    int life = 0;
    for (int f = 0; f < current_fragment; f++){
        if (fx[f] + speed * fdx[f] > LCD_X - FRAGMENT_SIZE||
            fx[f] + speed * fdx[f] < 0){
            fdx[f] = - speed * fdx[f];
        } else if (fy[f] + speed * fdy[f] >= shield - FRAGMENT_SIZE + 1){
            update_fragment_array(f);
            life -= 1;
        } else {
            fx[f] += speed * fdx[f];
            fy[f] += speed * fdy[f];
        }
//        draw_pixels(fx[f], fy[f], FRAGMENT_SIZE, FRAGMENT_SIZE, fragment, true);
    }
    return life;
}

// x -asteroid x
// y -asteroid y
void hit_asteroid(int index){
    int x1 = rand() % (90 + 1 - 60) + 60;
    int x2 = rand() % (120 + 1 - 90) + 90;

    if (ax[index] > LCD_X / 2){
        bx[current_boulder] = ax[index];
    } else {
        bx[current_boulder] = ax[index] + 5;
    }
    by[current_boulder] = ay[index];
    bdx[current_boulder] = cos(x1 * M_PI / 180);
    bdy[current_boulder] = sin(x1 * M_PI / 180);
    current_boulder++;
    
    if (ax[index] > LCD_X / 2){
        bx[current_boulder] = ax[index] - 5;
    } else {
        bx[current_boulder] = ax[index];
    }
    by[current_boulder] = ay[index];
    bdx[current_boulder] = cos(x2 * M_PI / 180);
    bdy[current_boulder] = sin(x2 * M_PI / 180);
    current_boulder++;
    
    update_asteroid_array(index);
    
}

void hit_boulder(int index){
    int x1 = rand() % (90 + 1 - 60) + 60;
    int x2 = rand() % (120 + 1 - 90) + 90;
    
    if(bx[index] > LCD_X / 2){
        fx[current_fragment] = bx[index];
    } else {
        fx[current_fragment] = bx[index] + 5;
    }

    fy[current_fragment] = by[index];
    fdx[current_fragment] = cos(x1 * M_PI / 180) + fdx[current_fragment];
    fdy[current_fragment] = sin(x1 * M_PI / 180) + fdx[current_fragment] ;
    current_fragment++;
    
    if(bx[index] > LCD_X / 2){
        fx[current_fragment] = bx[index] - 5;
    } else {
        fx[current_fragment] = bx[index];
    }
    fx[current_fragment] = bx[index];
    fy[current_fragment] = by[index];
    fdx[current_fragment] = cos(x2 * M_PI / 180) + fdx[current_fragment];
    fdy[current_fragment] = sin(x2 * M_PI / 180) + fdx[current_fragment];
    current_fragment++;
    
    update_boulder_array(index);
    
}

void hit_fragment(int index){
    update_fragment_array(index);
}

int get_current_asteroid(){
    return current_asteroid;
}

int get_current_boulder(){
    return current_boulder;
}

int get_current_fragment(){
    return current_fragment;
}

int get_asteroid_size(){
    return ASTEROID_SIZE;
}

int get_boulder_size(){
    return BOULDER_SIZE;
}

int get_fragment_size(){
    return FRAGMENT_SIZE;
}

char *get_asteroid(){
    return asteroid;
}

char *get_boulder(){
    return boulder;
}

char *get_fragment(){
    return fragment;
}

int get_axy(char xy, int index){
    if(xy == 'x'){
        return ax[index];
    } else {
        return ay[index];
    }
}

int get_bxy(char xy, int index){
    if(xy == 'x'){
        return bx[index];
    } else {
        return by[index];
    }
}

int get_fxy(char xy, int index){
    if(xy == 'x'){
        return fx[index];
    } else {
        return fy[index];
    }
}

void set_current_object(char abf, int num){
    if(abf == 'a'){
        current_asteroid = num;
    } else if(abf == 'b'){
        current_boulder = num;
    } else {
        current_fragment = num;
    }
}

int get_object_score(char abf){
    if(abf == 'a'){
        return asteroid_score;
    } else if(abf == 'b'){
        return boulder_score;
    } else {
        return fragment_score;
    }
}

void place_asteroid(int x, int y){
    if (x >= 0 && x < LCD_X - ASTEROID_SIZE &&
        y >= 0 && y < 39 - ASTEROID_SIZE &&
        current_asteroid < MAX_ASTEROID){
        ax[current_asteroid] = x;
        ay[current_asteroid] = y;
        current_asteroid++;
    }
}

void place_boulder(int x, int y){
    if (x >= 0 && x < LCD_X - BOULDER_SIZE &&
        y >= 0 && y < 39 - BOULDER_SIZE &&
        current_boulder < MAX_BOULDER){
        bx[current_boulder] = x;
        by[current_boulder] = y;
        bdx[current_boulder] = 0;
        bdy[current_boulder] = 1;
        current_boulder++;
    }
}

void place_fragment(int x, int y){
    if (x >= 0 && x < LCD_X - FRAGMENT_SIZE &&
        y >= 0 && y < 39 - FRAGMENT_SIZE &&
        current_fragment < MAX_FRAGMENT){
        fx[current_fragment] = x;
        fy[current_fragment] = y;
        fdx[current_fragment] = 0;
        fdy[current_fragment] = 1;
        current_fragment++;
    }
}

