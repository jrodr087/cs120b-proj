/*
 * lcd.c
 *
 * Created: 2/25/2018 1:41:30 PM
 * Author : Pete <= THATS MY DESKTOPS NAME
	ILL UPLOAD THIS TO GITHUB L?A?T?E?R?	 NEVER
 */ 

#include <avr/io.h>
#include <util/delay.h>
#include "timer.h"
#define dispy 6 //how many rows of chars
#define dispx 84 //chars per row
#define sprpery 14 //sprites per y
#define sprperx 6 //sprites per x
#define gravity 1 //negative value of grav would make the sprite accelerate up 
#define termVel 1 //falling any faster than one sprite per tick would be too fast
#define shotAccel -3 // how fast shooting will accelerate the player
#define jumpVel -3 //the velocity that jumping puts the player at
#define shotReload 6 //how long before you can shoot again, long reload means net height loss, intended
#define leftbutton (~PIND & 0x40)
#define rightbutton (~PIND & 0x20)
#define jumpbutton (~PIND & 0x10)
#define maxammo 4
#define maxHealth 4
#define maxcombo 15
#define sceneCnt 3
#define camYoffset 2
#define aiSpeed 9 //how many times a player can move before the ai does
unsigned char playerXPos = 0; // current position of the player on the x axis
unsigned char playerYPos = 0; // currednt position on the y axis
signed char playerYVel = termVel; // how fast the player falls
unsigned char playerScn = 0;
unsigned char playerReload = 0; // if zero we can shoot
unsigned char playerAmmo = maxammo;
unsigned char playerHealth = maxHealth;
unsigned char playerCombo = 0;
//unsigned char camPos = 0;
//unsigned char scenePos = 0;

///MUCH OF THIS SHOULD BE PUT INTO HEADERS AND SUCH VVVVV
void tickLCDClk(){//ticks the CLK pin on the LCD
	PORTB = PORTB | 0x01;
	//unsigned short countrst = 10;
	//while (countrst){countrst--;}
	PORTB = PORTB & 0xFE;
}
void setCE(unsigned char val){//just turns on LCD
	PORTB = PORTB & 0xF7;
	PORTB = PORTB | (val << 3);
//	tickLCDClk();
}
void sendData(unsigned char data){
	setCE(0);//turn on hcip
	for (unsigned char i = 7; i > 0; i--){
		unsigned char tosend = (data & (0x01 << i)) >> i;
		PORTB = PORTB & 0xFD;
		PORTB = PORTB | (tosend << 1);
		tickLCDClk();
	}
	unsigned char tosend = (data & 0x01);
	PORTB = PORTB & 0xFD;
	PORTB = PORTB | (tosend << 1);
	tickLCDClk();
	PORTB = PORTB & 0xFD;
	setCE(1);//turn the chip off so we dont accidentally write anything
}
void setDC(unsigned char val){//obvious
	PORTB = PORTB & 0xFB;
	PORTB = PORTB | (val << 2);
	//tickLCDClk();
}
void setRESET(unsigned char val){//obvious
	PORTB = PORTB & 0xEF;
	PORTB = PORTB | (val << 4);
	//tickLCDClk();
}
void clearScreen(){//obvious
	for (int y = 0; y <= 5; y++)
	for (int x = 0; x <= 83; x++){
		sendData(0x00);
	}
}
//takes in an array of unsigned chars to be sent to LCD to draw
void drawScene(unsigned char scene[][dispy][dispx]){//pointer to a 2d array
	setDC(0);
	sendData(0x40); //sets Y to 0
	sendData(0x80); //sets X to 0
	setDC(1);
	for (unsigned char y = 0; y < dispy; y++)
	for (unsigned char x = 0; x < dispx; x++){//incoming deudly pointer arithmetic
		sendData(*(*((*scene)+y)+x));//the things i do to save memory
	}
}
void initLCD(){//runs at the start of the program to turn on and initialize the LCD
	setRESET(1);
	setCE(1);
	_delay_ms(100);
	setRESET(0);
	_delay_ms(100);
	setRESET(1);
	setCE(0);
	//init lcd
	sendData(0x21); //powers on LCD, extended set instructions, and horizontal addressing
	sendData(0x13); //bias voltage????
	sendData(0x06); //dont know what a temperature coeffecient is so im guessing
	sendData(0xC0); //setting Vlcd thankfully the datasheet has the value for 5V
	sendData(0x20); //turns off extended instructions
	sendData(0x09); //screen normal mode
	sendData(0x40); //sets Y to 0
	sendData(0x80); //sets X to 0
	sendData(0x0C);//sets screen type to yes
	setDC(1); //lets draw
	clearScreen(); // clear the screen
	setDC(0); // done drawin
	sendData(0x40); //sets Y to 0
	sendData(0x80); //sets X to 0
	setDC(1);//turn on data mode so we can draw later
}
enum pixState{off = 0, on = 1};//basically a boolean, just banking on compiler optimizations or whatever so each pixState isnt a whole char
enum pixState canJump = on;
enum pixState leftEdge = off;
enum pixState rightEdge = off;
enum pixState jumpEdge = off;
enum spriteType{player, tile,brokentile,lballoon, rballoon, ghost,air};//types of different sprites, all the same size
enum actorState{na, wait, move, moved}; //"actors" are any tiles with rudimentary AI, such as the balloons. non-"actors" such as air or tiles get the na state, as in n/a, not available
enum ButtonState{notpressed, check1, check2, pressed};
enum ButtonState leftCheck = notpressed;
enum ButtonState rightCheck = notpressed;
enum ButtonState jumpCheck = notpressed;
typedef struct sprite{ //a scene can contain 6 sprites on the "x" (y) axis and 14 on the "y" (x) axis
		enum pixState image[8][6];// which means a width of 8 and height of 6 for each sprite
			
} sprite;
typedef struct spriteScene{//just an array of sprites basipally
	sprite picture[sprperx][sprpery];
	enum spriteType sprites[sprperx][sprpery];
	enum actorState states[sprperx][sprpery];
	
} spriteScene;
void GetSprite(enum spriteType type, volatile sprite* val){//assigning spirte values n stuff
	for (unsigned char x = 0; x < 8; x++){
		for (unsigned char y = 0; y < 6; y++){
			val->image[x][y] = off;
		}
	}
		switch (type){
			case player:
			for (unsigned char x = 1; x < 3; x++){
				for (unsigned char y = 0; y < 6; y++){
					val->image[x][y] = on;
				}	
			}
			for (unsigned char x = 3; x < 5; x++){
				for (unsigned char y = 2; y < 6; y++){
					val->image[x][y] = on;
				}
			}
			for (unsigned char x = 5; x < 7; x++){
				for (unsigned char y = 0; y < 6; y++){
					val->image[x][y] = on;
				}
			}
				break;
			case brokentile:
				for (unsigned char x = 0; x < 8; x++){
					for (unsigned char y = 0; y < 6; y++){
						val->image[x][y] = on;
					}
				}
				val->image[7][3] = off;
				val->image[6][2] = off;
				val->image[5][3] = off;
				val->image[4][4] = off;
				val->image[4][2] = off;
				val->image[4][0] = off;
				val->image[3][5] = off;
				val->image[3][1] = off;
				val->image[2][4] = off;
				val->image[2][0] = off;
				val->image[1][3] = off;
				val->image[0][3] = off;
				break;
			case tile:
				for (unsigned char x = 0; x < 8; x++){
					for (unsigned char y = 0; y < 6; y++){
						val->image[x][y] = on;
					}
				}
				break;
			case rballoon://omitting the break here to fall through to lballoon since sprites are identical
			case lballoon:
				for (unsigned char x = 0; x < 8; x++){
					for (unsigned char y = 0; y < 6; y++){
						val->image[x][y] = on;
					}
				}
				val->image[7][5] = off;
				val->image[7][1] = off;
				val->image[6][4] = off;
				val->image[6][3] = off;
				val->image[6][0] = off;
				val->image[5][4] = off;
				val->image[5][3] = off;
				val->image[5][1] = off;
				val->image[4][4] = off;
				val->image[4][3] = off;
				val->image[4][0] = off;
				val->image[3][4] = off;
				val->image[3][3] = off;
				val->image[3][0] = off;
				val->image[2][4] = off;
				val->image[2][3] = off;
				val->image[2][1] = off;
				val->image[1][4] = off;
				val->image[1][3] = off;
				val->image[1][0] = off;
				val->image[0][5] = off;
				val->image[0][1] = off;
				break;
			case ghost: // i accientally put all the sprite in upsidedown, thats why 5-i
				for (unsigned char i = 1; i < 6; i++){val->image[7][5-i] = on;}
				for (unsigned char i = 0; i < 2; i++){val->image[6][5-i] = on;}
				for (unsigned char i = 3; i < 6; i++){val->image[6][5-i] = on;}
				val->image[5][5] = on;
				for (unsigned char i = 3; i < 6; i++){val->image[5][5-i] = on;}
				for (unsigned char i = 0; i < 4; i++){val->image[4][5-i] = on;}
				val->image[4][0] = on;
				for (unsigned char i = 0; i < 4; i++){val->image[3][5-i] = on;}
				val->image[3][0] = on;
				val->image[2][5] = on;
				for (unsigned char i = 3; i < 6; i++){val->image[2][5-i] = on;}
				for (unsigned char i = 0; i < 2; i++){val->image[1][5-i] = on;}
				for (unsigned char i = 3; i < 6; i++){val->image[1][5-i] = on;}
				for (unsigned char i = 1; i < 6; i++){val->image[0][5-i] = on;}	
				break;
			default:
			break;
			
	}
}
void makeScene(spriteScene *scn){//takes in a scene of sprite types and makes the unsigned char array to be made into the display
	//have to somehow take the spritescene, flip it, and make a multidim array for drawing
	unsigned char arr[dispy][dispx];
	//clearScreen();
	unsigned char arrx = 0;
	unsigned char arry = 0;
	unsigned char spx = 0;
	unsigned char spy = 13; // this stands for sprite y not spy
	while (spx < 6){//x is actually y on the screen
			for (unsigned char x = 0; x < 6; x++){//this is where i regret doing everything sideways
				unsigned char val = 0x00;
				for (unsigned char y = 0; y < 8; y++){	//uhhhh 8 because sprite width of 8
					unsigned char pixel = scn->picture[spx][spy].image[y][x];//look at those array accesses christ
					val = val | (pixel << y); 
				}
				arr[arry][arrx] = ~val; //really confusing how these ended up backways
				arrx++;
			}
			if (arrx >= 84){arrx = 0; arry++;}//its ok if i leave this out of the for loop since 6 evenly divides 84 i think
			spy--;
			if (spy > 14){ // *scream*
				spy = 13;
				spx++;
			}
	}///thank god its over
	drawScene(&arr);//passing it in with a pointer because >>copying 504 bytes >>ishygddt
}
void drawSweep(){//this is just a cool sweep to draw, good for a transition maybe

	setDC(0);
	sendData(0x40); //sets Y to 0
	sendData(0x80); //sets X to 0
	setDC(1);
	for (unsigned char y = 0; y < 6; y++){
		for (unsigned char x = 0; x <= 28; x++){
			sendData(0x18);
			_delay_ms(10);
			sendData(0x3C);
			_delay_ms(10);
			sendData(0x7E);
			_delay_ms(10);
			sendData(0xFF);
			_delay_ms(10);
			sendData(0xFF);
			_delay_ms(10);
			sendData(0x7E);
			_delay_ms(10);
			sendData(0x3C);
			_delay_ms(10);
			sendData(0x18);
			_delay_ms(10);
		}
	}
	setDC(0);
	sendData(0x40); //sets Y to 0
	sendData(0x80); //sets X to 0
	setDC(1);
	for (unsigned char y = 0; y < 6; y++){
		for (unsigned char x = 0; x <= 28; x++){
			sendData(~(0x18));
			_delay_ms(10);
			sendData(~(0x3C));
			_delay_ms(10);
			sendData(~(0x7E));
			_delay_ms(10);
			sendData(0x00);
			_delay_ms(10);
			sendData(0x00);
			_delay_ms(10);
			sendData(~(0x7E));
			_delay_ms(10);
			sendData(~(0x3C));
			_delay_ms(10);
			sendData(~(0x18));
			_delay_ms(10);
		}
	}
}
void SetSprites(spriteScene scn[][3]){ //given an array of sprite types and a scene, set the appropriate sprites to the scene
	unsigned char curY;
	unsigned char currScn = playerScn;
	if (playerYPos < camYoffset){
		curY = sprpery-(camYoffset-playerYPos);
		if (playerScn){currScn--;}
		else {currScn = sceneCnt-1;}
	}
	else {curY = playerYPos-camYoffset;}
	for (unsigned char y = 0; y < sprpery; y++){
		for (unsigned char x = 0; x < sprperx; x++){
			if (x == playerXPos && curY == playerYPos && playerHealth){
				GetSprite(player,&((*scn+playerScn)->picture[x][y]));
			}
			else{
				GetSprite((*scn + currScn)->sprites[x][curY],&((*scn+playerScn)->picture[x][y]));
			}
		}
		if (curY == sprpery-1){
			curY = 0;
			if (currScn == sceneCnt-1){currScn = 0;}
			else {currScn++;}
		}
		else {curY++;}
	}
}
void GetScene(spriteScene *scn){//here;s where i manually craft bits of the level to be randomly picked and cobbled together during gameplay
	unsigned char lvlidx = 0;
	switch (lvlidx){
		case 0://this sucks
			lvlidx++;
			/*enum spriteType lvl[sprperx][sprpery] = {
				{air,air,air,tile,brokentile,rballoon,air,air,air,tile,air,air,air,air},
				{air,air,air,tile,air,air,air,air,air,tile,air,air,air,air},
				{air,air,air,brokentile,air,air,air,air,air,air,air,air,air,air},
				{air,air,air,air,air,air,air,air,air,tile,air,air,air,air},
				{air,air,air,brokentile,air,air,air,air,air,tile,air,air,air,air},
				{air,air,air,tile,tile,air,air,air,air,tile,air,air,air,air}
			};*/
			enum spriteType lvl[sprperx][sprpery] = {
				{air,air,air,tile,brokentile,rballoon,air,lballoon,air,tile,air,air,air,air},
				{air,air,air,tile,air,air,air,air,air,tile,air,air,air,air},
				{air,air,air,brokentile,air,air,air,air,air,air,air,air,air,air},
				{air,air,air,air,air,air,lballoon,air,air,tile,lballoon,air,air,air},
				{air,air,air,brokentile,air,air,rballoon,air,air,tile,air,air,air,air},
				{air,air,tile,tile,tile,air,air,tile,tile,tile,air,air,air,air}
			};
			for (unsigned char x = 0; x < sprperx; x++)
			for (unsigned char y = 0; y < sprpery; y++){
				scn->sprites[x][y] = lvl[x][y];
			}
			///SetSprites(scn);
			///////////////////////////VVVVVVVVVV PUT THIS IN A FUNCTION BEFORE MAKING MORE LEVEL LAYOUTS MORON VVVVVVV/////////////////////
			for (unsigned char x = 0; x < sprperx; x++)
			for (unsigned char y = 0; y < sprpery; y++){
				if (lvl[x][y] == lballoon || lvl[x][y] == rballoon || lvl[x][y] == ghost){ //gives these boys their proper actor states
					scn->states[x][y] = wait;
				}
				else{scn->states[x][y] = na;}//otherwise they're a nobody
			}
			break;
	}
}
//////////////////////////////////////////////////////////////////////////////V COME HERE LATER FOR HIT DETECTION MORON!!!!!!!! V//////////////////////////////////////////////////////////////////////////////////////////////
void UpdateActors(enum spriteType scn[][sprperx][sprpery], enum actorState states[][sprperx][sprpery], unsigned char scnNum){ //takes a scene and updates based on status array and is also where i wish the sprite array was included in the scene UPDATE: Did it
//	enum pixState balloonChange = off; //bootleg bool
	for (unsigned char x = 0; x < sprperx; x++)
		for (unsigned char y = 0; y < sprpery; y++){
			enum spriteType *curr = (*(*(scn)+x)+y);
			enum actorState *currState = (*(*(states) + x)+y);
			switch (*currState){
				case na:
				break;
				case moved:
					*currState = wait;
					break;
				case wait:
					*currState = move;
					break;
				
				case move://incoming AI logic here (nested switch)
					// THE BIOG PROBLEM with this aI:
					/* a line of actors trying to move left right-aligned will move as one but a line of actors trying to move 
					right left-aligned will move one at a time																*/
					switch (*curr){//uggggnnn this function too big already
						case lballoon: // THE DIFFERENCE between lballoons and rballoons is that lballoons will always move left if possible and opposite for rballons
							if (!x){// if already at the leftmost part of screen
								if (x+1 == playerXPos && y == playerYPos && playerHealth && playerScn == scnNum){//if player to right hurt he
									playerHealth--;
									*curr = air;
									*currState = na;
								}
								else if (*(*(*(scn)+x+1)+y)== air){//if notthing to the right move to the right
									*curr = air;
									*currState = na;
									*(*(*(scn)+x+1)+y) = rballoon;
									*(*(*(states) + x+1)+y) = moved;
								}
								else{}//else begrudgingly float in place
							}	
							else{
								if (x-1 == playerXPos && y == playerYPos && playerHealth && playerScn == scnNum){//if player to left hurt he
									playerHealth--;
									*curr = air;
									*currState = na;
								}
								else if (*(*(*(scn)+x-1)+y) == air){ // if nothing to the left move to the left ok
									*curr = air;
									*currState = na;
									*(*(*(scn)+x-1)+y) = lballoon;
									*(*(*(states) + x-1)+y) = moved;
								}
								else if (x+1 == playerXPos && y == playerYPos && playerHealth && playerScn == scnNum){//if player to right hurt he
									playerHealth--;
									*curr = air;
									*currState = na;
								}
								else if (*(*(*(scn)+x+1)+y) == air){//unbelievable how i just copy pasted that huh. anyway move right otherwise if possible
									*curr = air;
									*currState = na;
									*(*(*(scn)+x+1)+y) = rballoon;
									*(*(*(states)+x+1)+y) = moved;
								}
								else{}//else begrudgingly copypaste
							}
							break;
						case rballoon://back at it again copy pasting. anyway right balloon move ai
						if (x == (sprperx-1)){// if already at the rightmost part of screen
							if (x+1 == playerXPos && y == playerYPos && playerHealth && playerScn == scnNum){//if player to left hurt he
								playerHealth--;
								*curr = air;
								*currState = na;
							}
							else if (*(*(*(scn)+x-1)+y)== air){//if notthing to the left move to the left
								*curr = air;
								*currState = na;
								*(*(*(scn)+x-1)+y) = lballoon;
								*(*(*(states) + x-1)+y) = moved;
								break;
							}
							else{}//else begrudgingly float in place
						}
						else{
							if (x+1 == playerXPos && y == playerYPos && playerHealth && playerScn == scnNum){//if player to right hurt he
								playerHealth--;
								*curr = air;
								*currState = na;
							}
							else if (*(*(*(scn)+x+1)+y) == air){ // if nothing to the right move to the right ok
								*curr = air;
								*currState = na;
								*(*(*(scn)+x+1)+y) = rballoon;
								*(*(*(states)+x+1)+y) = moved;
								break;
							}
							else if (x-1 == playerXPos && y == playerYPos && playerHealth && playerScn == scnNum){//if player to left hurt he
								playerHealth--;
								*curr = air;
								*currState = na;
							}
							else if (*(*(*(scn)+x-1)+y) == air){//unbelievable how i just copy pasted that AGAIN huh. anyway move left otherwise if possible
								*curr = air;
								*currState = na;
								*(*(*(scn)+x-1)+y) = lballoon;
								*(*(*(states) + x-1)+y) = moved;
								break;
							}
							else{}//else begrudgingly copypaste
						}
						break;
						default: //never should ocme here
							break;
					}//end of AI switch
					break;//breaks out of the actors state switch		
			}//end of big switch
		}//wondering where my copy paste error is in here we'll see
		
		//since this moves left to right and up to down that means that movements such as an lballoon moving left must have their state updated once more, will do next
		for (unsigned char x = 0; x < sprperx; x++){
			for (unsigned char y = 0; y < sprpery; y++){
				enum actorState *temp = (*(*(states) + x)+y);
				if (*temp == moved){
					*temp = wait;	
				}
			}
		}
}
void UpdatePlayer(spriteScene* scn, spriteScene scnArr[][sceneCnt]){//check input and update player
	if (leftEdge && !rightEdge){
		if (playerXPos){
			if (scn->sprites[playerXPos-1][playerYPos] != tile && scn->sprites[playerXPos -1][playerYPos] != brokentile){
				playerXPos--;
				if (scn->sprites[playerXPos][playerYPos] == lballoon || scn->sprites[playerXPos][playerYPos] == rballoon || scn->sprites[playerXPos][playerYPos] == ghost){
					scn->sprites[playerXPos][playerYPos] = air;
					scn->states[playerXPos][playerYPos] = na;
					playerHealth--;
				}
			}
		}
	}
	else if (!leftEdge && rightEdge){
		if (playerXPos < sprperx-1){
			if (scn->sprites[playerXPos+1][playerYPos] != tile && scn->sprites[playerXPos+1][playerYPos] != brokentile){
				playerXPos++;
				if (scn->sprites[playerXPos][playerYPos] == lballoon || scn->sprites[playerXPos][playerYPos] == rballoon || scn->sprites[playerXPos][playerYPos] == ghost){
					scn->sprites[playerXPos][playerYPos] = air;
					scn->states[playerXPos][playerYPos] = na;
					playerHealth--;
				}
			}
		}
	}
	if (jumpEdge){
		if (canJump){
			playerYVel = jumpVel;
			canJump = off;	
		}
	}
	reloopMov:
	if (playerYVel < termVel){playerYVel++;}
	signed char vel = playerYVel;
	unsigned char sceneCurr = playerScn;
	signed char tempYpos = playerYPos;
	while (vel < 0)	{
		vel++;
		tempYpos--;
		if (tempYpos < 0){
			if (sceneCurr == 0){
				sceneCurr = sceneCnt-1;
			}
			else {sceneCurr--;}
			tempYpos = sprpery-1;
		}
		enum spriteType at = (*scnArr+sceneCurr)->sprites[playerXPos][tempYpos];
		if (at == tile || at == brokentile){
			if (tempYpos == sprpery-1){
				if (sceneCurr == sceneCnt-1){
					sceneCurr = 0;
				}
				else{sceneCurr++;}
				tempYpos = 0;
			}
			else {tempYpos++;}
			playerYPos = tempYpos;
			playerYVel = 0;
			break;
		}
		if (at == lballoon || at == rballoon || at == ghost){
			(*scnArr+sceneCurr)->sprites[playerXPos][tempYpos] = air;
			(*scnArr+sceneCurr)->states[playerXPos][tempYpos] = na;
			playerHealth--;
		}
	}
	while (vel > 0){
		vel--;
		tempYpos++;
		if (tempYpos >= sprpery){
			if (sceneCurr == sceneCnt-1){
				sceneCurr = 0;
			}
			else {sceneCurr++;}
			tempYpos = 0;
		}
		enum spriteType at = (*scnArr+sceneCurr)->sprites[playerXPos][tempYpos];
		canJump = off;
		if (at == tile || at == brokentile){
			if (tempYpos == 0){
				if (sceneCurr == 0){
					sceneCurr = sceneCnt-1;
				}
				else{sceneCurr--;}
				tempYpos = sprpery-1;
			}
			else{tempYpos--;}
			playerYPos = tempYpos;
			playerYVel = 0;
			canJump = on;
			break;
		}
		else if (at == lballoon || at == rballoon){
			(*scnArr+sceneCurr)->sprites[playerXPos][tempYpos] = air;
			(*scnArr+sceneCurr)->states[playerXPos][tempYpos] = na;
			playerAmmo = maxammo;
			playerYVel = jumpVel+1; //since we passed the playeryvell++ already
			if (playerCombo < maxcombo){
				playerCombo++;
			}
			goto reloopMov;
		}
		else if (at == ghost){
			scn->sprites[playerXPos][tempYpos] = air;
			scn->states[playerXPos][tempYpos] = na;
			playerHealth--;
		}
	}
	if (sceneCurr != playerScn){
		if (playerYVel > 0){
			switch (sceneCurr){
				case 0:
					GetScene(((*scnArr) + 1));
					break;
				case 1:
					GetScene(((*scnArr) + 2));
					break;
				case 2:
					GetScene(((*scnArr)));
					break;
				}
		}
		playerScn = sceneCurr;
	}
	playerYPos = tempYpos;
	unsigned char mask = 0x00;
	switch (playerAmmo){
		case 0:
			break;
		case 1:
			mask |= 0x01;
			break;
		case 2:
			mask |= 0x03;
			break;
		case 3:
			mask |= 0x07;
			break;
		case 4:
			mask |= 0x0F;
			break;
		default:
			break;
	}
	switch (playerHealth){
		case 0:
			break;
		case 1:
			mask |= 0x80;
			break;
		case 2:
			mask |= 0xC0;
			break;
		case 3:
			mask |= 0xE0;
			break;
		case 4:
			mask |= 0xF0;
			break;
		default:
			break;
	}
	PORTA = mask;
}
void CheckButtonState(enum pixState *edge, enum ButtonState *state, unsigned char button){
	//transitions
	switch (*state){
		case notpressed:
			if (button){
				*state = check1;	
			}
			else {*state = notpressed;}
			break;
		case check1:
			if (button){
				*state = pressed;
				*edge = on;
			}
			else {*state = notpressed;}
			break;
		case check2:
			if (button){
				*state = pressed;
				*edge = on;
			}
			else {*state = notpressed;}
			break;
		case pressed:
			/*if (button){
				*state = pressed;
			//	*edge = off;
			}
			else {
				*state = notpressed;
				//edge = off;
			}*/
			break;
		default:
			break;
	}
}
void UpdateButtons(){
		CheckButtonState(&rightEdge, &rightCheck, rightbutton);
		CheckButtonState(&leftEdge, &leftCheck, leftbutton);
		CheckButtonState(&jumpEdge, &jumpCheck, jumpbutton);
}

///MUCH OF THIS SHOULD BE PUT INTO HEADERS AND SUCH ^^^^^^
//ALSO MANY OF THE WEIRD ARGUMENTS TO THE FUNCTIONS ONLY SEEM WEIRD BECAUSE OF LATER REFACTORS. UPDATE: NVM FIXED
int main(void)
{
	//0x10 RESET
	//0x08 CE
	//0x04 DC
	//0x02 DIN
	//0x01 CLK 
	DDRB = 0xFF;
	DDRD = 0x00;
	PORTD = 0xFF;
	PORTB = 0x00;
	DDRA = 0xFF;
	unsigned char gametckcnt = 3;
	unsigned char aiTick = aiSpeed;
	initLCD();
	spriteScene disp[sceneCnt];
	for (unsigned char i = 0; i < sceneCnt; i++){
		GetScene(&disp[i]);
	}
	SetSprites(&disp);
	makeScene(&disp[playerScn]);
	TimerSet(20);
	TimerOn();
//	drawSweep();
    while (1) 
    {
		while (!TimerFlag){}
		gametckcnt--;
		aiTick--;
		UpdateButtons();
		if (!gametckcnt){
			if (playerHealth){UpdatePlayer(&disp[playerScn], &disp);}
			if (!aiTick){
				for (unsigned char i = 0; i < sceneCnt; i++){
					UpdateActors(&(disp[i].sprites), &(disp[i].states), i);
				}
				aiTick = aiSpeed;
			}
			SetSprites(&disp);
			makeScene(&disp[playerScn]);
			gametckcnt = 3;
			leftEdge = off;
			leftCheck = notpressed;
			rightEdge = off;
			rightCheck = notpressed;
			jumpEdge = off;
			jumpCheck = notpressed;
		}
			
		TimerFlag = 0;
		//sendData(0xF0);
    }
}
