#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL_scancode.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "TUM_Ball.h"
#include "TUM_Draw.h"
#include "TUM_Font.h"
#include "TUM_Event.h"
#include "TUM_Sound.h"
#include "TUM_Utils.h"
#include "TUM_FreeRTOS_Utils.h"
#include "TUM_Print.h"

#include "AsyncIO.h"


//Anfang
#define sizex 96
#define sizey 15
#define empty 0
#define enemy30 3
#define enemy20 2
#define enemy10 1
#define player 4
#define block 5
#define enemy_bullet 6
#define player_bullet 7

char world[sizey][sizex];
void EnemyShied()
{   int x=0;
    int y=0;
    //reset 
    
    for (y=0;y<=sizey;y++)
    {
        for(x=0;x<=sizex;x++)
        {
            world[y][x]= empty
        }
    }
    
    //player
    world[sizey-1][sizex/2]=player;
    
    //enemy30
    for (x=0,x<=16;x++)
        { 
         if(!x%2)
        world[0][x]=enemy30;
        }
    //enemy20
    for (y=1;y<3;y++)
    {
        for (x=0,x<=16;x++)
        {
            if(!x%2)
        world[0][x]=enemy20;
        }

    }
    //enemy20
    for (y=1;y<3;y++)
    {
        for (x=0,x<=16;x++)
        {
            if(!x%2)
        world[0][x]=enemy20;
     
        }
    }

    //enemy10
    for (y=3;y<5;y++)
    {
        for (x=0,x<=16;x++)
        {
            if(!x%2)
        world[0][x]=enemy10;
        }
    }

    

    //Blöckern
    for(y=sizey-3;y> sizey-6,y--)
    {      int abstand=6; 
        for(x=0;x<4,x++)
            world[y][x+3]=block;

        for(x=0;x<4,x++)
            world[y][x+3+abstand]=block;
        for(x=0;x<4,x++)
            world[y][x+3+abstand*2]=block;
        for(x=0;x<4,x++)
            world[y][x+3+abstand*3]=block;
}

void move_enemy()
{
    int speed=tumDrawGetloadedImageWide ( image_enemy_30)
    

}