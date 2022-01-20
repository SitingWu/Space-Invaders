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

#define mainGENERIC_PRIORITY (tskIDLE_PRIORITY)
#define mainGENERIC_STACK_SIZE ((unsigned short)2560)

#define STATE_QUEUE_LENGTH 1

#define STATE_COUNT 3

#define STATE_ONE 0
#define STATE_TWO 1
#define STATE_Three 2
#define NEXT_TASK_1 0
#define NEXT_TASK_2 1 
#define PREV_TASK   2

#define STARTING_STATE STATE_ONE

#define STATE_DEBOUNCE_DELAY 300

#define KEYCODE(CHAR) SDL_SCANCODE_##CHAR
#define CAVE_SIZE_X SCREEN_WIDTH / 2
#define CAVE_SIZE_Y SCREEN_HEIGHT / 2
#define CAVE_X CAVE_SIZE_X / 2
#define CAVE_Y CAVE_SIZE_Y / 2
#define BLOCK_THICKNESS 15


//Bildern define

#define LOGO_30 "30point.jpg"
#define LOGO_20 "20point.png"
#define LOGO_10 "10point.jpg"
#define LOGO_player "player"
#define LOGO_player_1 "player_1"
#define LOGO_mothership "mothership"


#define UDP_BUFFER_SIZE 2000
#define UDP_TEST_PORT_1 1234
#define UDP_TEST_PORT_2 4321
#define MSG_QUEUE_BUFFER_SIZE 1000
#define MSG_QUEUE_MAX_MSG_COUNT 10
#define TCP_BUFFER_SIZE 2000
#define TCP_TEST_PORT 2222
#define  xDelay 10
#ifdef TRACE_FUNCTIONS
#include "tracer.h"
#endif

static char *mq_one_name = "FreeRTOS_MQ_one_1";
static char *mq_two_name = "FreeRTOS_MQ_two_1";
aIO_handle_t mq_one = NULL;
aIO_handle_t mq_two = NULL;
aIO_handle_t udp_soc_one = NULL;
aIO_handle_t udp_soc_two = NULL;
aIO_handle_t tcp_soc = NULL;

const unsigned char next_state_signal_1 = NEXT_TASK_1;

const unsigned char prev_state_signal = PREV_TASK;

static TaskHandle_t StateMachine = NULL;
static TaskHandle_t BufferSwap = NULL;
static TaskHandle_t DemoTask1 = NULL;
static TaskHandle_t DemoTask2 = NULL;
static TaskHandle_t DemoTask3 = NULL;
static TaskHandle_t DemoTask4 = NULL;
static TaskHandle_t UDPDemoTask = NULL;
static TaskHandle_t TCPDemoTask = NULL;
static TaskHandle_t MQDemoTask = NULL;
static TaskHandle_t DemoSendTask = NULL;

static QueueHandle_t StateQueue = NULL;
static SemaphoreHandle_t DrawSignal = NULL;
static SemaphoreHandle_t ScreenLock = NULL;


static image_handle_t logo_image_30 = NULL;
static image_handle_t logo_image_20 = NULL;
static image_handle_t logo_image_10 = NULL;
static image_handle_t logo_image_player = NULL;
static image_handle_t logo_image_player_1 = NULL;
static image_handle_t logo_image_mothership = NULL;


// globale variable
int score_1=0;
int high_score=0;
int score_2=0;
int speed=0;
int button1=0;
int life = 3;

TaskHandle_t Text_Task_1=NULL;
TaskHandle_t Text_Task_2=NULL;

TaskHandle_t Text_Task_3=NULL;
TaskHandle_t Text_Task_4=NULL;
TaskHandle_t Text_Task_5=NULL;
TaskHandle_t Text_Task_6=NULL;

TaskHandle_t Text_Task_7=NULL;
TaskHandle_t Text_Task_8=NULL;
TaskHandle_t Text_Task_9=NULL;
TaskHandle_t Text_Task_10=NULL;
TaskHandle_t enemy_Task=NULL;
TaskHandle_t Player_Task=NULL;
TaskHandle_t Blcok_Task_1=NULL;
TaskHandle_t Blcok_Task_2=NULL;
typedef struct buttons_buffer {
	unsigned char buttons[SDL_NUM_SCANCODES];
	SemaphoreHandle_t lock;
} buttons_buffer_t;

static buttons_buffer_t buttons = { 0 };
    static char str1[50] = {0};
    static char str2[50] = {0};   
    static char str3[50] = {0};
    
    static char str5[50] = {0};
    static char str6[50] = {0};   
    static char str7[50] = {0};
    static char str8[50] = {0}; 
    static char str9[50] = {0};   
    static char str10[50] = {0};
    static char str11[50] = {0}; 

void checkDraw(unsigned char status, const char *msg)
{
	if (status) {
		if (msg)
			fprints(stderr, "[ERROR] %s, %s\n", msg,
				tumGetErrorMessage());
		else {
			fprints(stderr, "[ERROR] %s\n", tumGetErrorMessage());
		}
	}
}

/*
 * Changes the state, either forwards of backwards
 */
void changeState(volatile unsigned char *state, unsigned char forwards)
{
    switch (forwards) {
        case NEXT_TASK_1:
            if (*state == STATE_COUNT - 1) {
                *state = 0;
            }
            else {
                (*state)++;
            }
            break;
        case NEXT_TASK_2:
            if (*state == STATE_COUNT - 2) {
                *state = 1;
            }
            else {
                (*state)++;
            }
            break;

        case PREV_TASK:
            if (*state == 0) {
                *state = STATE_COUNT - 1;
            }
            else {
                (*state)--;
            }
            break;
        default:
            break;
    }
}

/*
 * Example basic state machine with sequential states
 */
void basicSequentialStateMachine(void *pvParameters)
{
    unsigned char current_state = STARTING_STATE; // Default state
    unsigned char state_changed =
        1; // Only re-evaluate state if it has changed
    unsigned char input = 0;

    const int state_change_period = STATE_DEBOUNCE_DELAY;

    TickType_t last_change = xTaskGetTickCount();

    while (1) {
        if (state_changed) {
            goto initial_state;
        }

        // Handle state machine input
        if (StateQueue)
            if (xQueueReceive(StateQueue, &input, portMAX_DELAY) ==
                pdTRUE)
                if (xTaskGetTickCount() - last_change >
                    state_change_period) {
                    changeState(&current_state, input);
                    state_changed = 1;
                    last_change = xTaskGetTickCount();
                }

initial_state:
        // Handle current state
        if (state_changed) {
            if (button1)
            { 
            switch (current_state) {
                case STATE_ONE:
                    if (DemoTask2) {
                        vTaskSuspend(DemoTask2);
                    }
                    if (DemoTask3) {
                        vTaskSuspend(DemoTask3);
                    }
                    if (DemoTask1) {
                        vTaskResume(DemoTask1);
                    }
                    
                    break;
                case STATE_TWO:
                    if (DemoTask3) {
                        vTaskSuspend(DemoTask3);
                    }
                    if (DemoTask1) {
                        vTaskSuspend(DemoTask1);

                    if (DemoTask2) {
                        vTaskResume(DemoTask2);
                    }
                    break;
                    
                    }
                case STATE_Three:
                  
                    if (DemoTask1) {
                        vTaskSuspend(DemoTask1);
                    } 
                    if (DemoTask2) {
                        vTaskSuspend(DemoTask2);
                    }
                    if (DemoTask3) {
                        vTaskResume(DemoTask3);
                    }
                    break;

                   
                default:
                    break;
            }
            state_changed = 0;
            
            }
        else
        {            
            switch (current_state) {
                case STATE_ONE:
                    if (DemoTask2) {
                        vTaskSuspend(DemoTask2);
                    }
                    if (DemoTask4) {
                        vTaskSuspend(DemoTask4);
                    }
                    if (DemoTask1) {
                        vTaskResume(DemoTask1);
                    }
                    
                    break;
                case STATE_TWO:
                    if (DemoTask4) {
                        vTaskSuspend(DemoTask4);
                    }
                    if (DemoTask1) {
                        vTaskSuspend(DemoTask1);

                    if (DemoTask2) {
                        vTaskResume(DemoTask2);
                    }
                    break;
                    
                    }
                case STATE_Three:
                    
                    if (DemoTask1) {
                        vTaskSuspend(DemoTask1);
                    } 
                    if (DemoTask2) {
                        vTaskSuspend(DemoTask2);
                    }
                    if (DemoTask4) {
                        vTaskResume(DemoTask4);
                    }
                    break;

                   
                default:
                    break;
                   }
            state_changed = 0;
            }
        }
    }
}


void vSwapBuffers(void *pvParameters)
{
	TickType_t xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();
	const TickType_t frameratePeriod = 20;

	tumDrawBindThread(); // Setup Rendering handle with correct GL context

	while (1) {
		if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
			tumDrawUpdateScreen();
			tumEventFetchEvents(FETCH_EVENT_BLOCK);
			xSemaphoreGive(ScreenLock);
			xSemaphoreGive(DrawSignal);
			vTaskDelayUntil(&xLastWakeTime,
					pdMS_TO_TICKS(frameratePeriod));
		}
	}
}

void xGetButtonInput(void)
{
	if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
		xQueueReceive(buttonInputQueue, &buttons.buttons, 0);
		xSemaphoreGive(buttons.lock);
	}
}


void vDrawCave(unsigned char ball_color_inverted)
{
	static unsigned short circlePositionX, circlePositionY;

	//vDrawCaveBoundingBox();

	circlePositionX = CAVE_X + tumEventGetMouseX() / 2;
	circlePositionY = CAVE_Y + tumEventGetMouseY() / 2;

	if (ball_color_inverted)
		checkDraw(tumDrawCircle(circlePositionX, circlePositionY, 20,
					Black),
			  __FUNCTION__);
	else
		checkDraw(tumDrawCircle(circlePositionX, circlePositionY, 20,
					Silver),
			  __FUNCTION__);
}

void vDrawHelpText(void)
{
	static char str[100] = { 0 };
	static int text_width;
	ssize_t prev_font_size = tumFontGetCurFontSize();

	tumFontSetSize((ssize_t)20);

	sprintf(str, "[M]enu, [R]est");

	if (!tumGetTextSize((char *)str, &text_width, NULL))
		checkDraw(tumDrawText(str, SCREEN_WIDTH - text_width - 10,
				      DEFAULT_FONT_SIZE * 0.5, White),
			  __FUNCTION__);

	tumFontSetSize(prev_font_size);
}

#define FPS_AVERAGE_COUNT 50
#define FPS_FONT "IBMPlexSans-Bold.ttf"

void vDrawFPS(void)
{
	static unsigned int periods[FPS_AVERAGE_COUNT] = { 0 };
	static unsigned int periods_total = 0;
	static unsigned int index = 0;
	static unsigned int average_count = 0;
	static TickType_t xLastWakeTime = 0, prevWakeTime = 0;
	static char str[10] = { 0 };
	static int text_width;
	int fps = 0;
	font_handle_t cur_font = tumFontGetCurFontHandle();

	if (average_count < FPS_AVERAGE_COUNT) {
		average_count++;
	} else {
		periods_total -= periods[index];
	}

	xLastWakeTime = xTaskGetTickCount();

	if (prevWakeTime != xLastWakeTime) {
		periods[index] =
			configTICK_RATE_HZ / (xLastWakeTime - prevWakeTime);
		prevWakeTime = xLastWakeTime;
	} else {
		periods[index] = 0;
	}

	periods_total += periods[index];

	if (index == (FPS_AVERAGE_COUNT - 1)) {
		index = 0;
	} else {
		index++;
	}

	fps = periods_total / average_count;

	tumFontSelectFontFromName(FPS_FONT);

	sprintf(str, "FPS: %2d", fps);

	if (!tumGetTextSize((char *)str, &text_width, NULL))
		checkDraw(tumDrawText(str, SCREEN_WIDTH - text_width - 10,
				      SCREEN_HEIGHT - DEFAULT_FONT_SIZE * 1.5,
				      Skyblue),
			  __FUNCTION__);

	tumFontSelectFontFromHandle(cur_font);
	tumFontPutFontHandle(cur_font);
}

void Score_Title ( int score_1, int high_score ,int score_2)
{   int x=90;
    int y=10;
    static char str_1[100] = { 0 };
    static char str_2[100] = { 0 };
    static char str_3[100] = { 0 };  
      
    static int text_width;
    ssize_t prev_font_size = tumFontGetCurFontSize();

    tumFontSetSize((ssize_t)22);

    sprintf(str_1, "  SCORE< 1 >    HI-SCORE   SCORE< 2 > ");
    sprintf(str_2, "%4d                     %4d                  %4d ",  score_1  , high_score,     score_2);
    sprintf(str_3, "CREDIT ");

    if (!tumGetTextSize((char *)str_1, &text_width, NULL))
        checkDraw(tumDrawText((char* )str_1,
        		                x,
                                y,
                                White),
                  __FUNCTION__);
    
        if (!tumGetTextSize((char *)str_2, &text_width, NULL))
        checkDraw(tumDrawText((char* )str_2, 
        		                x,
                                y+30,
                              White),
                  __FUNCTION__);
                  
    if (!tumGetTextSize((char *)str_3, &text_width, NULL))
        checkDraw(tumDrawText((char* )str_3, 
        	             (SCREEN_WIDTH-text_width-70),
                              SCREEN_HEIGHT-50,
                              White),
                  __FUNCTION__);
                  
      tumFontSetSize(prev_font_size);
}
void Move_Text_8(void *pvParameters)
{   static char str[50] = {0};
    sprintf(str, "[P]LAY");
    //animation loop
    for (int i=0; i<=strlen(str); i++)
        {
           str1[i]=str[i];
             vTaskDelay(xDelay);
         
        }vTaskSuspend(Text_Task_8);
}
void Move_Text_9(void *pvParameters)
{   static char str[50] = {0};
    sprintf(str, "SPACE INVADERS");
        vTaskDelay(80);
    //animation loop
    for (int i=0; i<=strlen(str); i++)
        {
           str2[i]=str[i];
             vTaskDelay(xDelay);
         
        }vTaskSuspend(Text_Task_9);
}
void Move_Text_10(void *pvParameters)
{   static char str[50] = {0};
    sprintf(str, "* SCORE ADVANCE TABE *");
        vTaskDelay(350);
    //animation loop
    for (int i=0; i<=strlen(str); i++)
        {
           str3[i]=str[i];
             vTaskDelay(xDelay);
         
        }vTaskSuspend(Text_Task_10);
}
void Move_Text_5(void *pvParameters)
{   static char str[50] = {0};
    sprintf(str, "= 30 POINTS");
        vTaskDelay(600);
    //animation loop
    for (int i=0; i<=strlen(str); i++)
        {
           str9[i]=str[i];
             vTaskDelay(xDelay);
         
        }vTaskSuspend(Text_Task_5);
}

void Move_Text_6(void *pvParameters)
{   static char str[50] = {0};
    sprintf(str, "= 20 POINTS");
    vTaskDelay(800);
    //animation loop
    for (int i=0; i<=strlen(str); i++)
        {
           str10[i]=str[i];
             vTaskDelay(xDelay);
         
        }vTaskSuspend(Text_Task_6);
}

void Move_Text_7(void *pvParameters)
{   static char str[50] = {0};
    sprintf(str, "= 10 POINTS");
    vTaskDelay(1000);
    //animation loop
    for (int i=0; i<=strlen(str); i++)
        {
           str11[i]=str[i];
             vTaskDelay(xDelay);
         
        }vTaskSuspend(Text_Task_7);
}

void PlayText()
{ int y=70;
    int x=120;
   static int text_width;
    tumFontSetSize((ssize_t)28);

if (!tumGetTextSize((char *)str1, &text_width, NULL))
 
   checkDraw(tumDrawText((char *)str1, 
					  CAVE_X+x+5, 
					  y+80, 
					  White),
                 __FUNCTION__);
}
void Move_enemy(void *pvParameters)
{   for(;;)
    {
    for(;;)
    {
        speed+=8;
    if (speed>145)
        break;

        vTaskDelay(xDelay*3);
    }
        for(;;)
    {
        speed-=8;
    if (speed<=0)
        break;

        vTaskDelay(xDelay*3);
    }
  }
}

int Status=0;
 void vCheckStateInput_R()
{       
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        if (buttons.buttons[KEYCODE(R)]) {
           buttons.buttons[KEYCODE(R)] = 0;
            if (Status) {
               vTaskSuspend(enemy_Task);
            vTaskSuspend(Player_Task);
                Status=0;
            }
            else
            { vTaskResume(Player_Task);
                vTaskResume(enemy_Task);
                
                Status=1;
            }
        }                xSemaphoreGive(buttons.lock);
              
    }
}
void DrawMoveText2 ()
{   int y=70;
    int x=120;
   static int text_width;
    tumFontSetSize((ssize_t)20);

 

 
 
 if (!tumGetTextSize((char *)str2, &text_width, NULL))
    checkDraw(tumDrawText((char *)str2, 
					  CAVE_X+x-25, 
					  y+130, 
					  White),
                 __FUNCTION__);
 if (!tumGetTextSize((char *)str3, &text_width, NULL))
        checkDraw(tumDrawText((char *)str3, 
					  CAVE_X+x-70, 
					  y+160, 
					  White),
                 __FUNCTION__);
if (!tumGetTextSize((char *)str9, &text_width, NULL))
    
    checkDraw(tumDrawText((char *)str9, 
					  CAVE_X+x, 
					  y+205, 
					  White),
                 __FUNCTION__);
    if (!tumGetTextSize((char *)str10, &text_width, NULL))             
     checkDraw(tumDrawText((char *)str10, 
					  CAVE_X+x, 
					  y+240, 
					  White),
                    __FUNCTION__);
 if (!tumGetTextSize((char *)str11, &text_width, NULL))
    checkDraw(tumDrawText((char *)str11, 
					  CAVE_X+x, 
					  y+270, 
					  White),
                    __FUNCTION__);


}
void vDrawLogo(void)

{
    int x=230;
    int y=SCREEN_HEIGHT - 210;
	   tumDrawSetLoadedImageScale 	( 	logo_image_30,
		                         0.04
	                                ) ;	

 	    tumDrawSetLoadedImageScale 	( 	logo_image_20,
		                         0.15
	                                ) ;

        tumDrawSetLoadedImageScale 	( 	logo_image_10,
		                         0.05
	                                ) ;		                                   
		checkDraw(
			tumDrawLoadedImage(logo_image_30,
                                 x,
					             y       ),
			__FUNCTION__);

		checkDraw(
			tumDrawLoadedImage(logo_image_20, 
                                x+5,
					            y+40),
			__FUNCTION__);
	
	

		checkDraw(tumDrawLoadedImage(logo_image_10, 
                                    x+5,
					                y+65 ),
			  __FUNCTION__);
	
	
}
void vDrawenemy_30(int x,int y,int v)
{   int Distand= 50;
    x+=v;
  	   tumDrawSetLoadedImageScale 	( 	logo_image_30,
		                         0.03
	                                ) ;	
        checkDraw(
			tumDrawLoadedImage(logo_image_30,
                                 x,
					             y       ),
                          __FUNCTION__);  
         checkDraw(
			tumDrawLoadedImage(logo_image_30,
                                 x+Distand,
					             y       ),
                          __FUNCTION__);
         checkDraw(
			tumDrawLoadedImage(logo_image_30,
                                 x+Distand*2,
					             y       ),
                          __FUNCTION__);  
                 checkDraw(
			tumDrawLoadedImage(logo_image_30,
                                 x+Distand*3,
					             y       ),
                          __FUNCTION__);      
                checkDraw(
			tumDrawLoadedImage(logo_image_30,
                                 x+Distand*4,
					             y       ),
                          __FUNCTION__);  
        		
                checkDraw(
			tumDrawLoadedImage(logo_image_30,
                                 x+Distand*5,
					             y       ),
                          __FUNCTION__); 
                checkDraw(
			tumDrawLoadedImage(logo_image_30,
                                 x+Distand*6,
					             y       ),
                          __FUNCTION__);  
                checkDraw(
			tumDrawLoadedImage(logo_image_30,
                                 x+Distand*7,
					             y       ),
                          __FUNCTION__);  
        		
        		 
        		
} 

void vDrawenemy_20(int x, int y,int v)
{       y+=35;
        x+=5+v;

  	    int Distand= 50;
  	   tumDrawSetLoadedImageScale 	( 	logo_image_20,
		                         0.12
	                                ) ;	
        checkDraw(
			tumDrawLoadedImage(logo_image_20,
                                 x,
					             y       ),
                          __FUNCTION__);  
         checkDraw(
			tumDrawLoadedImage(logo_image_20,
                                 x+Distand,
					             y       ),
                          __FUNCTION__);
         checkDraw(
			tumDrawLoadedImage(logo_image_20,
                                 x+Distand*2,
					             y       ),
                          __FUNCTION__);  
                 checkDraw(
			tumDrawLoadedImage(logo_image_20,
                                 x+Distand*3,
					             y       ),
                          __FUNCTION__);      
                checkDraw(
			tumDrawLoadedImage(logo_image_20,
                                 x+Distand*4,
					             y       ),
                          __FUNCTION__);  
        		
                checkDraw(
			tumDrawLoadedImage(logo_image_20,
                                 x+Distand*5,
					             y       ),
                          __FUNCTION__); 
                checkDraw(
			tumDrawLoadedImage(logo_image_20,
                                 x+Distand*6,
					             y       ),
                          __FUNCTION__);  
                checkDraw(
			tumDrawLoadedImage(logo_image_20,
                                 x+Distand*7,
					             y       ),
                          __FUNCTION__);  
        		
}
void vDrawenemy_10(int x,int y,int v)
{   int Distand= 50;
    y+=65;
    x+=v;
  	   tumDrawSetLoadedImageScale 	( 	logo_image_10,
		                         0.05
	                                ) ;	
        checkDraw(
			tumDrawLoadedImage(logo_image_10,
                                 x,
					             y       ),
                          __FUNCTION__);  
         checkDraw(
			tumDrawLoadedImage(logo_image_10,
                                 x+Distand,
					             y       ),
                          __FUNCTION__);
         checkDraw(
			tumDrawLoadedImage(logo_image_10,
                                 x+Distand*2,
					             y       ),
                          __FUNCTION__);  
                 checkDraw(
			tumDrawLoadedImage(logo_image_10,
                                 x+Distand*3,
					             y       ),
                          __FUNCTION__);      
                checkDraw(
			tumDrawLoadedImage(logo_image_10,
                                 x+Distand*4,
					             y       ),
                          __FUNCTION__);  
        		
                checkDraw(
			tumDrawLoadedImage(logo_image_10,
                                 x+Distand*5,
					             y       ),
                          __FUNCTION__); 
                checkDraw(
			tumDrawLoadedImage(logo_image_10,
                                 x+Distand*6,
					             y       ),
                          __FUNCTION__);  
                checkDraw(
			tumDrawLoadedImage(logo_image_10,
                                 x+Distand*7,
					             y       ),
                          __FUNCTION__);  
        		
        		 
        		
} 

void vDraw_Bullet(int x, int y)
{         int w=2;
            int h=15;
        
    checkDraw(tumDrawFilledBox(x,y,w,h,Green),
    __FUNCTION__);
        


}
int Position=0;
   
void Move_Player(void *pvParameters)
{ 
     for(;;)
     {
       //Button count
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        if (buttons.buttons[KEYCODE(RIGHT)]) {
            buttons.buttons[KEYCODE(RIGHT)] = 0;
            if(Position<220)
            Position+=20;
           
            }
        else if (buttons.buttons[KEYCODE(LEFT)]) {
            buttons.buttons[KEYCODE(LEFT)] = 0;
            if(Position>-280)
            Position-=20;
            ;}

       
        xSemaphoreGive(buttons.lock);
    }
 }
}


void xCreatBlock_1( )
{       int x=80;
        int y=320;

 wall_t *left_block =
        createWall(x, y, BLOCK_THICKNESS,
                   BLOCK_THICKNESS, 0.2,Lime , NULL, NULL);
    // Right wall
    wall_t *right_block =
        createWall(x+2*BLOCK_THICKNESS, y,BLOCK_THICKNESS,
                   BLOCK_THICKNESS, 0.2, Lime, NULL, NULL);
    // Top wall
    wall_t *top_block =
        createWall(x+BLOCK_THICKNESS, y ,BLOCK_THICKNESS
                   , BLOCK_THICKNESS,
                   0.2, Lime, NULL, NULL);
    // Bottom wall
    wall_t *bottom_block_left =
        createWall(x, y-BLOCK_THICKNESS,
                  BLOCK_THICKNESS,BLOCK_THICKNESS,
                   0.2, Lime, NULL, NULL);

    wall_t *bottom_block_right =
        createWall(x+2*BLOCK_THICKNESS ,y-BLOCK_THICKNESS,
                   BLOCK_THICKNESS, BLOCK_THICKNESS,
                   0.2, Lime, NULL, NULL);



        for (;;)
        {
     // Draw the BLOCKs
                checkDraw(tumDrawFilledBox(
                              left_block->x1, left_block->y1,
                              left_block->w, left_block->h,
                              left_block->colour),
                          __FUNCTION__);
                checkDraw(tumDrawFilledBox(right_block->x1,
                                           right_block->y1,
                                           right_block->w,
                                           right_block->h,
                                           right_block->colour),
                          __FUNCTION__);
                checkDraw(tumDrawFilledBox(
                              top_block->x1, top_block->y1,
                              top_block->w, top_block->h,
                              top_block->colour),
                          __FUNCTION__);
                checkDraw(tumDrawFilledBox(bottom_block_left->x1,
                                           bottom_block_left->y1,
                                           bottom_block_left->w,
                                           bottom_block_left->h,
                                           bottom_block_left->colour),
                          __FUNCTION__);

                checkDraw(tumDrawFilledBox(bottom_block_right->x1,
                                           bottom_block_right->y1,
                                           bottom_block_right->w,
                                           bottom_block_right->h,
                                           bottom_block_right->colour),
                          __FUNCTION__);
            }
}
void xCreatBlock_2( )
{       int x=80+7*BLOCK_THICKNESS;
        int y=320;

 wall_t *left_block_1 =
        createWall(x, y, BLOCK_THICKNESS,
                   BLOCK_THICKNESS, 0.2,Lime , NULL, NULL);
    // Right wall
    wall_t *right_block_1 =
        createWall(x+2*BLOCK_THICKNESS, y,BLOCK_THICKNESS,
                   BLOCK_THICKNESS, 0.2, Lime, NULL, NULL);
    // Top wall
    wall_t *top_block_1 =
        createWall(x+BLOCK_THICKNESS, y ,BLOCK_THICKNESS
                   , BLOCK_THICKNESS,
                   0.2, Lime, NULL, NULL);
    // Bottom wall
    wall_t *bottom_block_left_1 =
        createWall(x, y-BLOCK_THICKNESS,
                  BLOCK_THICKNESS,BLOCK_THICKNESS,
                   0.2, Lime, NULL, NULL);

    wall_t *bottom_block_right_1 =
        createWall(x+2*BLOCK_THICKNESS ,y-BLOCK_THICKNESS,
                   BLOCK_THICKNESS, BLOCK_THICKNESS,
                   0.2, Lime, NULL, NULL);



        for (;;)
        {
     // Draw the BLOCKs
                checkDraw(tumDrawFilledBox(
                              left_block_1->x1, left_block_1->y1,
                              left_block_1->w, left_block_1->h,
                              left_block_1->colour),
                          __FUNCTION__);
                checkDraw(tumDrawFilledBox(right_block_1->x1,
                                           right_block_1->y1,
                                           right_block_1->w,
                                           right_block_1->h,
                                           right_block_1->colour),
                          __FUNCTION__);
                checkDraw(tumDrawFilledBox(
                              top_block_1->x1, top_block_1->y1,
                              top_block_1->w, top_block_1->h,
                              top_block_1->colour),
                          __FUNCTION__);
                checkDraw(tumDrawFilledBox(bottom_block_left_1->x1,
                                           bottom_block_left_1->y1,
                                           bottom_block_left_1->w,
                                           bottom_block_left_1->h,
                                           bottom_block_left_1->colour),
                          __FUNCTION__);

                checkDraw(tumDrawFilledBox(bottom_block_right_1->x1,
                                           bottom_block_right_1->y1,
                                           bottom_block_right_1->w,
                                           bottom_block_right_1->h,
                                           bottom_block_right_1->colour),
                          __FUNCTION__);
            }
}

void vDraw_Player(int p )
{ int x=CAVE_SIZE_X+p;
    int y=SCREEN_HEIGHT-120;

  	   tumDrawSetLoadedImageScale 	( logo_image_player,
		                         0.25
	                                ) ;	
        checkDraw(
			tumDrawLoadedImage(logo_image_player,
                                 x,
					             y       ),
                          __FUNCTION__);  

}
void vDraw_PlayerLife()
{   int x=180;
    int y=SCREEN_HEIGHT-80;
    static char str[10] = { 0 };
    sprintf(str, "%d" ,life);
    tumFontSetSize((ssize_t)20);
  tumDrawSetLoadedImageScale 	( logo_image_player_1,
		                         0.12
	                             ) ;	
        if(life>1)  
        checkDraw(
			tumDrawLoadedImage(logo_image_player_1,
                                 x,
					             y       ),
                          __FUNCTION__);  
        if(life>2)
          checkDraw(
			tumDrawLoadedImage(logo_image_player_1,
                                 x+45,
					             y       ),
                          __FUNCTION__);  



        checkDraw(tumDrawText(str, x-20 ,
				                y, White),
			  __FUNCTION__);


}

void Gameover()
{
 static char str[10] = { 0 };
    sprintf(str," Game Over!!! ");
    tumFontSetSize((ssize_t)25);



        checkDraw(tumDrawText(str, CAVE_SIZE_X ,
				                CAVE_Y, Red
                                ),
			  __FUNCTION__);
            vTaskDelay(3000);
        xQueueSend(StateQueue, &next_state_signal_1, 0);
}
void vDraw_Mothership()
{ int x=CAVE_SIZE_X;
    int y=SCREEN_HEIGHT-350;

  	   tumDrawSetLoadedImageScale 	( logo_image_mothership,
		                         0.2
	                                ) ;	
        checkDraw(
			tumDrawLoadedImage(logo_image_mothership,
                                 x,
					             y       ),
                          __FUNCTION__);  

}



void Move_Text_1(void *pvParameters)
{   static char str[50] = {0};
    sprintf(str, "INSERT COIN");
    //animation loop
    for (int i=0; i<=12; i++)
        {
           str5[i]=str[i];
             vTaskDelay(xDelay);
         
        }vTaskSuspend(Text_Task_1);
       
}
void Move_Text_2(void *pvParameters)
{       static char str[50] = {0};
        sprintf(str, "< 1 OR 2 PLAYERS >");
   
            vTaskDelay(150);
    //animation loop
    
    for (int j=0; j<=19; j++)
        {
           str6[j]=str[j];
             vTaskDelay(xDelay);
         
        }vTaskSuspend(Text_Task_2);
   

}
void Move_Text_3(void *pvParameters)
{     
     static char str[50] = {0};
    sprintf(str, "* 1 PLAYER    1 COIN");  


      vTaskDelay(400);
  
    //animation loop
    for ( int k=0; k<=21; k++)
        {
           str7[k]=str[k];
             vTaskDelay(xDelay);
         
        }  vTaskSuspend(Text_Task_3); 
   
}
void Move_Text_4(void *pvParameters)
{
      static char str[50] = {0};
    sprintf(str, "* 2 PLAYER    2 COINS");

        vTaskDelay(600);
   
   //animation loop
    for (int l=0; l<=22; l++)
        {
           str8[l]=str[l];
             vTaskDelay(xDelay);
         
        } vTaskSuspend(Text_Task_4);

}

void DrawText(int x, int y)
{

    static int text_width;
    tumFontSetSize((ssize_t)23);
    		
     if (!tumGetTextSize((char *)str5, &text_width, NULL))
                checkDraw(tumDrawText((char *)str5, 
					            CAVE_X+x+32, 
					            y+170, 
					            White),
                 __FUNCTION__);
     if (!tumGetTextSize((char *)str6, &text_width, NULL))
      
                checkDraw(tumDrawText((char *)str6, 
					            CAVE_X+x, 
					            y+230, 
					            White),
                 __FUNCTION__);   
     if (!tumGetTextSize((char *)str7, &text_width, NULL))
                checkDraw(tumDrawText((char *)str7, 
					            CAVE_X+x, 
					            y+270, 
					            White),
                 __FUNCTION__);    
      if (!tumGetTextSize((char *)str8, &text_width, NULL))            
                checkDraw(tumDrawText((char *)str8, 
					            CAVE_X+x, 
					            y+310, 
					            White),
                 __FUNCTION__);   
}

void vDrawStaticItems(void)
{
	//vDrawHelpText();
	vDrawLogo();
}


static int vCheckStateInput(void)
{
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        if (buttons.buttons[KEYCODE(1)]) {
            button1=1;
           buttons.buttons[KEYCODE(1)] = 0;
            if (StateQueue) {
                xSemaphoreGive(buttons.lock);
                xQueueSend(StateQueue, &next_state_signal_1, 0);
                return 0;
            }
            return -1;
        }
        xSemaphoreGive(buttons.lock);
    }

    return 0;
}
static int vCheckStateInput2(void)
{
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        if (buttons.buttons[KEYCODE(2)]) {
           buttons.buttons[KEYCODE(2)] = 0;
            button1=0;
            if (StateQueue) {
               
                xSemaphoreGive(buttons.lock);
                xQueueSend(StateQueue, &next_state_signal_1, 0);
                return 0;
            }
            return -1;
        }
        xSemaphoreGive(buttons.lock);
    }

    return 0;
}

static int vCheckStateInputP(void)
{
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        if (buttons.buttons[KEYCODE(P)]) {
           buttons.buttons[KEYCODE(P)] = 0;
            if (StateQueue) {
                xSemaphoreGive(buttons.lock);
            
                xQueueSend(StateQueue, &next_state_signal_1, 0);
               
                return 0;
            }
            return -1;
        }
        xSemaphoreGive(buttons.lock);
    }

    return 0;
}

static int vCheckStateInputM(void)
{
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        if (buttons.buttons[KEYCODE(M)]) {
           buttons.buttons[KEYCODE(M)] = 0;
            if (StateQueue) {
                xSemaphoreGive(buttons.lock);
                xQueueSend(StateQueue, &next_state_signal_1, 0);
                return 0;
            }
            return -1;
        }
        xSemaphoreGive(buttons.lock);
    }

    return 0;
}
void UDPHandlerOne(size_t read_size, char *buffer, void *args)
{
	prints("UDP Recv in first handler: %s\n", buffer);
}

void UDPHandlerTwo(size_t read_size, char *buffer, void *args)
{
	prints("UDP Recv in second handler: %s\n", buffer);
}

void vUDPDemoTask(void *pvParameters)
{
	char *addr = NULL; // Loopback
	in_port_t port = UDP_TEST_PORT_1;

	udp_soc_one = aIOOpenUDPSocket(addr, port, UDP_BUFFER_SIZE,
				       UDPHandlerOne, NULL);

	prints("UDP socket opened on port %d\n", port);
	prints("Demo UDP Socket can be tested using\n");
	prints("*** netcat -vv localhost %d -u ***\n", port);

	port = UDP_TEST_PORT_2;

	udp_soc_two = aIOOpenUDPSocket(addr, port, UDP_BUFFER_SIZE,
				       UDPHandlerTwo, NULL);

	prints("UDP socket opened on port %d\n", port);
	prints("Demo UDP Socket can be tested using\n");
	prints("*** netcat -vv localhost %d -u ***\n", port);

	while (1) {
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void MQHandlerOne(size_t read_size, char *buffer, void *args)
{
	prints("MQ Recv in first handler: %s\n", buffer);
}

void MQHanderTwo(size_t read_size, char *buffer, void *args)
{
	prints("MQ Recv in second handler: %s\n", buffer);
}

void vDemoSendTask(void *pvParameters)
{
	static char *test_str_1 = "UDP test 1";
	static char *test_str_2 = "UDP test 2";
	static char *test_str_3 = "TCP test";

	while (1) {
		prints("*****TICK******\n");
		if (mq_one) {
			aIOMessageQueuePut(mq_one_name, "Hello MQ one");
		}
		if (mq_two) {
			aIOMessageQueuePut(mq_two_name, "Hello MQ two");
		}

		if (udp_soc_one)
			aIOSocketPut(UDP, NULL, UDP_TEST_PORT_1, test_str_1,
				     strlen(test_str_1));
		if (udp_soc_two)
			aIOSocketPut(UDP, NULL, UDP_TEST_PORT_2, test_str_2,
				     strlen(test_str_2));
		if (tcp_soc)
			aIOSocketPut(TCP, NULL, TCP_TEST_PORT, test_str_3,
				     strlen(test_str_3));

		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void vMQDemoTask(void *pvParameters)
{
	mq_one = aIOOpenMessageQueue(mq_one_name, MSG_QUEUE_MAX_MSG_COUNT,
				     MSG_QUEUE_BUFFER_SIZE, MQHandlerOne, NULL);
	mq_two = aIOOpenMessageQueue(mq_two_name, MSG_QUEUE_MAX_MSG_COUNT,
				     MSG_QUEUE_BUFFER_SIZE, MQHanderTwo, NULL);

	while (1)

	{
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void TCPHandler(size_t read_size, char *buffer, void *args)
{
	prints("TCP Recv: %s\n", buffer);
}

void vTCPDemoTask(void *pvParameters)
{
	char *addr = NULL; // Loopback
	in_port_t port = TCP_TEST_PORT;

	tcp_soc =
		aIOOpenTCPSocket(addr, port, TCP_BUFFER_SIZE, TCPHandler, NULL);

	prints("TCP socket opened on port %d\n", port);
	prints("Demo TCP socket can be tested using\n");
	prints("*** netcat -vv localhost %d ***\n", port);

	while (1) {
		vTaskDelay(10);
	}
}

void vDemoTask2(void *pvParameters)
{ 
    xTaskCreate(Move_Text_8, "MOVE_TEXT_8", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &Text_Task_8 );
    xTaskCreate(Move_Text_9, "MOVE_TEXT_9", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &Text_Task_9 );
     xTaskCreate(Move_Text_10, "MOVE_TEXT_9", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &Text_Task_10 );
    xTaskCreate(Move_Text_5, "MOVE_TEXT_5", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &Text_Task_5 );
    xTaskCreate(Move_Text_6, "MOVE_TEXT_6", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &Text_Task_6 );
    xTaskCreate(Move_Text_7, "MOVE_TEXT_7", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &Text_Task_7 );

    	logo_image_30 = tumDrawLoadImage(LOGO_30);
		logo_image_20 = tumDrawLoadImage(LOGO_20);
		logo_image_10 = tumDrawLoadImage(LOGO_10);
	while (1) {
		if (DrawSignal)
			if (xSemaphoreTake(DrawSignal, portMAX_DELAY) ==
			    pdTRUE) {
				tumEventFetchEvents(FETCH_EVENT_BLOCK |
						    FETCH_EVENT_NO_GL_CHECK);
				xGetButtonInput(); // Update global input

				xSemaphoreTake(ScreenLock, portMAX_DELAY);

				// Clear screen
				checkDraw(tumDrawClear(Black), __FUNCTION__);
				vDrawStaticItems();
                DrawMoveText2 ();
                PlayText();
				

                Score_Title (  score_1, high_score ,score_2);
				// Draw FPS in lower right corner
				

				xSemaphoreGive(ScreenLock);

				// Get input and check for state change
				vCheckStateInputP();
			}
	}
}

void playBallSound(void *args)
{
	tumSoundPlaySample(a3);
}


void vDemoTask1(void *pvParameters)
{  

    int y=0;
    int x=50;
          
    tumFontSetSize((ssize_t)23);
    
	xTaskCreate(Move_Text_1, "MOVE_TEXT_1", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &Text_Task_1 );
   
    
    xTaskCreate(Move_Text_2, "MOVE_TEXT_2", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &Text_Task_2 ); 
  
   
    xTaskCreate(Move_Text_3, "MOVE_TEXT_3", mainGENERIC_STACK_SIZE,NULL, mainGENERIC_PRIORITY, &Text_Task_3 ); 
      
    
     xTaskCreate(Move_Text_4, "MOVE_TEXT_4", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY , &Text_Task_4 );  
  
   

	while (1) {
		if (DrawSignal)
			if (xSemaphoreTake(DrawSignal, portMAX_DELAY) == pdTRUE)
             {
			    tumEventFetchEvents(FETCH_EVENT_BLOCK |
						    FETCH_EVENT_NO_GL_CHECK);	

				xGetButtonInput(); // Update global button data
                xSemaphoreTake(ScreenLock, portMAX_DELAY);
				checkDraw(tumDrawClear(Black), __FUNCTION__);
			
				// Clear screen
 
				
                DrawText(x, y);
				Score_Title (  score_1, high_score ,score_2);
			    xSemaphoreGive(ScreenLock);
               
				// Check for state change
				vCheckStateInput();
                vCheckStateInput2();
			
			}
	}
    
   
    
}

void CleanText()
{
      sprintf(str1," ");
sprintf(str2," ");
sprintf(str3," ");
  sprintf(str5," ");
sprintf(str6," ");
sprintf(str7," ");

  sprintf(str8," ");
sprintf(str9," ");
sprintf(str10," ");
sprintf(str11," ");
}

void vDemoTask3(void *pvParameters)
{ 
 
logo_image_player = tumDrawLoadImage(LOGO_player);
logo_image_player_1= tumDrawLoadImage(LOGO_player_1);
    int x=30;
    int y=90;
xTaskCreate(Move_enemy, "MOVE_enemy", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &enemy_Task );

xTaskCreate(Move_Player, "MOVE_Player", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &Player_Task );
//xTaskCreate(xCreatBlock_1, "Block_1", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &Blcok_Task_1 );
//xTaskCreate(xCreatBlock_2, "Block_2", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &Blcok_Task_2 );
     while (1) {
        if (DrawSignal)
            if (xSemaphoreTake(DrawSignal, portMAX_DELAY) ==
                pdTRUE) {
                tumEventFetchEvents(FETCH_EVENT_BLOCK |
                                    FETCH_EVENT_NO_GL_CHECK);
                xSemaphoreTake(ScreenLock, portMAX_DELAY);
                
                checkDraw(tumDrawClear(Black), __FUNCTION__);
                Score_Title (  score_1, high_score ,score_2);
                vDrawHelpText();           
                vDrawenemy_30(x,y,speed);
                vDrawenemy_20(x,y,speed);
                vDrawenemy_10(x,y,speed);
                vDraw_Player(Position);
                vDraw_Bullet(CAVE_SIZE_X+Position+15, 400-speed*2);
                xSemaphoreGive(ScreenLock);
                xGetButtonInput();
                vCheckStateInput_R();
                vDraw_PlayerLife();
                vCheckStateInputM();
                if (life==0)
                Gameover();
            }
     }

}
void vDemoTask4(void *pvParameters)
{    
    logo_image_player = tumDrawLoadImage(LOGO_player);
    logo_image_mothership= tumDrawLoadImage(LOGO_mothership);
    logo_image_player_1=  tumDrawLoadImage(LOGO_player_1);
    xTaskCreate(Move_Player, "MOVE_Player", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &Player_Task );
	while (1) {
		if (DrawSignal)
			if (xSemaphoreTake(DrawSignal, portMAX_DELAY) == pdTRUE)
             {
			    tumEventFetchEvents(FETCH_EVENT_BLOCK |
						    FETCH_EVENT_NO_GL_CHECK);	

				xGetButtonInput(); // Update global button data
                xSemaphoreTake(ScreenLock, portMAX_DELAY);
				checkDraw(tumDrawClear(Black), __FUNCTION__);
			
				// Clear screen
 
				vDraw_Player(Position);
                vDrawHelpText(); 
				Score_Title (  score_1, high_score ,score_2);
			    xSemaphoreGive(ScreenLock);
                vCheckStateInput_R();
				// Check for state change
				vCheckStateInputM();
                vDraw_PlayerLife();
                vDraw_Mothership();
                if (life==0)
                Gameover();
			}
	}       
    
   
    
}

#define PRINT_TASK_ERROR(task) PRINT_ERROR("Failed to print task ##task");

int main(int argc, char *argv[])
{
	char *bin_folder_path = tumUtilGetBinFolderPath(argv[0]);

	prints("Initializing: ");

	//  Note PRINT_ERROR is not thread safe and is only used before the
	//  scheduler is started. There are thread safe print functions in
	//  TUM_Print.h, `prints` and `fprints` that work exactly the same as
	//  `printf` and `fprintf`. So you can read the documentation on these
	//  functions to understand the functionality.

	if (tumDrawInit(bin_folder_path)) {
		PRINT_ERROR("Failed to intialize drawing");
		goto err_init_drawing;
	} else {
		prints("drawing");
	}

	if (tumEventInit()) {
		PRINT_ERROR("Failed to initialize events");
		goto err_init_events;
	} else {
		prints(", events");
	}

	if (tumSoundInit(bin_folder_path)) {
		PRINT_ERROR("Failed to initialize audio");
		goto err_init_audio;
	} else {
		prints(", and audio\n");
	}

	if (safePrintInit()) {
		PRINT_ERROR("Failed to init safe print");
		goto err_init_safe_print;
	}

	

	atexit(aIODeinit);

	//Load a second font for fun
	tumFontLoadFont(FPS_FONT, DEFAULT_FONT_SIZE);

	buttons.lock = xSemaphoreCreateMutex(); // Locking mechanism
	if (!buttons.lock) {
		PRINT_ERROR("Failed to create buttons lock");
		goto err_buttons_lock;
	}

	DrawSignal = xSemaphoreCreateBinary(); // Screen buffer locking
	if (!DrawSignal) {
		PRINT_ERROR("Failed to create draw signal");
		goto err_draw_signal;
	}
	ScreenLock = xSemaphoreCreateMutex();
	if (!ScreenLock) {
		PRINT_ERROR("Failed to create screen lock");
		goto err_screen_lock;
	}

	// Message sending
	StateQueue = xQueueCreate(STATE_QUEUE_LENGTH, sizeof(unsigned char));
	if (!StateQueue) {
		PRINT_ERROR("Could not open state queue");
		goto err_state_queue;
	}

	if (xTaskCreate(basicSequentialStateMachine, "StateMachine",
			mainGENERIC_STACK_SIZE * 2, NULL,
			configMAX_PRIORITIES - 1, &StateMachine) != pdPASS) {
		PRINT_TASK_ERROR("StateMachine");
		goto err_statemachine;
	}
	if (xTaskCreate(vSwapBuffers, "BufferSwapTask",
			mainGENERIC_STACK_SIZE * 2, NULL, configMAX_PRIORITIES,
			&BufferSwap) != pdPASS) {
		PRINT_TASK_ERROR("BufferSwapTask");
		goto err_bufferswap;
	}

	/** Demo Tasks */
	if (xTaskCreate(vDemoTask1, "DemoTask1", mainGENERIC_STACK_SIZE * 2,
			NULL, mainGENERIC_PRIORITY, &DemoTask1) != pdPASS) {
		PRINT_TASK_ERROR("DemoTask1");
		goto err_demotask1;
	}
	if (xTaskCreate(vDemoTask2, "DemoTask2", mainGENERIC_STACK_SIZE * 2,
			NULL, mainGENERIC_PRIORITY, &DemoTask2) != pdPASS) {
		PRINT_TASK_ERROR("DemoTask2");
		goto err_demotask2;
	}
     if (xTaskCreate(vDemoTask3, "DemoTask3", mainGENERIC_STACK_SIZE * 2,
                    NULL, mainGENERIC_PRIORITY, &DemoTask3) != pdPASS) {
       PRINT_TASK_ERROR("DemoTask3");
        goto err_demotask3;
    }
    if (xTaskCreate(vDemoTask4, "DemoTask4", mainGENERIC_STACK_SIZE * 2,
                    NULL, mainGENERIC_PRIORITY, &DemoTask4) != pdPASS) {
       PRINT_TASK_ERROR("DemoTask4");
        goto err_demotask4;
    }
	/** SOCKETS */
	xTaskCreate(vUDPDemoTask, "UDPTask", mainGENERIC_STACK_SIZE * 2, NULL,
		    configMAX_PRIORITIES - 1, &UDPDemoTask);
	xTaskCreate(vTCPDemoTask, "TCPTask", mainGENERIC_STACK_SIZE, NULL,
		    configMAX_PRIORITIES - 1, &TCPDemoTask);

	/** POSIX MESSAGE QUEUES */
	xTaskCreate(vMQDemoTask, "MQTask", mainGENERIC_STACK_SIZE * 2, NULL,
		    configMAX_PRIORITIES - 1, &MQDemoTask);
	xTaskCreate(vDemoSendTask, "SendTask", mainGENERIC_STACK_SIZE * 2, NULL,
		    configMAX_PRIORITIES - 1, &DemoSendTask);

	vTaskSuspend(DemoTask1);
	vTaskSuspend(DemoTask2);
    vTaskSuspend(DemoTask3);
    vTaskSuspend(DemoTask4);

	tumFUtilPrintTaskStateList();

	vTaskStartScheduler();

	return EXIT_SUCCESS;

err_demotask4:
    vTaskDelete(DemoTask2);
err_demotask3:
    vTaskDelete(DemoTask2);
err_demotask2:
	vTaskDelete(DemoTask1);
err_demotask1:
   
	vTaskDelete(BufferSwap);
err_bufferswap:
	vTaskDelete(StateMachine);
err_statemachine:
	vQueueDelete(StateQueue);
err_state_queue:
	vSemaphoreDelete(ScreenLock);
err_screen_lock:
	vSemaphoreDelete(DrawSignal);
err_draw_signal:
	vSemaphoreDelete(buttons.lock);
err_buttons_lock:
	tumSoundExit();
err_init_audio:
	tumEventExit();
err_init_events:
	tumDrawExit();
err_init_drawing:
	safePrintExit();
err_init_safe_print:
	return EXIT_FAILURE;
}

// cppcheck-suppress unusedFunction
__attribute__((unused)) void vMainQueueSendPassed(void)
{
	/* This is just an example implementation of the "queue send" trace hook. */
}

// cppcheck-suppress unusedFunction
__attribute__((unused)) void vApplicationIdleHook(void)
{
#ifdef __GCC_POSIX__
	struct timespec xTimeToSleep, xTimeSlept;
	/* Makes the process more agreeable when using the Posix simulator. */
	xTimeToSleep.tv_sec = 1;
	xTimeToSleep.tv_nsec = 0;
	nanosleep(&xTimeToSleep, &xTimeSlept);
#endif
}
