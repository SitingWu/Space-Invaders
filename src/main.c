#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <SDL2/SDL_scancode.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"



#include "TUM_Utils.h"
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
#define PREV_TASK 2

#define STARTING_STATE STATE_ONE

#define STATE_DEBOUNCE_DELAY 300

#define KEYCODE(CHAR) SDL_SCANCODE_##CHAR
#define CAVE_SIZE_X SCREEN_WIDTH / 2
#define CAVE_SIZE_Y SCREEN_HEIGHT / 2
#define CAVE_X CAVE_SIZE_X / 2
#define CAVE_Y CAVE_SIZE_Y / 2
#define BLOCK_THICKNESS 15
#define world_offsetX 20
#define world_offsetY 85

#define player_offsetX SCREEN_WIDTH / 2

/** GAME DIMENSIONS */
#define WALL_OFFSET 20
#define WALL_THICKNESS 10
#define GAME_FIELD_OUTER WALL_OFFSET-15
#define GAME_FIELD_INNER (GAME_FIELD_OUTER + WALL_THICKNESS)
#define GAME_FIELD_HEIGHT_INNER (SCREEN_HEIGHT - 2 * GAME_FIELD_INNER)
#define GAME_FIELD_HEIGHT_OUTER (SCREEN_HEIGHT - 2 * GAME_FIELD_OUTER)
#define GAME_FIELD_WIDTH_INNER (SCREEN_WIDTH - 5 * GAME_FIELD_INNER)
#define GAME_FIELD_WIDTH_OUTER (SCREEN_WIDTH - 2 * GAME_FIELD_OUTER)


/** PADDLE MOVEMENT */
#define PADDLE_INCREMENT_SIZE 5
#define PADDLE_LENGTH (SCREEN_WIDTH / 10)-10
#define PADDLE_INCREMENT_COUNT                                                 \
    (GAME_FIELD_WIDTH_INNER - PADDLE_LENGTH) / PADDLE_INCREMENT_SIZE
#define PADDLE_START_LOCATION_X ((SCREEN_WIDTH / 2) - (PADDLE_LENGTH / 2))
#define PADDLE_EDGE_OFFSET 10
#define PADDLE_HIGHT 20

//Bildern define

#define LOGO_30 "30point.jpg"
#define LOGO_20 "20point.png"
#define LOGO_10 "10point.jpg"
#define LOGO_player "player"
#define LOGO_player_1 "player_1"
#define LOGO_mothership "mothership"

/** PADDLE MOVING FLAGS */
#define START_LEFT 1
#define START_RIGHT 2


#define UDP_BUFFER_SIZE 2000
#define UDP_TEST_PORT_1 1234
#define UDP_TEST_PORT_2 4321
#define UDP_TRANSMIT_PORT 1235
#define MSG_QUEUE_BUFFER_SIZE 1000
#define MSG_QUEUE_MAX_MSG_COUNT 10
#define TCP_BUFFER_SIZE 2000
#define TCP_TEST_PORT 2222

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
//2Player
aIO_handle_t udp_soc_receive = NULL, udp_soc_transmit = NULL;

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
static SemaphoreHandle_t BulletInactivePlayer = NULL;
static QueueHandle_t ScoreQueue30 = NULL;
static QueueHandle_t ScoreQueue20 = NULL;
static QueueHandle_t ScoreQueue10 = NULL;
static QueueHandle_t MothershipScoreQueue =NULL;
static QueueHandle_t LifeCountQueue =NULL;

//2Player
static QueueHandle_t PlayerScoreQueue = NULL;
static QueueHandle_t StartDirectionQueue = NULL;
static QueueHandle_t NextKeyQueue = NULL;
static QueueHandle_t BulletYQueue = NULL;
static QueueHandle_t PaddleXQueue = NULL;
static QueueHandle_t DifficultyQueue = NULL;


static image_handle_t logo_image_30 = NULL;
static image_handle_t logo_image_20 = NULL;
static image_handle_t logo_image_10 = NULL;
static image_handle_t logo_image_player = NULL;
static image_handle_t logo_image_player_1 = NULL;
static image_handle_t logo_image_mothership = NULL;

static TaskHandle_t Text_task_all = NULL;
static TaskHandle_t xtask_menu = NULL;
static TaskHandle_t enemy_Task = NULL;
static TaskHandle_t Player_Task = NULL;
static TaskHandle_t Check_kill_player = NULL;
static TaskHandle_t Check_kill_enemy = NULL;
static TaskHandle_t enemy_position = NULL;
static TaskHandle_t help_text_play = NULL;
static TaskHandle_t Check_Button_Input=NULL;
static TaskHandle_t EnemyShoot_task=NULL;

//2Player
TaskHandle_t MothershipTask = NULL;
TaskHandle_t PlayerTask = NULL;
TaskHandle_t MothershipBulletTask = NULL;
TaskHandle_t PlayerBulletTask = NULL;
TaskHandle_t PausedStateTask = NULL;
TaskHandle_t UDPControlTask = NULL;
static SemaphoreHandle_t BulletInactive = NULL;
static SemaphoreHandle_t HandleUDP = NULL;

//Anfang
#define sizex 22
#define sizey 15
#define empty 0
#define enemy30 3
#define enemy20 2
#define enemy10 1
#define player 4
#define block 5
#define enemy_bullet 6
#define player_bullet 7
typedef struct buttons_buffer {
	unsigned char buttons[SDL_NUM_SCANCODES];
	SemaphoreHandle_t lock;
} buttons_buffer_t;

static buttons_buffer_t buttons = { 0 };
static char str1[50] = { 0 };
static char str2[50] = { 0 };
static char str3[50] = { 0 };
static char str5[50] = { 0 };
static char str6[50] = { 0 };
static char str7[50] = { 0 };
static char str8[50] = { 0 };
static char str9[50] = { 0 };
static char str10[50] = { 0 };
static char str11[50] = { 0 };

// globale variable
int xDelay =8;
int speed = 0;
int button1 = 0;
//int life = 3;
int cur_enemy = 40;
const int total_enemy = 40;
int Ready_bullet_p = 1;
int player_X = sizex/2;
int player_PositionX = CAVE_SIZE_X-30;
int enemy_move=0;
int Bullet_speed=0;
int Bullet_speed_enemy=0;
int lowestEnemy=4;
int Ready_bullet_e=1;
int enemy_shoot=0;
int enemy_num=0;
int enemy_bullet_x=0;
int Position = 0;
int world[sizey][sizex];



void Enemyfield()
{
	int x = 0;
	int y = 0;
	//reset

	for (y = 0; y <= sizey; y++) {
		for (x = 0; x <= sizex; x++) {
			world[y][x] = empty;
		}
	}

	//player
	world[sizey - 1][11] = player;
    world[sizey - 1][10] = player;
    world[sizey - 1][12] = player;

	//enemy30
	for (x = 0; x <= 14; x += 2)

	{
		world[0][x] = enemy30;
	}
	//enemy20
	for (y = 1; y < 3; y++) {
		for (x = 0; x <= 14; x += 2) {
			world[y][x] = enemy20;
		}
	}
	//enemy10
	for (y = 3; y < 5; y++) {
		for (x = 0; x <= 14; x += 2) {
			world[y][x] = enemy10;
		}
	}

	//Blöckern
	for (y = sizey - 3; y > sizey - 6; y--) {
		int abstand = 5;
		for (x = 0; x < 2; x++)

			world[y][x + 4] = block;

		for (x = 0; x < 2; x++)
			world[y][x + 4 + abstand] = block;
		for (x = 0; x < 2; x++)
			world[y][x + 4 + abstand * 2] = block;
		for (x = 0; x < 2; x++)
			world[y][x + 4 + abstand * 3] = block;
	}
} 
int d=0;
int down=0;
void move_enemy_position()
{    int y=0; int d_max=3; int Down=0;
    int flag=0;
    vTaskDelay(xDelay * 5);
	for (;;)
     {     Down++;
           
         //  printf("Down=%i\n",Down);
         //  printf("Down_Y=%i\n",y);
         //  printf("Down_d=%i\n",d);
            if  (Down==3)
            {
                    d=1 ;d_max=4; down+=32.5 ;
                    lowestEnemy+=1;flag=1;
            }
            else if (Down==5)
             {
                    d=2 ;d_max=5; down+=32.5 ;
                    lowestEnemy+=1;flag=1;
            }
            else if (Down==8)
             {
                    d=3 ;d_max=6;down+=32.5  ;
                    lowestEnemy+=1;flag=1;
            }else if (Down==11)
             {
                    d=4 ;d_max=6;down+=32.5  ;
                    lowestEnemy+=1;flag=1;
            }
            if (d>0&&d<5&&flag)
            {
                for(y=d_max+3;y>=d-1;y--)
                {
                    for(int x = 0; x <=22; x++ )
                    {
                        world[y+1][x]=world[y][x];
                        flag=0;
                      }
                }

            }
		for (int k = 1; k < 9; k++) {
                enemy_move=k;
             //     printf("Enemy_Move %i\n",enemy_move);
		         int x = 0;
                 
            //      printf("enemy_Y=d =%i\n",y);
        	//enemy30
			for (x = 0+k; x <= 14+k; x += 2)
            {    y=d;  
            //    printf("enemy30_Y1=%i\n",y);                  
			    if(world[y][x-1] == empty)
                    world[y][x] = empty;
                else
               { 
				world[y][x-1] = empty;
				world[y][x] = enemy30;

               }   
           //    printf("enemy30_Y=%i\n",y);
			//enemy20
			for (y=d+1; y <d_max; y++) {
                 if(world[y][x-1] == empty)
                    world[y][x] = empty;
                else{
					world[y][x-1] = empty;
					world[y][x] = enemy20;
				}//printf("enemy20_Y=%i\n",y);
            }
			
			//enemy10

			for (y=d+3; y < d_max+2; y++) {
			
			 if(world[y][x-1] == empty)
                    world[y][x] = empty;
                else{
					world[y][x-1] = empty;
					world[y][x] = enemy10;
				}//printf("enemy10_Y=%i\n",y);
            }
             //   printf("enemy_x=%i\n",x);
			}vTaskDelay(xDelay * 8);
             //   printf("Moving_left %i\n",k);

    	}  
	
		for (int j = 8; j>=1; j--) {
           
		         int x = 14;
                 
			//enemy30
			for (x = 14+j; x >= 0+j; x -= 2)
            	{    y=d;
		
				if(world[y][x] == empty)
                    world[y][x - 1] = empty;
                else
               { 
				world[y][x] = empty;
				world[y][x -1] = enemy30;
               }   
			//enemy20
			for ( y=d+1; y < d_max; y++) {
				
                    if(world[y][x] == empty)
                    world[y][x -1] = empty;
                else{
					world[y][x] = empty;
					world[y][x -1] = enemy20;
				}
				}
			
			//enemy10
			for ( y=d+3; y <d_max+2; y++) {
                if(world[y][x] == empty)
                    world[y][x - 1] = empty;
                else{
					world[y][x] = empty;
					world[y][x - 1] = enemy10;
				}
				} 
         //       printf("enemy_x_2=%i\n",x);
			 } enemy_move=j-1;
          //  printf("Enemy_Move_2 %i\n",enemy_move);
           
           vTaskDelay(xDelay * 8);
         
		// printf("Moving_right %i\n",j);
		    } 
        }
     }



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
		} else {
			(*state)++;
		}
		break;
	case NEXT_TASK_2:
		if (*state == STATE_COUNT - 2) {
			*state = 1;
		} else {
			(*state)++;
		}
		break;

	case PREV_TASK:
		if (*state == 0) {
			*state = STATE_COUNT - 1;
		} else {
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
			if (button1) {
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

			} else {
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
void vDrawHelpText(void)
{
	static char str[50] = { 0 };
    static char str1[50] = { 0 };
    static char str2[50]={0};
    static int text_width;
	ssize_t prev_font_size = tumFontGetCurFontSize();

	tumFontSetSize((ssize_t)20);

	sprintf(str, "[M]enu, [R]est");
    sprintf(str1, "[SPACE] for Shoot!");
    sprintf(str2, "[D]ifficult");

	if (!tumGetTextSize((char *)str, &text_width, NULL))
		checkDraw(tumDrawText(str, SCREEN_WIDTH - text_width - 10,
				      DEFAULT_FONT_SIZE * 0.5, White),
			  __FUNCTION__);
    	if (!tumGetTextSize((char *)str1, &text_width, NULL))
		checkDraw(tumDrawText(str1, SCREEN_WIDTH - text_width - 10,
				      DEFAULT_FONT_SIZE * 0.5+20, White),
			  __FUNCTION__);
    checkDraw(tumDrawText(str, SCREEN_WIDTH - text_width - 10,
				      DEFAULT_FONT_SIZE * 0.5+45
                    , White),
			  __FUNCTION__);



	tumFontSetSize(prev_font_size);
}
 int high_score_1=0;
int high_score_2=0;
void Score_Title(int score_1,  int score_2)
{
	int x = 75;
	int y = 10;
	static char str_1[100] = { 0 };
	static char str_2[100] = { 0 };
	//static char str_3[100] = { 0 };


	static int text_width;
	ssize_t prev_font_size = tumFontGetCurFontSize();


   

	tumFontSetSize((ssize_t)22);
    if(score_1>high_score_1)
    high_score_1=score_1;
	  if(score_2>high_score_2)
    high_score_2=score_2;
	sprintf(str_1, "  SCORE< 1 >    HI-SCORE   SCORE< 2 > ");
	sprintf(str_2, "%04d                 %04d  %04d               %04d ",
		score_1, high_score_1, high_score_2, score_2);
	//sprintf(str_3, "CREDIT ");

	if (!tumGetTextSize((char *)str_1, &text_width, NULL))
		checkDraw(tumDrawText((char *)str_1, x, y, White),
			  __FUNCTION__);

	if (!tumGetTextSize((char *)str_2, &text_width, NULL))
		checkDraw(tumDrawText((char *)str_2, x, y + 30, White),
			  __FUNCTION__);

/*	if (!tumGetTextSize((char *)str_3, &text_width, NULL))
		checkDraw(tumDrawText((char *)str_3,
				      (SCREEN_WIDTH - text_width - 70),
				      SCREEN_HEIGHT - 50, White),
			  __FUNCTION__);*/

	tumFontSetSize(prev_font_size);
}
 int flag = 1;
 int p;
	
void vDraw_Bullet()
{
	int w = 6;
	int h = 6;
	
    int y=440-Bullet_speed*(8.5-(lowestEnemy-4)*0.3);
	if (xSemaphoreTake(BulletInactivePlayer, 0)==pdTRUE) {
		int x = player_PositionX+25;
          printf("BulletInactivePlayer==False"); 
		if (flag) {
			p = x;
            flag=0;
          
        } // printf("flag=%i",flag);
		//printf("Bullet_p=%i", p);
        //if (world[sizey-2-Bullet_speed/2][p_x]==player_bullet)
		checkDraw(tumDrawFilledBox(p, y, w, h, Green), __FUNCTION__);
	}
        else
	    flag = 1;
}

void Kill_task_player(void *pvParameters)

{  const unsigned char point = 1;
    DifficultyQueue = xQueueCreate(5, sizeof(unsigned char));
    ScoreQueue30 = xQueueCreate(10, sizeof(unsigned char));
    ScoreQueue20 = xQueueCreate(10, sizeof(unsigned char));
    ScoreQueue10 = xQueueCreate(10, sizeof(unsigned char));
    
    BulletInactivePlayer = xSemaphoreCreateBinary();
	for (;;) {
          
		if (xSemaphoreTake(BulletInactivePlayer, 0)==pdTRUE) {
            int x = player_X;
          //  printf("shooting\n");
             
			for (int y = sizey - 2; y >= 0; y--) {
               //  printf("player_matrix=%i\n", player_X);
              //   printf("player_X=%i\n", x);
              //    printf("player_y=%i\n", y);
				if (world[y - 1][x] == empty&& y>1) {
					Ready_bullet_p = 0;
					world[y][x] = empty;
					world[y - 1][x] = player_bullet;
                    Bullet_speed++;
                    xSemaphoreGive(BulletInactivePlayer);
                //    printf("Bullet_Position=%i, Y %i\n",x,y-1);
					vTaskDelay(xDelay * 2);
                    xSemaphoreGive(BulletInactivePlayer);
                     Bullet_speed++;
                    vTaskDelay(xDelay * 2);
                    xSemaphoreGive(BulletInactivePlayer);
                    Bullet_speed++;
               vTaskDelay(xDelay * 2);
               xSemaphoreGive(BulletInactivePlayer);
                    Bullet_speed++;
 
				} else if (world[y - 1][x] == enemy30 ||
					   world[y - 1][x] == enemy20 ||
					   world[y - 1][x] == enemy10) 
                {
                        if(world[y - 1][x] == enemy30 )
                       

                     {
                    xQueueSend(ScoreQueue30, &point, portMAX_DELAY);
                    }
                     else if(world[y - 1][x] == enemy20)

                     {
                     xQueueSend(ScoreQueue20, &point, portMAX_DELAY);
                    }
                     else if(world[y - 1][x] == enemy10)

                     {
                     xQueueSend(ScoreQueue10, &point, portMAX_DELAY);
                    }
                   // score_1 += world[y - 1][x] * 10;
                    if(world[y][x] != enemy30 &&
					   world[y][x] != enemy20 &&
					   world[y][x] != enemy10)
					world[y][x] = empty;
					world[y - 1][x] = empty;
              //      printf("Enemy_Killed_X=%i Y=%i \n",x,y-1);
					cur_enemy--;
                    Bullet_speed=0;
                    vTaskDelay(xDelay/5);
                    xSemaphoreTake(BulletInactivePlayer,0);
	             //Ready_bullet_p=1;
                   break;
				
				} 
                else if (world[y - 1][x] == enemy_bullet) {
					world[y][x] = empty;
					world[y - 1][x] = empty;
                    Bullet_speed=0;
                    vTaskDelay(xDelay/5 );
					xSemaphoreGive(BulletInactivePlayer);
                    break;
					

				} else if (world[y - 1][x] == block) {
					world[y][x] = empty;
					world[y - 1][x] = empty;
                     Bullet_speed=0;
                    vTaskDelay(xDelay/5 );
					//Ready_bullet_p=1;
                    xSemaphoreTake(BulletInactivePlayer,0);
                   // printf("Ready_bullet_p_b= %i\n",Ready_bullet_p);
                    break;
					
				}
                else if (world[y - 1][x] == empty && y==1)
                {   world[y][x] = empty;
					world[y - 1][x] = empty;
                     Bullet_speed=0;
                     vTaskDelay(xDelay/5 );
                     //Ready_bullet_p=1;
                     xSemaphoreTake(BulletInactivePlayer,0);
                    //printf("Ready_bullet_p_b= %i\n",Ready_bullet_p);
                    //printf("Shoot again\n");
                    break;
                }
			}
            
		} else
           // printf("Bullet Ready!\n");
            
            vTaskSuspend(Check_kill_player);
        
	}
}

static int LowestEnemy()
{
    int y=lowestEnemy;
    if(lowestEnemy>=0)
    {
        int cur_lowestenemy=0;
        for(;;)
        {
        for (int x=0;x<=22;x++)
        {   
                if(world[y][x]==enemy10||
                world[y][x]==enemy20||
                world[y][x]==enemy30 )
                    cur_lowestenemy+=1;  
                    printf("CUR_ENEMY_X=%i\n",x);
                    printf("CUR_ENEMY=%i\n",cur_lowestenemy);

                
             }
                    if(cur_lowestenemy==0)
                    lowestEnemy-=1;
                    else
                    {
                    return cur_lowestenemy;
                        break;
                    }
     }           
  }
}

void Enemy_Shoot(void* pvParameter)
{   LifeCountQueue = xQueueCreate(10, sizeof(unsigned char));
    const unsigned char life =1;

         vTaskDelay(xDelay*15);
         int cur_enemy=LowestEnemy();
    for(;;)
    {  if(xSemaphoreTake(BulletInactivePlayer, 0)==pdTRUE) //check current Anzahl of lowstEnemy when player shoot
        cur_enemy=LowestEnemy();
      int enemy_x;
    int y=lowestEnemy;
    int shoot =rand()% cur_enemy;
  
   // printf("Cur_Enemy=%i \n",cur_enemy);
   // printf("Shoot=%i \n",shoot);
    //check random enemy
    for (int x=0;x<=22;x++)
    { 
        if(world[y][x]==enemy10||
            world[y][x]==enemy20||
            world[y][x]==enemy30)
            {   shoot-=1;
             
                Bullet_speed_enemy=0;
                if(shoot ==1)
                {Ready_bullet_e=0;
                enemy_x=x;
                enemy_bullet_x=x*25+world_offsetX;
                world[y+1][x]=enemy_bullet;
                
                break;
                }
            }
           
     } 

    //  printf("enemy_Ready=%i \n",Ready_bullet_e);
    //    printf("enemy_num=%i \n",enemy_num);
    //    printf("enemy_bullet=%i ,Y=%i \n",enemy_x,y+1);
    //shooting
    for(y=lowestEnemy+1;y<=sizey-1;y++)
    {   
        printf("enemy_bullet=%i ,Y=%i \n",enemy_x,y+1);

           if (world[y+1][enemy_x]==empty && y+1==14)
            {
                 world[y][enemy_x]=empty;
                 world[y+1][enemy_x]=empty;
                Ready_bullet_e=1;
           //     printf("Ready_Enemy_bullet=%i\n",Ready_bullet_e);
                 vTaskDelay(xDelay/5);
           
                break;
            }   
        if (world[y+1][enemy_x]==empty)
        {   world[y][enemy_x]=empty;
            world[y+1][enemy_x]=enemy_bullet;
                Ready_bullet_e=0;
                Bullet_speed_enemy++;
                vTaskDelay(xDelay*2);
                Bullet_speed_enemy++;
                vTaskDelay(xDelay*2);
                 Bullet_speed_enemy++;
                vTaskDelay(xDelay*2);
                Bullet_speed_enemy++;
               
               // printf("enemy_bullet=%i ,Y=%i \n",enemy_x,y+1);
               
            }
            
        else if (world[y+1][enemy_x]==block)
        {   vTaskDelay(xDelay*2);
            world[y][enemy_x]=empty;
            world[y+1][enemy_x]=empty;
          //  printf("enemy_bullet==Block\n");
          //  printf("enemy_bulletX=%i Y=%i \n",enemy_x,y+1);
              Ready_bullet_e=1;
              
           
                break;
        }
        else if (world[y+1][enemy_x]==player_bullet)
        {   
            vTaskDelay(xDelay*2);
            world[y][enemy_x]=empty;
            world[y+1][enemy_x]=empty;
              Ready_bullet_e=1;
             
           
            break;
        }    
        else if (world[y+1][enemy_x]==player)  
         {  vTaskDelay(xDelay*2);
            printf("Kill Player");
            world[y][enemy_x]=empty;
            for(int x=enemy_x-2;x<enemy_x+3;x++)
            {
            world[y+1][x]=empty; 
            }
            world[sizey-1][sizex/2]=player;
            world[sizey-1][sizex/2-1]=player;
            world[sizey-1][sizex/2+1]=player;
            //life-=1;
            xQueueSend(LifeCountQueue, &life, portMAX_DELAY);
            Ready_bullet_e=1;
            //vTaskSuspend(Kill_task_player);
            
               player_PositionX=CAVE_SIZE_X-30;
               Position=0;
               player_X=sizex/2;
               vTaskDelay(xDelay*8);
            
           
           
              
               //vTaskResume(Kill_task_player);
           
           break;
           
         }

    }vTaskDelay(xDelay*15);

    }
   
}


void vDrawEnemyBullet()
{
    int w = 6;
	int h = 6;
	int p1 = enemy_bullet_x+20;
    int y=world_offsetY+(lowestEnemy+1)*31+Bullet_speed_enemy*8.8;
    if (Ready_bullet_e == 0) {
		
        
		//printf("Bullet_e=%i\n", p1);
       
		checkDraw(tumDrawFilledBox(p1, y, w, h, Red), __FUNCTION__);
	  }
        
}

 void vCheckStateInput_bullet(void)
{
	if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
		if (buttons.buttons[KEYCODE(SPACE)]) {
			buttons.buttons[KEYCODE(SPACE)] = 0;
            //if(ReadyBullet_Player==1)
			if (xSemaphoreTake(BulletInactivePlayer, 0)!=pdTRUE) {
				xSemaphoreGive(BulletInactivePlayer);
                world[sizey - 2][player_X] = player_bullet;
				vTaskResume(Check_kill_player);
				
			}
		
		}
		xSemaphoreGive(buttons.lock);
	}
}
void PlayText()
{
	int y = 70;
	int x = 120;
	static int text_width;
	tumFontSetSize((ssize_t)28);

	if (!tumGetTextSize((char *)str1, &text_width, NULL))

		checkDraw(tumDrawText((char *)str1, CAVE_X + x + 5, y + 80,
				      White),
			  __FUNCTION__);
}
void Move_enemy(void *pvParameters)
{
	for (;;) {
		for (int i = 0; i < 8; i++) {
			speed +=25;
				
			
           
			vTaskDelay(xDelay * 8);
		}
		for (int i = 0; i < 8; i++) {
			speed -=25;
			

			vTaskDelay(xDelay * 8);
		}
	}
}

int Status = 0;
void vCheckStateInput_R()
{
	if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
		if (buttons.buttons[KEYCODE(R)]) {
			buttons.buttons[KEYCODE(R)] = 0;
			if (Status) {
				vTaskSuspend(enemy_Task);
				vTaskSuspend(Player_Task);
				Status = 0;
			} else {
				vTaskResume(Player_Task);
				vTaskResume(enemy_Task);

				Status = 1;
			}
		}
		xSemaphoreGive(buttons.lock);
	}
}
void DrawMoveText2()
{
	int y = 70;
	int x = 120;
	static int text_width;
	tumFontSetSize((ssize_t)20);

	if (!tumGetTextSize((char *)str2, &text_width, NULL))
		checkDraw(tumDrawText((char *)str2, CAVE_X + x - 25, y + 130,
				      White),
			  __FUNCTION__);
	if (!tumGetTextSize((char *)str3, &text_width, NULL))
		checkDraw(tumDrawText((char *)str3, CAVE_X + x - 70, y + 160,
				      White),
			  __FUNCTION__);
	if (!tumGetTextSize((char *)str9, &text_width, NULL))

		checkDraw(tumDrawText((char *)str9, CAVE_X + x, y + 205, White),
			  __FUNCTION__);
	if (!tumGetTextSize((char *)str10, &text_width, NULL))
		checkDraw(tumDrawText((char *)str10, CAVE_X + x, y + 240,
				      White),
			  __FUNCTION__);
	if (!tumGetTextSize((char *)str11, &text_width, NULL))
		checkDraw(tumDrawText((char *)str11, CAVE_X + x, y + 270,
				      White),
			  __FUNCTION__);
}

void Move_text_all(void *pvParameters)
{
	static char str[50] = { 0 };
	sprintf(str, "[P]LAY");
	//animation loop
	for (int i = 0; i <= strlen(str); i++) {
		str1[i] = str[i];
		vTaskDelay(xDelay);
	}

	sprintf(str, "SPACE INVADERS");
	//animation loop
	for (int i = 0; i <= strlen(str); i++) {
		str2[i] = str[i];
		vTaskDelay(xDelay);
	}

	sprintf(str, "* SCORE ADVANCE TABE *");

	//animation loop
	for (int i = 0; i <= strlen(str); i++) {
		str3[i] = str[i];
		vTaskDelay(xDelay);
	}
	sprintf(str, "= 30 POINTS");

	//animation loop
	for (int i = 0; i <= strlen(str); i++) {
		str9[i] = str[i];
		vTaskDelay(xDelay);
	}
	sprintf(str, "= 20 POINTS");

	//animation loop
	for (int i = 0; i <= strlen(str); i++) {
		str10[i] = str[i];
		vTaskDelay(xDelay);
	}
	sprintf(str, "= 10 POINTS");

	//animation loop
	for (int i = 0; i <= strlen(str); i++) {
		str11[i] = str[i];
		vTaskDelay(xDelay);
	}
	vTaskDelete(Text_task_all);
}
void vDrawLogo(void)

{
	int x = 230;
	int y = SCREEN_HEIGHT /2-20;
	tumDrawSetLoadedImageScale(logo_image_30, 0.04);

	tumDrawSetLoadedImageScale(logo_image_20, 0.15);

	tumDrawSetLoadedImageScale(logo_image_10, 0.05);
	checkDraw(tumDrawLoadedImage(logo_image_30, x, y), __FUNCTION__);

	checkDraw(tumDrawLoadedImage(logo_image_20, x + 5, y + 40),
		  __FUNCTION__);

	checkDraw(tumDrawLoadedImage(logo_image_10, x + 5, y + 65),
		  __FUNCTION__);
}
void vDrawenemy_30(int x, int y, int v, int x1, int y1)
{
	int Distand = 45;
	x += v;
    y+=down;
	tumDrawSetLoadedImageScale(logo_image_30, 0.03);
	if (world[y1][x1] == enemy30)
		checkDraw(tumDrawLoadedImage(logo_image_30, x, y),
			  __FUNCTION__);
	if (world[y1][x1 + 2] == enemy30)
		checkDraw(tumDrawLoadedImage(logo_image_30, x + Distand, y),
			  __FUNCTION__);
	if (world[y1][x1 + 4] == enemy30)
		checkDraw(tumDrawLoadedImage(logo_image_30, x + Distand * 2, y),
			  __FUNCTION__);
	if (world[y1][x1 + 6] == enemy30)
		checkDraw(tumDrawLoadedImage(logo_image_30, x + Distand * 3, y),
			  __FUNCTION__);
	if (world[y1][x1 + 8] == enemy30)
		checkDraw(tumDrawLoadedImage(logo_image_30, x + Distand * 4, y),
			  __FUNCTION__);

	if (world[y1][x1 + 10] == enemy30)
		checkDraw(tumDrawLoadedImage(logo_image_30, x + Distand * 5, y),
			  __FUNCTION__);
	if (world[y1][x1 + 12] == enemy30)
		checkDraw(tumDrawLoadedImage(logo_image_30, x + Distand * 6, y),
			  __FUNCTION__);
	if (world[y1][x1 + 14] == enemy30)
		checkDraw(tumDrawLoadedImage(logo_image_30, x + Distand * 7, y),
			  __FUNCTION__);
}

void vDrawenemy_20(int x, int y, int v, int x1, int y1)
{
	y = y+35+down;
	x += 5 + v;

	int Distand = 45;

	tumDrawSetLoadedImageScale(logo_image_20, 0.115);
	if (world[y1][x1] == enemy20)
		checkDraw(tumDrawLoadedImage(logo_image_20, x, y),
			  __FUNCTION__);

	if (world[y1][x1 + 2] == enemy20)
		checkDraw(tumDrawLoadedImage(logo_image_20, x + Distand, y),
			  __FUNCTION__);
	if (world[y1][x1 + 4] == enemy20)
		checkDraw(tumDrawLoadedImage(logo_image_20, x + Distand * 2, y),
			  __FUNCTION__);
	if (world[y1][x1 + 6] == enemy20)
		checkDraw(tumDrawLoadedImage(logo_image_20, x + Distand * 3, y),
			  __FUNCTION__);
	if (world[y1][x1 + 8] == enemy20)
		checkDraw(tumDrawLoadedImage(logo_image_20, x + Distand * 4, y),
			  __FUNCTION__);
	if (world[y1][x1 + 10] == enemy20)

		checkDraw(tumDrawLoadedImage(logo_image_20, x + Distand * 5, y),
			  __FUNCTION__);
	if (world[y1][x1 + 12] == enemy20)
		checkDraw(tumDrawLoadedImage(logo_image_20, x + Distand * 6, y),
			  __FUNCTION__);
	if (world[y1][x1 + 14] == enemy20)
		checkDraw(tumDrawLoadedImage(logo_image_20, x + Distand * 7, y),
			  __FUNCTION__);
}
void vDrawenemy_10(int x, int y, int v, int x1, int y1)
{
	int Distand = 45;
	y =y+ 65+down;
	x += v;
    
	tumDrawSetLoadedImageScale(logo_image_10, 0.04);
	if (world[y1][x1] == enemy10)
		checkDraw(tumDrawLoadedImage(logo_image_10, x, y),
			  __FUNCTION__);
	if (world[y1][x1 + 2] == enemy10)
		checkDraw(tumDrawLoadedImage(logo_image_10, x + Distand, y),
			  __FUNCTION__);
	if (world[y1][x1 + 4] == enemy10)
		checkDraw(tumDrawLoadedImage(logo_image_10, x + Distand * 2, y),
			  __FUNCTION__);
	if (world[y1][x1 + 6] == enemy10)
		checkDraw(tumDrawLoadedImage(logo_image_10, x + Distand * 3, y),
			  __FUNCTION__);
	if (world[y1][x1 + 8] == enemy10)
		checkDraw(tumDrawLoadedImage(logo_image_10, x + Distand * 4, y),
			  __FUNCTION__);
	if (world[y1][x1 + 10] == enemy10)
		checkDraw(tumDrawLoadedImage(logo_image_10, x + Distand * 5, y),
			  __FUNCTION__);
	if (world[y1][x1 + 12] == enemy10)
		checkDraw(tumDrawLoadedImage(logo_image_10, x + Distand * 6, y),
			  __FUNCTION__);
	if (world[y1][x1 + 14] == enemy10)
		checkDraw(tumDrawLoadedImage(logo_image_10, x + Distand * 7, y),
			  __FUNCTION__);
}



void Move_Player(void *pvParameters)
{
    	for (;;) {
		//Button count
		if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
			if (buttons.buttons[KEYCODE(RIGHT)]) {
				buttons.buttons[KEYCODE(RIGHT)] = 0;
				if (Position < 260)
                {
					Position += 26;
				player_PositionX += 26;
				
                player_X++;
                world[sizey-1][player_X+1]=player;
                world[sizey-1][player_X-2]=empty;
                }
                printf("player_matrix=%i\n", player_X);
                printf("player_Position=%i\n", player_PositionX);

			} else if (buttons.buttons[KEYCODE(LEFT)]) {
				buttons.buttons[KEYCODE(LEFT)] = 0;
				if (Position > -280){
					Position -= 26;
				player_PositionX -= 26;
				player_X--;
                world[sizey-1][player_X-1]=player;
                
                world[sizey-1][player_X+2]=empty;}
				printf("player_Position=%i\n", player_PositionX);
                printf("player_matrix=%i\n", player_X);
			}

			xSemaphoreGive(buttons.lock);
		}
	}
}

void vDrawBlock_1(int q, int p, int w, int h)
{
	q = q + 4 * w-10;
    int x1 = 4;
	int x2 = 5;
	// Draw the BLOCKs
	/*for (int j=sizey-5;j>=sizey-3;j--)
            {

                for(int k=0;k<4;k++)
                {
                    if(world[j][k+3]==block){
                    checkDraw(tumDrawFilledBox(
                                q+k*w, p+(j-(sizey-5))*h,
                                w, h,
                                Lime),
                            __FUNCTION__);
                    }
                }
            }*/  //funtioniert nicht
	//1.Block
	if (world[sizey - 5][x1] == block)
		checkDraw(tumDrawFilledBox(q, p, w, h, Lime), __FUNCTION__);
	if (world[sizey - 5][x2] == block)
		checkDraw(tumDrawFilledBox(q + w, p, w, h, Lime), __FUNCTION__);
	if (world[sizey - 4][x1] == block)
		checkDraw(tumDrawFilledBox(q, p + h, w, h, Lime), __FUNCTION__);
	if (world[sizey - 4][x2] == block)
		checkDraw(tumDrawFilledBox(q + w, p + h, w, h, Lime),
			  __FUNCTION__);
}

void vDrawBlock_2(int q, int p, int w, int h)
{
	q = q + 9 * w-5;
	int x1 = 9;
	int x2 = 10;
	//2.Block
	if (world[sizey - 5][x1] == block)
		checkDraw(tumDrawFilledBox(q, p, w, h, Lime), __FUNCTION__);
	if (world[sizey - 5][x2] == block)
		checkDraw(tumDrawFilledBox(q + w, p, w, h, Lime), __FUNCTION__);
	if (world[sizey - 4][x1] == block)
		checkDraw(tumDrawFilledBox(q, p + h, w, h, Lime), __FUNCTION__);
	if (world[sizey - 4][x2] == block)
		checkDraw(tumDrawFilledBox(q + w, p + h, w, h, Lime),
			  __FUNCTION__);
}

void vDrawBlock_3(int q, int p, int w, int h)
{
	q = q + 14 * w;
	int x1 = 14;
	int x2 = 15;
	//3.Block
	if (world[sizey - 5][x1] == block)
		checkDraw(tumDrawFilledBox(q, p, w, h, Lime), __FUNCTION__);
	if (world[sizey - 5][x2] == block)
		checkDraw(tumDrawFilledBox(q + w, p, w, h, Lime), __FUNCTION__);
	if (world[sizey - 4][x1] == block)
		checkDraw(tumDrawFilledBox(q, p + h, w, h, Lime), __FUNCTION__);
	if (world[sizey - 4][x2] == block)
		checkDraw(tumDrawFilledBox(q + w, p + h, w, h, Lime),
			  __FUNCTION__);
}
void vDrawBlock_4(int q, int p, int w, int h)
{
	q = q + 18* w+35;
	int x1 = 19;
	int x2 = 20;
	//4.Block
	if (world[sizey - 5][x1] == block)
		checkDraw(tumDrawFilledBox(q, p, w, h, Lime), __FUNCTION__);
	if (world[sizey - 5][x2] == block)
		checkDraw(tumDrawFilledBox(q + w, p, w, h, Lime), __FUNCTION__);
	if (world[sizey - 4][x1] == block)
		checkDraw(tumDrawFilledBox(q, p + h, w, h, Lime), __FUNCTION__);
	if (world[sizey - 4][x2] == block)
		checkDraw(tumDrawFilledBox(q + w, p + h, w, h, Lime),
			  __FUNCTION__);
}
void vDrawBlock_all(int q, int p, int w, int h)
{
	vDrawBlock_1(q, p, w, h);
	vDrawBlock_2(q, p, w, h);
	vDrawBlock_3(q, p, w, h);
	vDrawBlock_4(q, p, w, h);
}

void vDraw_Player(int p)
{
	int x = CAVE_SIZE_X-30+ p;
	int y = SCREEN_HEIGHT-130;

	tumDrawSetLoadedImageScale(logo_image_player, 0.22);
	checkDraw(tumDrawLoadedImage(logo_image_player, x, y), __FUNCTION__);
}
void vDraw_PlayerLife(int life)
{
	int x = 180;
	int y = SCREEN_HEIGHT-80;
	static char str[10] = { 0 };
	sprintf(str, "%d", life);
	tumFontSetSize((ssize_t)20);
	tumDrawSetLoadedImageScale(logo_image_player_1, 0.12);
	if (life > 1)
		checkDraw(tumDrawLoadedImage(logo_image_player_1, x, y),
			  __FUNCTION__);
	if (life > 2)
		checkDraw(tumDrawLoadedImage(logo_image_player_1, x + 45, y),
			  __FUNCTION__);

	checkDraw(tumDrawText(str, x - 20, y, White), __FUNCTION__);
}

void Gameover()
{
	static char str[10] = { 0 };
	sprintf(str, " Game Over!!! ");
	tumFontSetSize((ssize_t)40);

	checkDraw(tumDrawText(str, CAVE_SIZE_X-100, CAVE_Y, Red), __FUNCTION__);
	vTaskDelay(1000);
   
	xQueueSend(StateQueue, &next_state_signal_1, 0);
}

void vDraw_Mothership()
{
	int x = CAVE_SIZE_X-50;
	int y = SCREEN_HEIGHT - 350;

	tumDrawSetLoadedImageScale(logo_image_mothership, 0.2);
	checkDraw(tumDrawLoadedImage(logo_image_mothership, x, y),
		  __FUNCTION__);
}

void DrawText(int x, int y)
{
	static int text_width;
	tumFontSetSize((ssize_t)23);

	if (!tumGetTextSize((char *)str5, &text_width, NULL))
		checkDraw(tumDrawText((char *)str5, CAVE_X + x + 32, y + 170,
				      White),
			  __FUNCTION__);
	if (!tumGetTextSize((char *)str6, &text_width, NULL))

		checkDraw(tumDrawText((char *)str6, CAVE_X + x, y + 230, White),
			  __FUNCTION__);
	if (!tumGetTextSize((char *)str7, &text_width, NULL))
		checkDraw(tumDrawText((char *)str7, CAVE_X + x, y + 270, White),
			  __FUNCTION__);
	if (!tumGetTextSize((char *)str8, &text_width, NULL))
		checkDraw(tumDrawText((char *)str8, CAVE_X + x, y + 310, White),
			  __FUNCTION__);
}

void vDrawStaticItems(void)
{
	//vDrawHelpText();
	vDrawLogo();
}

void Move_text_menu(void *pvParamemter)
{
	static char str[50] = { 0 };
	sprintf(str, "INSERT COIN");
	//animation loop
	for (int i = 0; i <= strlen(str); i++) {
		str5[i] = str[i];
		vTaskDelay(xDelay);
	}

	sprintf(str, "< 1 OR 2 PLAYERS >");

	for (int i = 0; i <= strlen(str); i++) {
		str6[i] = str[i];
		vTaskDelay(xDelay);
	}
	sprintf(str, "* 1 PLAYER    1 COIN");
	for (int i = 0; i <= strlen(str); i++) {
		str7[i] = str[i];
		vTaskDelay(xDelay);
	}
	sprintf(str, "* 2 PLAYER    2 COINS");
	for (int i = 0; i <= strlen(str); i++) {
		str8[i] = str[i];
		vTaskDelay(xDelay);
	}
	vTaskSuspend(xtask_menu);
}

static int vCheckStateInput(void)
{
	if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
		if (buttons.buttons[KEYCODE(1)]) {
			button1 = 1;
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
			button1 = 0;
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
void DIFFICULT_SET(int Diffcult)
{
    if (Diffcult==1||cur_enemy<=30)
    xDelay=7;
    else if (Diffcult==2||cur_enemy<=22)
    xDelay=6;
    else if (Diffcult==3||cur_enemy<=15)
    xDelay=5;
}

static int vCheckStateInputM(void)
{
	if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
		if (buttons.buttons[KEYCODE(M)]) {
			buttons.buttons[KEYCODE(M)] = 0;
            Enemyfield();
           // score_1=0;
           // score_2=0;
            //vTaskSuspend(move_enemy_position);
       
			if (StateQueue) {
                
				xSemaphoreGive(buttons.lock);
				xQueueSend(StateQueue, &next_state_signal_1, 0);
				
			}

			
		}
		xSemaphoreGive(buttons.lock);
	}

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
    unsigned int score_1 = 0;
    unsigned int score_2 = 0;
   
void vDemoTask2(void *pvParameters)
{
		xTaskCreate(Move_text_all, "MOVE_TEXT_all", mainGENERIC_STACK_SIZE,
		    NULL, mainGENERIC_PRIORITY, &Text_task_all);

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
				DrawMoveText2();
				PlayText();

				Score_Title(score_1, score_2);
				// Draw FPS in lower right corner

				xSemaphoreGive(ScreenLock);

				// Get input and check for state change
				vCheckStateInputP();
			}
	}
}

void vDemoTask1(void *pvParameters)
{
	int y = 0;
	int x = 50;


	
    tumFontSetSize((ssize_t)23);

	xTaskCreate(Move_text_menu, "MOVE_TEXT_MEMU", mainGENERIC_STACK_SIZE,
		    NULL, mainGENERIC_PRIORITY, &xtask_menu);

	while (1) {
		if (DrawSignal)
			if (xSemaphoreTake(DrawSignal, portMAX_DELAY) ==
			    pdTRUE) {
				tumEventFetchEvents(FETCH_EVENT_BLOCK |
						    FETCH_EVENT_NO_GL_CHECK);

				xGetButtonInput(); // Update global button data
				xSemaphoreTake(ScreenLock, portMAX_DELAY);
				checkDraw(tumDrawClear(Black), __FUNCTION__);

				// Clear screen
				DrawText(x, y);
				Score_Title(score_1, score_2);
				xSemaphoreGive(ScreenLock);

				// Check for state change
				vCheckStateInput();
				vCheckStateInput2();
			}
	}
}

void CheckButton_INPUT3(void *pvParameters)
{
    for(;;)
        {
        xGetButtonInput();
        vCheckStateInput_bullet();
        vCheckStateInput_R();
       vCheckStateInputM();

        } 
}
void vDemoTask3(void *pvParameters)
{   unsigned int score_1 = 0;
    unsigned int score_2 = 0;
    unsigned int high_score = 0;
    unsigned char score_flag;
    char difficulty = 1; //1=easy 4=difficult
    unsigned int life= 3;
    unsigned char life_flag;
	Enemyfield();
    BaseType_t xReturn = pdPASS;
    taskENTER_CRITICAL();
	logo_image_player = tumDrawLoadImage(LOGO_player);
	logo_image_player_1 = tumDrawLoadImage(LOGO_player_1);
	int x = world_offsetX+20;
	int y = world_offsetY;
	int w = tumDrawGetLoadedImageWidth(logo_image_10) * 0.6;
	int h = tumDrawGetLoadedImageHeight(logo_image_10) * 0.6;

	xReturn=xTaskCreate(Move_enemy, "MOVE_enemy", mainGENERIC_STACK_SIZE, NULL,
		    mainGENERIC_PRIORITY, &enemy_Task);
            if(pdPASS== xReturn)
             printf("task_move_enemy created\n");
           
         
    xReturn=xTaskCreate(Enemy_Shoot,"Enemy_Shoot", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &EnemyShoot_task);
                if(pdPASS!= xReturn)
                vTaskResume(EnemyShoot_task);
				vTaskResume(enemy_Task);
				vTaskResume(enemy_position);
    xTaskCreate(CheckButton_INPUT3,"Check_INPUT3", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &Check_Button_Input);       
  xReturn=xTaskCreate(Kill_task_player, "Bullet_position", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &Check_kill_player);
	            if(pdPASS== xReturn)
                printf("task_Killed_enemy created\n");
    xReturn= xTaskCreate(Move_Player, "MOVE_Player", mainGENERIC_STACK_SIZE, NULL,
                        mainGENERIC_PRIORITY, &Player_Task);
          
    xReturn=xTaskCreate(move_enemy_position, "Enemy_Position", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY+1, &enemy_position );
            if(pdPASS== xReturn)
              printf("task_move_enemy_position created\n");
        
                 vTaskSuspend(Check_kill_player);

    while (1) {
		if (DrawSignal)
			if (xSemaphoreTake(DrawSignal, portMAX_DELAY) ==
			    pdTRUE) {
				tumEventFetchEvents(FETCH_EVENT_BLOCK |
						    FETCH_EVENT_NO_GL_CHECK);
				xSemaphoreTake(ScreenLock, portMAX_DELAY);

				
                checkDraw(tumDrawClear(Black), __FUNCTION__);
                xGetButtonInput();             
                if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
				 // Check for score updates
                    if (buttons.buttons[KEYCODE(D)]) {
                        xSemaphoreGive(buttons.lock);
                        difficulty = (difficulty + 1) % 4+1;
                        xQueueSend(DifficultyQueue, (void *) &difficulty, portMAX_DELAY);
                        vTaskDelay(200);
                    }
                  else {
                        xSemaphoreGive(buttons.lock);
                    }
                }
                if (LifeCountQueue) {
                    
                        while (xQueueReceive(
                                  LifeCountQueue,
                                   &life_flag,
                                   0) == pdTRUE) {
                            life--;
                        }
                        
                    }
                    
                    if (ScoreQueue30) {
                        while (xQueueReceive(
                                   ScoreQueue30,
                                   &score_flag,
                                   0) == pdTRUE) {
                            score_1+=30;
                        }
                    }
                    if (ScoreQueue20) {
                        while (xQueueReceive(
                                   ScoreQueue20,
                                   &score_flag,
                                   0) == pdTRUE) {
                            score_1+=20;
                        }
                        
                    }
                   if (ScoreQueue10) {
                        while (xQueueReceive(
                                   ScoreQueue10,
                                   &score_flag,
                                   0) == pdTRUE) {
                            score_1+=10;
                        }
                        
                    }  
                DIFFICULT_SET( difficulty); 
                Score_Title(score_1, score_2);
				vDrawHelpText();

				vDrawBlock_all(x+10, SCREEN_HEIGHT -200, w, h);

				vDrawenemy_30(x-5, y, speed, 0+enemy_move, d);
				vDrawenemy_20(x-5, y, speed, 0+enemy_move, d+1);
				vDrawenemy_20(x-5, y + h + 10, speed, 0+enemy_move, d+2);
				vDrawenemy_10(x-5 + 5, y + h + 10, speed, 0+enemy_move,d+ 3);
				vDrawenemy_10(x-5 + 5, y + 2 * h + 15, speed, 0+enemy_move,
					      d+4);
				vDraw_Player(Position);
                vDrawEnemyBullet();
				vDraw_Bullet();
				xSemaphoreGive(ScreenLock);
				
				
				vDraw_PlayerLife(life);
				
				 if(life==0||lowestEnemy==sizey-5) 
					    Gameover();
			}
	}    vTaskSuspend(EnemyShoot_task);
    vTaskSuspend(enemy_Task);
}

#define UDP_RECEIVE_PORT 1234
typedef enum { NONE = 0, INC = 1, DEC = -1 } opponent_cmd_t;
void UDPHandler(size_t read_size, char *buffer, void *args)
{
   opponent_cmd_t next_key = NONE;
    BaseType_t xHigherPriorityTaskWoken1 = pdFALSE;
    BaseType_t xHigherPriorityTaskWoken2 = pdFALSE;
    BaseType_t xHigherPriorityTaskWoken3 = pdFALSE;

    if (xSemaphoreTakeFromISR(HandleUDP, &xHigherPriorityTaskWoken1) ==
        pdTRUE) {

        char send_command = 0;
        if (strncmp(buffer, "INC", (read_size < 3) ? read_size : 3) ==
            0) {
            next_key = INC;
            send_command = 1;
        }
        else if (strncmp(buffer, "DEC",
                         (read_size < 3) ? read_size : 3) == 0) {
            next_key = DEC;
            send_command = 1;
        }
        else if (strncmp(buffer, "NONE",
                         (read_size < 4) ? read_size : 4) == 0) {
            next_key = NONE;
            send_command = 1;
        }

        if (NextKeyQueue && send_command) {
            xQueueSendFromISR(NextKeyQueue, (void *)&next_key,
                              &xHigherPriorityTaskWoken2);
        }
        xSemaphoreGiveFromISR(HandleUDP, &xHigherPriorityTaskWoken3);

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken1 |
                           xHigherPriorityTaskWoken2 |
                           xHigherPriorityTaskWoken3);
    }
    else {
        fprintf(stderr, "[ERROR] Overlapping UDPHandler call\n");
    }
}



//Computer Control
void vUDPControlTask(void *pvParameters)
{
    static char buf[50];
    char *addr = NULL; // Loopback
    in_port_t port = UDP_RECEIVE_PORT;
    unsigned int bullet_y = 0;
    unsigned int player_x = 0;
    char last_difficulty = -1;
    char difficulty = 1;

    udp_soc_receive =
        aIOOpenUDPSocket(addr, port, UDP_BUFFER_SIZE, UDPHandler, NULL);

   printf("UDP socket opened on port %d\n", port);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(15));
        while (xQueueReceive(BulletYQueue, &bullet_y, 0) == pdTRUE) {
        }
        while (xQueueReceive(PaddleXQueue, &player_x, 0) == pdTRUE) {
        }
        while (xQueueReceive(DifficultyQueue, &difficulty, 0) == pdTRUE) {
        }
        signed int diff = bullet_y - player_X;
        if (diff > 0) {
            sprintf(buf, "+%d", diff);
        }
        else {
            sprintf(buf, "-%d", -diff);
        }
        aIOSocketPut(UDP, NULL, UDP_TRANSMIT_PORT, buf,
                     strlen(buf));
        if (last_difficulty != difficulty) {
            sprintf(buf, "D%d", difficulty + 1);
            aIOSocketPut(UDP, NULL, UDP_TRANSMIT_PORT, buf,
                         strlen(buf));
            last_difficulty = difficulty;
        }
    }
}
void vIncrementPaddleX(unsigned short *paddle)
{
    if (paddle)
        if (*paddle != 0) {
            (*paddle)--;
        }
    if (paddle)
        if (*paddle != 0) {
            (*paddle)--;
        }
}

void vDecrementPaddleX(unsigned short *paddle)
{
    if (paddle)
        if (*paddle != PADDLE_INCREMENT_COUNT) {
            (*paddle)++;
        }
    if (paddle)
        if (*paddle != PADDLE_INCREMENT_COUNT) {
            (*paddle)++;
        }
}

	//Change to CheckPlayer Input
unsigned char xCheckPlayerInput(unsigned short *player_paddle_x)
{
    xGetButtonInput(); // Update global button data

    if (xSemaphoreTake(buttons.lock, portMAX_DELAY) == pdTRUE) {
        if (buttons.buttons[KEYCODE(RIGHT)]) {
            vIncrementPaddleX(player_paddle_x);
            xSemaphoreGive(buttons.lock);
            return 1;
        }
        if (buttons.buttons[KEYCODE(LEFT)]) {
            vDecrementPaddleX(player_paddle_x);
            xSemaphoreGive(buttons.lock);
            return 1;
        }
    }
    xSemaphoreGive(buttons.lock);
    return 0;
}
unsigned char xCheckMotherInput(unsigned short *player_paddle_x)
{
    xGetButtonInput(); // Update global button data

    if (xSemaphoreTake(buttons.lock, portMAX_DELAY) == pdTRUE) {
        if (buttons.buttons[KEYCODE(D)]) {
            vIncrementPaddleX(player_paddle_x);
            xSemaphoreGive(buttons.lock);
            return 1;
        }
        if (buttons.buttons[KEYCODE(A)]) {
            vDecrementPaddleX(player_paddle_x);
            xSemaphoreGive(buttons.lock);
            return 1;
        }
    }
    xSemaphoreGive(buttons.lock);
    return 0;
}

//Draw Player and Mothership
void vDrawWall_image(wall_t *wall, image_handle_t*logo_image)
{
   tumDrawSetLoadedImageScale(logo_image, 0.2);
	checkDraw(tumDrawLoadedImage(logo_image,wall-> x1,wall-> y1-20),
			  __FUNCTION__);
 tumDrawFilledBox(wall->x1, wall->y1, wall->w, wall->h, wall->colour);


}

void vDrawWall(wall_t *wall)
{
	tumDrawFilledBox(wall->x1, wall->y1, wall->w, wall->h, wall->colour);

}


void vDrawPaddle(wall_t *wall,image_handle_t*logo_image, unsigned short x_increment)
{
    // Set wall X
    setWallProperty(wall,
                    x_increment * PADDLE_INCREMENT_SIZE + GAME_FIELD_INNER +
                    2, 0,
                    0, 0, SET_WALL_X);
    // Draw wall
    vDrawWall_image(wall,logo_image);
}

typedef struct player_data {
    wall_t *paddle;
    unsigned short paddle_position;
} player_data_t;

void vResetPaddle(wall_t *wall)
{
    setWallProperty(wall,  PADDLE_INCREMENT_COUNT / 2,0, 0, 0, SET_WALL_Y);
}

void vPlayerWallCallback(void *player_data)
{	 
    // Reset ball's position and speed and increment left player's score
    const unsigned char point = 1;

    if (PlayerScoreQueue) {
        xQueueSend(PlayerScoreQueue, &point, portMAX_DELAY);
    }
	

    vResetPaddle(((player_data_t *)player_data)->paddle);

    xSemaphoreGive(BulletInactive);

    //xQueueOverwrite(StartDirectionQueue, &start_right);
}

unsigned char xCheckPongUDPInput(unsigned short *paddle_x)
{
    static opponent_cmd_t current_key = NONE;

    if (NextKeyQueue) {
        xQueueReceive(NextKeyQueue, &current_key, 0);
    }

    if (current_key == INC) {
        vDecrementPaddleX(paddle_x);
    }
    else if (current_key == DEC) {
        vIncrementPaddleX(paddle_x);
    }
    return 0;
}


void vWakePaddles(char opponent_mode)
{
    /*if (xTaskNotify(PlayerTask, opponent_mode,
                    eSetValueWithOverwrite) != pdPASS) {
        fprintf(stderr,
                "[ERROR] Task Notification to PlayerTask failed\n");
    }*/
    if (xTaskNotify(MothershipTask, 0x0, eSetValueWithOverwrite) !=
        pdPASS) {
        fprintf(stderr,
                "[ERROR] Task Notification to MothershipTask failed\n");
    }
}
unsigned char xCheckForInput(void)
{
	 if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
				 // Check for score updates
                    if (buttons.buttons[KEYCODE(SPACE)]) {
						buttons.buttons[KEYCODE(SPACE)] = 0;
                        xSemaphoreGive(buttons.lock);
						xSemaphoreGive(BulletInactive);
                    }
                  else {
                        xSemaphoreGive(buttons.lock);
                    }
                }
}

void vPlayerPaddleTask(void *pvParameters)
{	
	logo_image_player = tumDrawLoadImage(LOGO_player);	
    player_data_t player1 = { 0 };
    player1.paddle_position = PADDLE_INCREMENT_COUNT / 2;


    // Player paddle
    player1.paddle =
        createWall(PADDLE_START_LOCATION_X,SCREEN_HEIGHT - PADDLE_EDGE_OFFSET - PADDLE_HIGHT*3,
                    PADDLE_LENGTH-15, PADDLE_HIGHT/10,
                   0.1, White, &vPlayerWallCallback, &player1);

    PlayerScoreQueue = xQueueCreate(10, sizeof(unsigned char));

    while (1) {
        // Get input
      /*  if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) { // COMPUTER
            xCheckPongUDPInput(&player.paddle_position);
            unsigned long paddle_x = player.paddle_position *
                                     PADDLE_INCREMENT_SIZE +
                                     PADDLE_LENGTH / 2 +
                                     WALL_OFFSET + WALL_THICKNESS;
            xQueueSend(PaddleXQueue, (void *)&paddle_x, 0);
        }*/   
    // else {   // PLAYER
       xCheckPlayerInput(&player1.paddle_position);
							
    //    }

        taskENTER_CRITICAL();
        if (xSemaphoreTake(ScreenLock, 0) == pdTRUE) {
            
            vDrawPaddle(player1.paddle,
						logo_image_player,
                        player1.paddle_position
						);
        }
        xSemaphoreGive(ScreenLock);
        taskEXIT_CRITICAL();
    }
}

void vMothershipWallCallback(void *player_data)
{
    // Reset ball's position and speed and increment right player's score
   
    const unsigned char life =1;

   if(MothershipScoreQueue)
		xQueueSend(MothershipScoreQueue, &life, portMAX_DELAY);
	
    vResetPaddle(((player_data_t *)player_data)->paddle);

    xSemaphoreGive(BulletInactive);

    //xQueueOverwrite(StartDirectionQueue, &start_left);
}

void vMothershipPaddleTask(void *pvParameters)
{	
	
	logo_image_mothership = tumDrawLoadImage(LOGO_mothership);
    player_data_t Mothership = { 0 };
    Mothership.paddle_position = PADDLE_INCREMENT_COUNT / 2;


    // Left paddle
    Mothership.paddle = createWall(PADDLE_START_LOCATION_X,GAME_FIELD_INNER + PADDLE_EDGE_OFFSET*7,
                                     PADDLE_LENGTH-12,
                                    PADDLE_HIGHT/10, 0.1, White,  &vMothershipWallCallback,&Mothership);

   MothershipScoreQueue = xQueueCreate(10, sizeof(unsigned char));

    while (1) {
        // Get input
       if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) { // COMPUTER
            xCheckPongUDPInput(&Mothership.paddle_position);
            unsigned long paddle_x = Mothership.paddle_position *
                                     PADDLE_INCREMENT_SIZE +
                                     PADDLE_LENGTH / 2 +
                                     WALL_OFFSET + WALL_THICKNESS;
            xQueueSend(PaddleXQueue, (void *)&paddle_x, 0);

        }
     /*   else {   // PLAYER
            xCheckPongLeftInput(&Mothership.paddle_position);
        }*/

        taskENTER_CRITICAL();

        if (xSemaphoreTake(ScreenLock, 0) == pdTRUE) {
           
            vDrawPaddle(Mothership.paddle,
						logo_image_mothership,
                        Mothership.paddle_position
						);
        }

        xSemaphoreGive(ScreenLock);
        taskEXIT_CRITICAL();
    }
}

void vDemoTask4(void *pvParameters)
{	
	 //Random numbers
    srand(time(NULL));
    TickType_t xLastWakeTime, prevWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    prevWakeTime = xLastWakeTime;
    const TickType_t updatePeriod = 10;
    unsigned char score_flag;
	unsigned int pause=0;

    ball_t *my_ball = createBall(SCREEN_WIDTH / 2,SCREEN_HEIGHT/2-120, Lime,
                                 8, 250,NULL, NULL);

    unsigned char bullet_active = 0;
  //  unsigned char ball_direction = 0;
	unsigned int score_1 = 0;
	
    unsigned int score_2 = 0;

	unsigned int life= 3;
    unsigned char life_flag;
	unsigned char paddle_x;

  char opponent_mode = 0; // 0: player 1: computer
    char difficulty = 1; // 0: easy 1: normal 2: hard

	
    BulletInactive = xSemaphoreCreateBinary();
    if (!BulletInactive) {
        exit(EXIT_FAILURE);
    }
   HandleUDP = xSemaphoreCreateMutex();
    if (!HandleUDP) {
        exit(EXIT_FAILURE);
    }
   /*  StartDirectionQueue = xQueueCreate(1, sizeof(unsigned char));
    if (!StartDirectionQueue) {
        exit(EXIT_FAILURE);
    }*/
    NextKeyQueue = xQueueCreate(1, sizeof(opponent_cmd_t));
    if (!NextKeyQueue) {
        exit(EXIT_FAILURE);
    }
    BulletYQueue = xQueueCreate(5, sizeof(unsigned long));
    if (!BulletYQueue) {
        exit(EXIT_FAILURE);
    }
    PaddleXQueue = xQueueCreate(5, sizeof(unsigned long));
    if (!PaddleXQueue) {
        exit(EXIT_FAILURE);
    }
    DifficultyQueue = xQueueCreate(5, sizeof(unsigned char));
    if (!DifficultyQueue) {
        exit(EXIT_FAILURE);
    }
	logo_image_player_1 = tumDrawLoadImage(LOGO_player_1);
    setBallSpeed(my_ball, 0, 200, 0, SET_BALL_SPEED_AXES);
	//logo_image_player = tumDrawLoadImage(LOGO_player);
	//logo_image_mothership = tumDrawLoadImage(LOGO_mothership);
	
//	xTaskCreate(Move_Player, "MOVE_Player", mainGENERIC_STACK_SIZE, NULL,
//		    mainGENERIC_PRIORITY, &Player_Task);

	// Top wall
    wall_t *top_wall = createWall(GAME_FIELD_INNER, GAME_FIELD_OUTER,
                                  GAME_FIELD_WIDTH_INNER, WALL_THICKNESS/4,
                                  0.1, White, NULL, NULL);
	 // Bottom wall
    wall_t *bottom_wall = createWall(
                              GAME_FIELD_INNER, GAME_FIELD_INNER + GAME_FIELD_HEIGHT_INNER,
                              GAME_FIELD_WIDTH_INNER, WALL_THICKNESS/4, 0.1, White, NULL, NULL);
	
/*	xTaskCreate(vPlayerPaddleTask, "PlayerPaddleTask",
                    mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY,
                    &PlayerTask);
	
	xTaskCreate(vMothershipPaddleTask, "MothershipPaddleTask",
                    mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY,
                    &);
xTaskCreate(vUDPControlTask, "UDPControlTask",
                    mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY,
                    &UDPControlTask); */
	
	
	vTaskResume(PlayerTask);
	vTaskResume(MothershipTask);
	
	
	while (1) {
		if (DrawSignal)
			if (xSemaphoreTake(DrawSignal, portMAX_DELAY) ==
			    pdTRUE) {		
				tumEventFetchEvents(FETCH_EVENT_BLOCK |
						   		 FETCH_EVENT_NO_GL_CHECK);
					xSemaphoreTake(ScreenLock, portMAX_DELAY);
				vWakePaddles(opponent_mode);
				checkDraw(tumDrawClear(Black), __FUNCTION__);
					// Check if ball has made a collision
				checkBallCollisions(my_ball, NULL, NULL);

				// Update the balls position now that possible collisions have
				// updated its speeds
				updateBallPosition(my_ball, xLastWakeTime - prevWakeTime);
									unsigned long ball_y = my_ball->y;
									xQueueSend(BulletYQueue, (void *)&ball_y, 0);

									
									// Check for score updates
										if (PlayerScoreQueue) {
											while (xQueueReceive(
													PlayerScoreQueue,
													&score_flag,
													0) == pdTRUE) {
												score_2++;
											}
										}
									// Clear screen
									vDrawWall(top_wall);
									vDrawWall(bottom_wall);
									//vDraw_Player(Position);
									vDrawHelpText();
									Score_Title(score_1,  score_2);
									
									//vCheckStateInput_R();
									// Check for state change
									vCheckStateInputM();
									vDraw_PlayerLife(life);
									//vDraw_Mothership();
								xSemaphoreGive(ScreenLock);
									taskEXIT_CRITICAL();				
					//Get playerX for setBallX
				 while (xQueueReceive(PaddleXQueue, &paddle_x, 0) == pdTRUE) {
       				 }

                if (xSemaphoreTake(buttons.lock, 0) != pdTRUE) {
					xGetButtonInput(); // Update global button data
					if (buttons.buttons[KEYCODE(R)]) {
                        xSemaphoreGive(buttons.lock);
                        bullet_active = 0;
                        setBallLocation(
                            my_ball,
                            SCREEN_WIDTH / 2+paddle_x*PADDLE_INCREMENT_SIZE,
                            SCREEN_HEIGHT -120);
                        setBallSpeed(
                            my_ball, 0, 0, 0,
                            SET_BALL_SPEED_AXES);
                        score_2 = 0;
                        
                    }
					 if (buttons.buttons[KEYCODE(P)]) {
                        xSemaphoreGive(buttons.lock);
							if(!pause%2)
							{vTaskSuspend(PlayerTask);
							 vTaskSuspend(MothershipTask);
							setBallSpeed(
                            my_ball, 0, 0, 0,
                            SET_BALL_SPEED_AXES);
								pause++;
								}
							else
							{vTaskResume(PlayerTask);
							 vTaskResume(MothershipTask);
                         setBallSpeed( my_ball,
									0,200,
                                    0,
                                    SET_BALL_SPEED_AXES);
								pause++;
								}

                    }
                    xSemaphoreGive(buttons.lock);
                	}
				
				 // Bullet is no longer active
                if (xSemaphoreTake(BulletInactive, 0) == pdTRUE) {
                    bullet_active = 0;
                }

                if (!bullet_active) {
                    setBallLocation(my_ball,
                                    SCREEN_WIDTH / 2,
                                    SCREEN_HEIGHT -100);
                    setBallSpeed(my_ball, 0, 0, 0,
                                 SET_BALL_SPEED_AXES);

                 if (xCheckForInput()) {
					 bullet_active = 1;
                    setBallSpeed( my_ball,
									0,200,
                                    0,
                                    SET_BALL_SPEED_AXES);
					
                    
               
                       /* xQueueReceive(
                            StartDirectionQueue,
                            &ball_direction, 0);
                        
                        switch (ball_direction) {
                            case START_LEFT:
                                setBallSpeed(
                                    my_ball,
                                    -(rand() % 100 +
                                      200),
                                    ((rand() % 2) *
                                     2 -
                                     1) * (100 +
                                           (rand() %
                                            200)),
                                    0,
                                    SET_BALL_SPEED_AXES);
                                break;
                            default:
                            case START_RIGHT:
                                setBallSpeed(
                                    my_ball,
                                    rand() % 100 +
                                    200,
                                    ((rand() % 2) *
                                     2 -
                                     1) * (100 +
                                           (rand() %
                                            200)),
                                    0,
                                    SET_BALL_SPEED_AXES);
                                break;*/
				 }
				}
							// Draw the ball
                      tumDrawCircle(my_ball->x,
                                    my_ball->y,
                                    my_ball->radius,
                                    my_ball->colour);              
			  
			 if (MothershipScoreQueue) {
                    
                        while (xQueueReceive(
                                  MothershipScoreQueue,
                                   &life_flag,
                                   0) == pdTRUE) {
                            life--;
                        }
                        
                    }	

                // Keep track of when task last ran so that you know how many ticks
                //(in our case miliseconds) have passed so that the balls position
                // can be updated appropriatley
                prevWakeTime = xLastWakeTime;
                vTaskDelayUntil(&xLastWakeTime, updatePeriod);	
			if(life==0)
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
	//tumFontLoadFont(FPS_FONT, DEFAULT_FONT_SIZE);

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

	//2 Player Tasks
	    if (xTaskCreate(vPlayerPaddleTask, "PlayerPaddleTask",
                    mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY,
                    &PlayerTask) != pdPASS) {
        PRINT_TASK_ERROR("PlayerPaddleTask");
        goto err_playerpaddle;
    }
    if (xTaskCreate(vMothershipPaddleTask, "MothershipPaddleTask",
                    mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY,
                    &MothershipTask) != pdPASS) {
        PRINT_TASK_ERROR("MotershipPaddleTask");
        goto err_mothershippaddle;
    }
/*	    if (xTaskCreate(Move_enemy, "MOVE_enemy", mainGENERIC_STACK_SIZE, NULL,
		    mainGENERIC_PRIORITY, &enemy_Task) != pdPASS) {
        PRINT_TASK_ERROR("MOVE_enemy");
        goto err_move_enemy;
    }
		    if (xTaskCreate(Move_enemy, "MOVE_enemy", mainGENERIC_STACK_SIZE, NULL,
		    mainGENERIC_PRIORITY, &enemy_Task) != pdPASS) {
        PRINT_TASK_ERROR("MOVE_enemy");
        goto err_move_enemy;
    }
			 if (xTaskCreate(Enemy_Shoot,"Enemy_Shoot", mainGENERIC_STACK_SIZE, NULL,
			  mainGENERIC_PRIORITY, &EnemyShoot_task) != pdPASS) {
        PRINT_TASK_ERROR("Enemy_Shoot");
        goto err_enemyshoot;
    }
		if (xTaskCreate(move_enemy_position, "Enemy_Position", mainGENERIC_STACK_SIZE, NULL, 
		mainGENERIC_PRIORITY+1, &enemy_position ) != pdPASS) {
        PRINT_TASK_ERROR("Enemy_Position");
        goto err_enemyposition;
    }


    if (xTaskCreate(vUDPControlTask, "UDPControlTask",
                    mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY,
                    &UDPControlTask) != pdPASS) {
        PRINT_TASK_ERROR("UDPControlTask");
        goto err_udpcontrol; 
    }*/

	vTaskSuspend(DemoTask1);
	vTaskSuspend(DemoTask2);
	vTaskSuspend(DemoTask3);
	vTaskSuspend(DemoTask4);
	vTaskSuspend(PlayerTask);
	vTaskSuspend(MothershipTask);
	
   
	tumFUtilPrintTaskStateList();

	vTaskStartScheduler();

	return EXIT_SUCCESS;
/*err_udpcontrol:
    vTaskDelete(UDPControlTask);
err_enemyposition:
	vTaskDelete(enemy_position);
err_enemyshoot:
	vTaskDelete(EnemyShoot_task);
err_move_enemy:
	vTaskDelete(enemy_Task);*/
err_playerpaddle:
    vTaskSuspend(PlayerTask);
err_mothershippaddle:
    vTaskDelete(MothershipTask);
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
