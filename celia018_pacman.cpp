#include <avr/io.h>
#include "ST7735.h"
#include <avr/pgmspace.h>
#include <stdlib.h>
#include "timerISR.h"
#include "periph.h"
#include "useful.h"


#define NUM_TASKS 11
#define TOTAL_DOTS 110

unsigned char xValue;
unsigned char yValue;
unsigned char level; //0 is title, 1 is loading screen, 2 is game, 3 is victory. loop back to 1 after victory
const unsigned int chompSound[4] = {600, 20, 420, 20};
const unsigned int menuMusic[8] = {262, 330, 392, 523, 392, 330, 262, 330};
const unsigned int ghostAmbience[8] = {110, 0, 110, 147, 0, 147, 110, 0};
const unsigned int deathNoise[8] = {523, 494, 440, 392, 330, 262, 196, 0};
const unsigned int ghostEatSound[6] = {784, 988, 1175, 1568, 1175, 0};
unsigned int score;
unsigned int highScore;
unsigned char dotCount;
bool deathFlag;
bool startFlag;
bool ghostEatenFlag;

const unsigned long Joystick_PERIOD = 50;
const unsigned long MovePlayer_PERIOD = 33;
const unsigned long RenderEntity_PERIOD = 33; //30fps
const unsigned long AnimatePlayer_PERIOD = 100;
const unsigned long GameState_PERIOD = 150;
const unsigned long Audio_PERIOD = 75;
const unsigned long Ghost_PERIOD = 33;
const unsigned long LED_PERIOD = 100;
const unsigned long GCD_PERIOD = 1;

#pragma region SPRITES
Sprite pacman1 = { 13, 13, pacman1Sprite };
Sprite pacman2 = { 13, 13, pacman2Sprite };
Sprite pacman3 = { 13, 13, pacman3Sprite };
Sprite pacman4 = { 13, 13, pacman4Sprite };
Sprite pacman5 = { 13, 13, pacman5Sprite };
Sprite pacman6 = { 13, 13, pacman6Sprite };
Sprite pacman7 = { 13, 13, pacman7Sprite };
Sprite pacman8 = { 13, 13, pacman8Sprite };
Sprite pacman9 = { 13, 13, pacman9Sprite };
Sprite pacman10 = { 13, 13, pacman10Sprite };
Sprite pacman11 = { 13, 13, pacman11Sprite };
Sprite pacman12 = { 13, 13, pacman12Sprite };
Sprite pacman13 = { 13, 13, pacman13Sprite };
Sprite pacman14 = { 13, 13, pacman14Sprite };
Sprite redGhost = { 13, 13, redSprite };
Sprite cyanGhost = { 13, 13, cyanSprite };
Sprite orangeGhost = { 13, 13, orangeSprite };
Sprite pinkGhost = { 13, 13, pinkSprite };
Sprite blueGhost = { 13, 13, blueSprite };
Sprite eyes = {13, 13, eyesSprite };
Sprite* GetDeathSprite(uint8_t i){
    if(i == 0) return &pacman1;
    if(i == 1) return &pacman2;
    if(i == 2) return &pacman3;
    if(i == 3) return &pacman10;
    if(i == 4) return &pacman11;
    if(i == 5) return &pacman12;
    if(i == 6) return &pacman13;
    return &pacman14;
}
#pragma endregion
Entity player = {{64, 116}, player.position, &pacman1, 2, 0, 0, 4, 0, false};
Entity red;
Entity pink;
Entity orange;
Entity cyan;

task tasks[NUM_TASKS];

void EatDotsNearPacman(void){ //the dots have a hitbox of 2x2
    uint8_t centerTileX = (player.position.x + player.sprite->width / 2) / TILE_SIZE;
    uint8_t centerTileY = (player.position.y + player.sprite->length / 2) / TILE_SIZE;

    for(int8_t y = -1; y <= 0; y++){
        for(int8_t x = -1; x <= 0; x++){

            int8_t tileX = centerTileX + x;
            int8_t tileY = centerTileY + y;

            if(tileX < 0 || tileX >= 16 || tileY < 0 || tileY >= 20){
                continue;
            }

            if(maze[tileY][tileX] == DOT){
                score += 10;
                dotCount--;
                maze[tileY][tileX] = EMPTY;
            }
            else if(maze[tileY][tileX] == BIGDOT){
                score += 50;
                dotCount--;
                if(red.lives != 0) red.lives = 1;
                if(pink.lives != 0) pink.lives = 1;
                if(orange.lives != 0) orange.lives = 1;
                if(cyan.lives != 0) cyan.lives = 1;
                maze[tileY][tileX] = EMPTY;
            }
        }
    }
}

enum Joystick_States {Joystick_Start, read};
enum MovePlayer_States {MovePlayer_Start, idle, moveLeft, moveRight, moveUp, moveDown};
enum RenderEntity_States {RenderEntity_Start, waitStart, render};
enum AnimatePlayer_States {AnimatePlayer_Start, waitMove, aniRight, aniLeft, aniDown, aniUp, aniDeath};
enum Audio_States {Audio_Start, Audio_Menu, Audio_Game, Audio_GhostEaten, Audio_Death};
enum RedGhost_States {RedGhost_Start, RedChase, RedLocked, RedTransition, RedFlee, RedCageReturn};
enum PinkGhost_States {PinkGhost_Start, PinkChase, PinkLocked, PinkTransition, PinkFlee, PinkCageReturn};
enum OrangeGhost_States {OrangeGhost_Start, OrangeChase, OrangeLocked, OrangeTransition, OrangeFlee, OrangeCageReturn};
enum CyanGhost_States {CyanGhost_Start, CyanChase, CyanLocked, CyanTransition, CyanFlee, CyanCageReturn};
enum GameState_States {GameState_Start, TitleScreen, ButtonPress, GameOn, Victory, Dead, EndScreen};
enum LED_States {LED_Start, LED_OffState, LED_BlueState, LED_GreenFlash, LED_RedFlash};

int TickFct_Joystick(int state);
int TickFct_MovePlayer(int state);
int TickFct_RenderEntity(int state);
int TickFct_AnimatePlayer(int state);
int TickFct_Audio(int state);
int TickFct_RedGhost(int state);
int TickFct_PinkGhost(int state);
int TickFct_OrangeGhost(int state);
int TickFct_CyanGhost(int state);
int TickFct_GameState(int state);
int TickFct_LED(int state);

int TickFct_Joystick(int state){
    switch(state) {//transitions
        case Joystick_Start:
            player.xDir = 0;
            player.yDir = 0;
            state = read;
            break;
        case read:
            state = read;
            break;
    }
    switch(state) {
        case Joystick_Start:
            break;
        case read:
            xValue = ADC_read(0);
            yValue = ADC_read(1);
            
            if (xValue < 21) xValue = 21;
            if (xValue > 250) xValue = 250;
            if(yValue < 26){
                player.yDir = 1; //up
            }
            else if(yValue > 30){
                player.yDir = 2; //down
            }
            else{
                player.yDir = 0; //center
            }

            if(xValue < 40){
                player.xDir = 1; //left
            }
            else if(xValue > 50){
                player.xDir = 2; //right
            }
            else{
                player.xDir = 0; //center
            }
            break;
    }
    return state;
}
int TickFct_MovePlayer(int state){
    switch(state){//transitions
        case MovePlayer_Start:
            player.position = {60, 110}; //change once we have maze rendered
            player.oldPosition = player.position;
            state = idle;
            break;
        case idle:
            if(!startFlag){
                state = idle;
            }
            else if((!player.xDir && !player.yDir) || (player.xDir && player.yDir)){
                state = idle;
            }
            else if(player.xDir == 1 && !player.yDir){
                player.isMoving = true;
                state = moveLeft;
            }
            else if(player.xDir == 2 && !player.yDir){
                player.isMoving = true;
                state = moveRight;
            }
            else if(player.yDir == 1 && !player.xDir){
                player.isMoving = true;
                state = moveUp;
            }
            else if(player.yDir == 2 && !player.xDir){
                player.isMoving = true;
                state = moveDown;
            }
            break;
        case moveLeft:
            if(player.xDir == 1 && !player.yDir){
                state = moveLeft;
            }
            else {
                state = idle;
            }
            break;
        case moveRight:
            if(player.xDir == 2 && !player.yDir){
                state = moveRight;
            }
            else {
                state = idle;
            }
            break;
        case moveUp:
            if(player.yDir == 1 && !player.xDir){
                state = moveUp;
            }
            else {
                state = idle;
            }
            break;
        case moveDown:
            if(player.yDir == 2 && !player.xDir){
                state = moveDown;
            }
            else {
                state = idle;
            }
            break;

    }
    switch(state){//actions
        case MovePlayer_Start:
            break;
        case idle:
            player.isMoving = false;
            break;
        case moveLeft:
            if(CanMoveLeft(player)){
                player.position.x -= player.speed;
            }
            break;
        case moveRight:
            if(CanMoveRight(player)){
                player.position.x += player.speed;
            }
            break;
        case moveUp:
            if(CanMoveUp(player)){
                player.position.y -= player.speed;
            }
            break;
        case moveDown:
            if(CanMoveDown(player)){
                player.position.y += player.speed;
            }
            break;
    }
    //handles if we move onto a dot
    EatDotsNearPacman();
    return state;
}
int TickFct_RenderEntity(int state){//draws sprites, only in level 1. need to clear screen inbetween level transitions
    switch(state){ //transitions
        case RenderEntity_Start:
            state = waitStart;
            break;
        case waitStart:
            if(level == 1){
                state = render;
            }
            else{
                state = waitStart;
            }
            break;
        case render:
            if(level == 1){
                state = render;
            }
            else{
                state = waitStart;
            }
            break;
    }
    switch(state){//actions
        case RenderEntity_Start:
            break;
        case waitStart:
            break;
        case render:
            if(deathFlag){
                ClearEntityBox(&red);
                ClearEntityBox(&pink);
                ClearEntityBox(&orange);
                ClearEntityBox(&cyan);
                ClearEntityBox(&player);
                TFT_DrawSprite(player.sprite, player.position.x, player.position.y);

                player.oldPosition = player.position;
                red.oldPosition = red.position;
                pink.oldPosition = pink.position;
                orange.oldPosition = orange.position;
                cyan.oldPosition = cyan.position;
            }
            else{
                eraseOldRender(player);
                eraseOldRender(red);
                eraseOldRender(pink);
                eraseOldRender(orange);
                eraseOldRender(cyan);

                TFT_DrawSprite(player.sprite, player.position.x, player.position.y);
                TFT_DrawSprite(red.sprite, red.position.x, red.position.y);
                TFT_DrawSprite(pink.sprite, pink.position.x, pink.position.y);
                TFT_DrawSprite(orange.sprite, orange.position.x, orange.position.y);
                TFT_DrawSprite(cyan.sprite, cyan.position.x, cyan.position.y);

                player.oldPosition = player.position;
                red.oldPosition = red.position;
                pink.oldPosition = pink.position;
                orange.oldPosition = orange.position;
                cyan.oldPosition = cyan.position;
            }
            break;    
    }
    return state;
}
int TickFct_AnimatePlayer(int state){
    static unsigned char i;
    switch(state){//transitions
        case AnimatePlayer_Start:
            state = waitMove;
            break;
        case waitMove:
            if(deathFlag){
                i = 0;
                state = aniDeath;
            }
            else if(!startFlag){ // if game hasnt started, dont animate
                state = waitMove;
            }
            else if(player.isMoving && (player.xDir == 2)){
                state = aniRight;
            }
            else if(player.isMoving && (player.xDir == 1)){
                state = aniLeft;
            }
            else if(player.isMoving && (player.yDir == 1)){
                state = aniUp;
            }
            else if(player.isMoving && (player.yDir == 2)){
                state = aniDown;
            }
            else{
                state = waitMove;
            }
            break;
        case aniRight:
            if(deathFlag){
                i = 0;
                state = aniDeath;
            }
            else if(player.isMoving && (player.xDir == 2)){
                state = aniRight;
            }
            else {
                state = waitMove;
            }
            break;
        case aniLeft:
            if(deathFlag){
                i = 0;
                state = aniDeath;
            }
            else if(player.isMoving && (player.xDir == 1)){
                state = aniLeft;
            }
            else {
                state = waitMove;
            }
            break;
        case aniUp:
            if(deathFlag){
                i = 0;
                state = aniDeath;
            }
            else if(player.isMoving && (player.yDir == 1)){
                state = aniUp;
            }
            else {
                state = waitMove;
            }
            break;
        case aniDown:
            if(deathFlag){
                i = 0;
                state = aniDeath;
            }
            else if(player.isMoving && (player.yDir == 2)){
                state = aniDown;
            }
            else {
                state = waitMove;
            }
            break;
        case aniDeath:
            if(deathFlag){
                state = aniDeath;
            }
            else {
                player.position = {60, 110};
                player.oldPosition = player.position;
                player.sprite = &pacman1;
                player.isMoving = false;
                state = waitMove;
            }
            break;
    }
    switch(state){//actions
        case AnimatePlayer_Start:
            break;
        case waitMove:
            i = 0;
            break;
        case aniRight:
            i++;
            if(i > 3) {
                i = 1;
            }
            if(i == 1){
                player.sprite = &pacman1;
            }
            else if(i == 2){
                player.sprite = &pacman2;
            }
            else{
                player.sprite = &pacman3;
            }
            break;
        case aniLeft:
            i++;
            if(i > 3) {
                i = 1;
            }
            if(i == 1){
                player.sprite = &pacman1;
            }
            else if(i == 2){
                player.sprite = &pacman6;
            }
            else{
                player.sprite = &pacman7;
            }
            break;
        case aniUp:
            i++;
            if(i > 3) {
                i = 1;
            }
            if(i == 1){
                player.sprite = &pacman1;
            }
            else if(i == 2){
                player.sprite = &pacman4;
            }
            else{
                player.sprite = &pacman5;
            }
            break;
        case aniDown:
            i++;
            if(i > 3) {
                i = 1;
            }
            if(i == 1){
                player.sprite = &pacman1;
            }
            else if(i == 2){
                player.sprite = &pacman8;
            }
            else{
                player.sprite = &pacman9;
            }
            break;
        case aniDeath:
            if(i < 8){
                player.sprite = GetDeathSprite(i);
                i++;
            }
            break;//todo
            
    }
    return state;
}
int TickFct_Audio(int state){
    static unsigned char menuIndex;
    static unsigned char gameIndex;
    static unsigned char deathIndex;
    static unsigned char eatenIndex;

    switch(state){ // transitions
        case Audio_Start: //change naming conventions
            menuIndex = 0;
            gameIndex = 0;
            deathIndex = 0;
            eatenIndex = 0;
            ghostEatenFlag = false;
            state = Audio_Menu;
            break;

        case Audio_Menu:
            if(deathFlag){//delete
                deathIndex = 0;
                state = Audio_Death;
            }
            else if(startFlag){
                gameIndex = 0;
                state = Audio_Game;
            }
            else{
                state = Audio_Menu;
            }
            break;
        case Audio_Game:
            if(deathFlag){
                deathIndex = 0;
                state = Audio_Death;
            }
            else if(ghostEatenFlag){
                eatenIndex = 0;
                state = Audio_GhostEaten;
            }
            else if(!startFlag){
                menuIndex = 0;
                state = Audio_Menu;
            }
            else{
                state = Audio_Game;
            }
            break;
        case Audio_GhostEaten:
            if(deathFlag){
                deathIndex = 0;
                state = Audio_Death;
            }
            else if(eatenIndex >= 6){
                ghostEatenFlag = false;
                Buzzer_Stop();
                if(startFlag){
                    gameIndex = 0;
                    state = Audio_Game;
                }
                else{
                    menuIndex = 0;
                    state = Audio_Menu;
                }
            }
            else{
                state = Audio_GhostEaten;
            }
            break;
        case Audio_Death:
            if(!deathFlag){
                Buzzer_Stop();
                if(startFlag){
                    gameIndex = 0;
                    state = Audio_Game;
                }
                else{
                    menuIndex = 0;
                    state = Audio_Menu;
                }
            }
            else{
                state = Audio_Death;
            }
            break;
    }
    switch(state){ // actions
        case Audio_Start:
            break;
        case Audio_Menu:
            Buzzer_Play(menuMusic[menuIndex]);
            menuIndex++;
            if(menuIndex >= 8){
                menuIndex = 0;
            }
            break;
        case Audio_Game:
            if(player.isMoving){
                Buzzer_Play(chompSound[gameIndex % 4]);
            }
            else{
                Buzzer_Play(ghostAmbience[gameIndex % 8]);
            }
            gameIndex++;
            if(gameIndex >= 8){
                gameIndex = 0;
            }
            break;
        case Audio_GhostEaten:
            if(eatenIndex < 6){
                Buzzer_Play(ghostEatSound[eatenIndex]);
                eatenIndex++;
            }
            else{
                Buzzer_Stop();
            }
            break;
        case Audio_Death:
            if(deathIndex < 8){
                Buzzer_Play(deathNoise[deathIndex]);
                deathIndex++;
            }
            else{
                Buzzer_Stop();
            }
            break;
    }
    return state;
}
int TickFct_RedGhost(int state){
    static unsigned char i;
    static Entity redTarget;
    switch(state){//transition
        case RedGhost_Start:
            red.position.x = 38;
            red.position.y = 80;
            red.sprite = &eyes;
            red.speed = 1;
            red.xDir = 0;
            red.yDir = 1;
            red.lives = 0;
            red.entityType = 1;

            redTarget.position.x = 8 * TILE_SIZE;
            redTarget.position.y = 7 * TILE_SIZE;
            redTarget.sprite = 0;

            i = 0;
            if(startFlag){
                state = RedLocked;
            }
            else {
                state = RedGhost_Start;
            }
            break;
        case RedLocked:
            if(!startFlag){
                state = RedGhost_Start;
            }
            else if(i > 150){
                state = RedTransition;
                i = 0;
            }
            else state = RedLocked;
            break;
        case RedTransition:
            if(!startFlag){
                state = RedGhost_Start;
            }
            else if(GhostReachedTile(&red, 8, 7)){
                state = RedChase;
                red.lives = 2;
                red.sprite = &redGhost;
            }
            else state = RedTransition;
            break;
        case RedChase:
            if(!startFlag){
                state = RedGhost_Start;
            }
            else if(red.lives == 2) state = RedChase;
            else if(red.lives == 1){
                state = RedFlee;
                red.sprite = &blueGhost;
                i = 0;
            }
            else if (red.lives == 0) state = RedCageReturn;
            break;
        case RedFlee:
            if(!startFlag){
                state = RedGhost_Start;
            }
            else if(i > 200 && red.lives == 1){
                state = RedChase;
                red.lives = 2;
                red.sprite = &redGhost;
                i = 0;
            }
            else if(red.lives == 0){
                state = RedCageReturn;
                red.sprite = &eyes;
                i = 0;
            }
            else state = RedFlee;
            break;
        case RedCageReturn:
            if(!startFlag){
                state = RedGhost_Start;
            }
            else if(GhostReachedTile(&red, 6, 10)){
                state = RedLocked;

                red.position.x = 38;
                red.position.y = 80;
                red.oldPosition = red.position;
                red.sprite = &eyes;
                red.lives = 0;
                red.xDir = 2;
                red.yDir = 0;

                redTarget.position.x = 8 * TILE_SIZE;
                redTarget.position.y = 7 * TILE_SIZE;
            }
            else state = RedCageReturn;
            break;
    }
    switch(state){//actions
        case RedGhost_Start:
            break;
        case RedLocked:
            i++;
            break;
        case RedTransition:
            MoveGhost(&red, &redTarget);
            break;
        case RedChase:
            MoveGhost(&red, &player);
            if(red.lives == 2 && CheckEntityCollision(&player, &red)){
                deathFlag = true;
            }
            break;
        case RedFlee:
            i++;
            if(i < 100){
                redTarget.position.x = 1 * TILE_SIZE;
                redTarget.position.y = 3 * TILE_SIZE;
                MoveGhost(&red, &redTarget);
            }
            else if(i < 200){
                redTarget.position.x = 14 * TILE_SIZE;
                redTarget.position.y = 16 * TILE_SIZE;
                MoveGhost(&red, &redTarget);
            }
            if(red.lives == 1 && CheckEntityCollision(&player, &red)){
                red.lives = 0;
                ghostEatenFlag = true;
                score += 400;
            }
            break;
        case RedCageReturn:
            redTarget.position.x = 6 * TILE_SIZE;
            redTarget.position.y = 10 * TILE_SIZE;
            MoveGhost(&red, &redTarget);
            break;
    }
    return state;
}
int TickFct_PinkGhost(int state){
    static unsigned char j;
    static Entity pinkTarget;
    switch(state){//transition
        case PinkGhost_Start:
            pink.position.x = 51;
            pink.position.y = 80;
            pink.sprite = &eyes;
            pink.speed = 1;
            pink.xDir = 0;
            pink.yDir = 1;
            pink.lives = 0;
            pink.entityType = 1;

            pinkTarget.position.x = 8 * TILE_SIZE;
            pinkTarget.position.y = 7 * TILE_SIZE;
            pinkTarget.sprite = 0;

            j = 0;
            if(startFlag){
                state = PinkLocked;
            }
            else {
                state = PinkGhost_Start;
            }
            break;
        case PinkLocked:
            if(!startFlag){
                state = PinkGhost_Start;
            }
            else if(j > 250){
                state = PinkTransition;
                j = 0;
            }
            else state = PinkLocked;
            break;
        case PinkTransition:
            if(!startFlag){
                state = PinkGhost_Start;
            }
            else if(GhostReachedTile(&pink, 8, 7)){
                state = PinkChase;
                pink.lives = 2;
                pink.sprite = &pinkGhost;
            }
            else state = PinkTransition;
            break;
        case PinkChase:
            if(!startFlag){
                state = PinkGhost_Start;
            }
            else if(pink.lives == 2) state = PinkChase;
            else if(pink.lives == 1){
                state = PinkFlee;
                pink.sprite = &blueGhost;
                j = 0;
            }
            else if (pink.lives == 0) state = PinkCageReturn;
            break;
        case PinkFlee:
            if(!startFlag){
                state = PinkGhost_Start;
            }
            else if(j > 200 && pink.lives == 1){
                state = PinkChase;
                pink.lives = 2;
                pink.sprite = &pinkGhost;
                j = 0;
            }
            else if(pink.lives == 0){
                state = PinkCageReturn;
                pink.sprite = &eyes;
                j = 0;
            }
            else state = PinkFlee;
            break;
        case PinkCageReturn:
            if(!startFlag){
                state = PinkGhost_Start;
            }
            else if(GhostReachedTile(&pink, 7, 10)){
                state = PinkLocked;
                pink.position.x = 51;
                pink.position.y = 80;
                pink.oldPosition = pink.position;
                pink.sprite = &eyes;
                pink.lives = 0;
                pink.xDir = 2;
                pink.yDir = 0;

                pinkTarget.position.x = 8 * TILE_SIZE;
                pinkTarget.position.y = 7 * TILE_SIZE;
            }
            else state = PinkCageReturn;
            break;
    }
    switch(state){//actions
        case PinkGhost_Start:
            break;
        case PinkLocked:
            j++;
            break;
        case PinkTransition:
            MoveGhost(&pink, &pinkTarget);
            break;
        case PinkChase:
            MoveGhost(&pink, &player);
            if(pink.lives == 2 && CheckEntityCollision(&player, &pink)){
                deathFlag = true;
            }
            break;
        case PinkFlee:
            j++;
            if(j < 100){
                pinkTarget.position.x = 14 * TILE_SIZE;
                pinkTarget.position.y = 3 * TILE_SIZE;
                MoveGhost(&pink, &pinkTarget);
            }
            else if(j < 200){
                pinkTarget.position.x = 1 * TILE_SIZE;
                pinkTarget.position.y = 16 * TILE_SIZE;
                MoveGhost(&pink, &pinkTarget);
            }
            if(pink.lives == 1 && CheckEntityCollision(&player, &pink)){
                pink.lives = 0;
                ghostEatenFlag = true;
                score += 400;
            }
            break;
        case PinkCageReturn:
            pinkTarget.position.x = 7 * TILE_SIZE;
            pinkTarget.position.y = 10 * TILE_SIZE;
            MoveGhost(&pink, &pinkTarget);
            break;
    }
    return state;
}
int TickFct_OrangeGhost(int state){
    static unsigned short k;
    static Entity orangeTarget;
    switch(state){//transition
        case OrangeGhost_Start:
            orange.position.x = 64;
            orange.position.y = 80;
            orange.oldPosition = orange.position;
            orange.sprite = &eyes;
            orange.speed = 1;
            orange.xDir = 1;
            orange.yDir = 0;
            orange.lives = 0;
            orange.entityType = 1;

            orangeTarget.position.x = 8 * TILE_SIZE;
            orangeTarget.position.y = 7 * TILE_SIZE;
            orangeTarget.sprite = 0;
            k = 0;
            if(startFlag){
                state = OrangeLocked;
            }
            else{
                state = OrangeGhost_Start;
            }
            break;
        case OrangeLocked:
            if(!startFlag){
                state = OrangeGhost_Start;
            }
            else if(k > 350){
                state = OrangeTransition;
                k = 0;
            }
            else{
                state = OrangeLocked;
            }
            break;
        case OrangeTransition:
            if(!startFlag){
                state = OrangeGhost_Start;
            }
            else if(GhostReachedTile(&orange, 8, 7)){
                state = OrangeChase;
                orange.lives = 2;
                orange.sprite = &orangeGhost;
            }
            else{
                state = OrangeTransition;
            }
            break;
        case OrangeChase:
            if(!startFlag){
                state = OrangeGhost_Start;
            }
            else if(orange.lives == 2){
                state = OrangeChase;
            }
            else if(orange.lives == 1){
                state = OrangeFlee;
                orange.sprite = &blueGhost;
                k = 0;
            }
            else if(orange.lives == 0){
                state = OrangeCageReturn;
            }
            break;
        case OrangeFlee:
            if(!startFlag){
                state = OrangeGhost_Start;
            }
            else if(k > 200 && orange.lives == 1){
                state = OrangeChase;
                orange.lives = 2;
                orange.sprite = &orangeGhost;
                k = 0;
            }
            else if(orange.lives == 0){
                state = OrangeCageReturn;
                orange.sprite = &eyes;
                k = 0;
            }
            else{
                state = OrangeFlee;
            }
            break;
        case OrangeCageReturn:
            if(!startFlag){
                state = OrangeGhost_Start;
            }
            else if(GhostReachedTile(&orange, 8, 10)){
                state = OrangeLocked;

                orange.position.x = 64;
                orange.position.y = 80;
                orange.oldPosition = orange.position;
                orange.sprite = &eyes;
                orange.lives = 0;
                orange.xDir = 0;
                orange.yDir = 1;
                orangeTarget.position.x = 8 * TILE_SIZE;
                orangeTarget.position.y = 7 * TILE_SIZE;
            }
            else{
                state = OrangeCageReturn;
            }
            break;
    }
    switch(state){//actions
        case OrangeGhost_Start:
            break;
        case OrangeLocked:
            k++;
            break;
        case OrangeTransition:
            MoveGhost(&orange, &orangeTarget);
            break;
        case OrangeChase:
            MoveGhost(&orange, &player);
            if(orange.lives == 2 && CheckEntityCollision(&player, &orange)){
                deathFlag = true;
            }
            break;
        case OrangeFlee:
            k++;
            if(k < 100){
                orangeTarget.position.x = 14 * TILE_SIZE;
                orangeTarget.position.y = 16 * TILE_SIZE;
                MoveGhost(&orange, &orangeTarget);
            }
            else if(k < 200){
                orangeTarget.position.x = 1 * TILE_SIZE;
                orangeTarget.position.y = 3 * TILE_SIZE;
                MoveGhost(&orange, &orangeTarget);
            }
            if(orange.lives == 1 && CheckEntityCollision(&player, &orange)){
                orange.lives = 0;
                ghostEatenFlag = true;
                score += 400;
            }
            break;
        case OrangeCageReturn:
            orangeTarget.position.x = 9 * TILE_SIZE;
            orangeTarget.position.y = 10 * TILE_SIZE;
            MoveGhost(&orange, &orangeTarget);
            break;
    }
    return state;
}
int TickFct_CyanGhost(int state){
    static unsigned short c;
    static Entity cyanTarget;

    switch(state){//transition
        case CyanGhost_Start:
            cyan.position.x = 77;
            cyan.position.y = 80;
            cyan.oldPosition = cyan.position;
            cyan.sprite = &eyes;
            cyan.speed = 1;
            cyan.xDir = 1;
            cyan.yDir = 0;
            cyan.lives = 0;
            cyan.entityType = 1;

            cyanTarget.position.x = 8 * TILE_SIZE;
            cyanTarget.position.y = 7 * TILE_SIZE;
            cyanTarget.sprite = 0;

            c = 0;

            if(startFlag){
                state = CyanLocked;
            }
            else{
                state = CyanGhost_Start;
            }
            break;

        case CyanLocked:
            if(!startFlag){
                state = CyanGhost_Start;
            }
            else if(c > 450){
                state = CyanTransition;
                c = 0;
            }
            else{
                state = CyanLocked;
            }
            break;

        case CyanTransition:
            if(!startFlag){
                state = CyanGhost_Start;
            }
            else if(GhostReachedTile(&cyan, 8, 7)){
                state = CyanChase;
                cyan.lives = 2;
                cyan.sprite = &cyanGhost;
            }
            else{
                state = CyanTransition;
            }
            break;

        case CyanChase:
            if(!startFlag){
                state = CyanGhost_Start;
            }
            else if(cyan.lives == 2){
                state = CyanChase;
            }
            else if(cyan.lives == 1){
                state = CyanFlee;
                cyan.sprite = &blueGhost;
                c = 0;
            }
            else if(cyan.lives == 0){
                state = CyanCageReturn;
            }
            break;

        case CyanFlee:
            if(!startFlag){
                state = CyanGhost_Start;
            }
            else if(c > 200 && cyan.lives == 1){
                state = CyanChase;
                cyan.lives = 2;
                cyan.sprite = &cyanGhost;
                c = 0;
            }
            else if(cyan.lives == 0){
                state = CyanCageReturn;
                cyan.sprite = &eyes;
                c = 0;
            }
            else{
                state = CyanFlee;
            }
            break;

        case CyanCageReturn:
            if(!startFlag){
                state = CyanGhost_Start;
            }
            else if(GhostReachedTile(&cyan, 9, 10)){
                state = CyanLocked;

                cyan.position.x = 77;
                cyan.position.y = 80;
                cyan.oldPosition = cyan.position;
                cyan.sprite = &eyes;
                cyan.lives = 0;
                cyan.xDir = 1;
                cyan.yDir = 0;

                cyanTarget.position.x = 8 * TILE_SIZE;
                cyanTarget.position.y = 7 * TILE_SIZE;
            }
            else{
                state = CyanCageReturn;
            }
            break;
    }

    switch(state){//actions
        case CyanGhost_Start:
            break;

        case CyanLocked:
            c++;
            break;

        case CyanTransition:
            MoveGhost(&cyan, &cyanTarget);
            break;

        case CyanChase:
            MoveGhost(&cyan, &player);

            if(cyan.lives == 2 && CheckEntityCollision(&player, &cyan)){
                deathFlag = true;
            }
            break;

        case CyanFlee:
            c++;

            if(c < 100){
                cyanTarget.position.x = 1 * TILE_SIZE;
                cyanTarget.position.y = 16 * TILE_SIZE;
                MoveGhost(&cyan, &cyanTarget);
            }
            else if(c < 200){
                cyanTarget.position.x = 14 * TILE_SIZE;
                cyanTarget.position.y = 3 * TILE_SIZE;
                MoveGhost(&cyan, &cyanTarget);
            }

            if(cyan.lives == 1 && CheckEntityCollision(&player, &cyan)){
                cyan.lives = 0;
                ghostEatenFlag = true;
                score += 400;
            }
            break;

        case CyanCageReturn:
            cyanTarget.position.x = 10 * TILE_SIZE;
            cyanTarget.position.y = 10 * TILE_SIZE;
            MoveGhost(&cyan, &cyanTarget);
            break;
    }
    return state;
}
int TickFct_GameState(int state){
    static unsigned char i;
    static unsigned char flash;
    switch(state){//transitions
        case GameState_Start:
            deathFlag = 0;
            level = 0;
            startFlag = 0;
            i = 0;
            score = 0;
            highScore = 0;
            flash = 0;
            TFT_FillScreen(ST7735_BLACK);
            TFT_DrawText(50, 40, "PACMAN", ST7735_YELLOW, ST7735_BLACK);
            TFT_DrawText(10, 120, "PRESS START BUTTON", ST7735_WHITE, ST7735_BLACK);
            state = TitleScreen;
            break;
        case TitleScreen:
            if(PINC & 0b001000){
                state = ButtonPress;
            }
            else {
                state = TitleScreen;
            }
            break;
        case ButtonPress:
            if(PINC & 0b001000){
                state = ButtonPress;
            }    
            else {
                state = GameOn;
                player.lives = 4;
                startFlag = 1;
                dotCount = TOTAL_DOTS;
                level = 1;
                //reset maze
                for(uint8_t y = 0; y < 20; y++){
                    for(uint8_t x = 0; x < 16; x++){
                        maze[y][x] = pgm_read_byte(&mazeRef[y][x]);
                    }
                }
                TFT_FillScreen(ST7735_BLACK);
                DrawMaze();
                TFT_DrawText(2, 2, "SCORE", ST7735_WHITE, ST7735_BLACK);
                TFT_DrawSprite(&pacman1, 0, 146);
                TFT_DrawSprite(&pacman1, 20, 146);
                TFT_DrawSprite(&pacman1, 40, 146);
            }
            break;
        case GameOn:
            if((dotCount == 0) && !deathFlag){
                state = Victory;
                startFlag = 0;
            }
            else if(deathFlag){
                state = Dead;
                startFlag = 0;
                player.lives--;
            }
            else {
                state = GameOn;
            }
            break;
        case Victory:
            if(i > 20){
                state = GameOn;
                startFlag = 1;
                dotCount = TOTAL_DOTS;
                i = 0;
                player.position = {60, 110};
                player.oldPosition = player.position;
                player.sprite = &pacman1;
                player.isMoving = false;
                for(uint8_t y = 0; y < 20; y++){
                    for(uint8_t x = 0; x < 16; x++){
                        maze[y][x] = pgm_read_byte(&mazeRef[y][x]);
                    }
                }

                TFT_FillScreen(ST7735_BLACK);
                DrawMaze();
                TFT_DrawText(2, 2, "SCORE", ST7735_WHITE, ST7735_BLACK);
                if(player.lives >= 4){
                    TFT_DrawSprite(&pacman1, 40, 146);
                }
                if(player.lives >= 3){
                    TFT_DrawSprite(&pacman1, 20, 146);
                }
                if(player.lives >= 2){
                    TFT_DrawSprite(&pacman1, 0, 146);
                }
            }
            else {
                state = Victory;
                startFlag = 0;
            }
            break;
        case Dead:
            if((i > 30) && (player.lives > 0)){
                state = GameOn;
                deathFlag = 0;
                startFlag = 1;
                i = 0;

                player.position = {60, 110};
                player.oldPosition = player.position;
                player.sprite = &pacman1;
                player.xDir = 0;
                player.yDir = 0;
                player.isMoving = false;

                TFT_FillScreen(ST7735_BLACK);
                DrawMaze();
                TFT_DrawText(2, 2, "SCORE", ST7735_WHITE, ST7735_BLACK);
                if(player.lives >= 4){
                    TFT_DrawSprite(&pacman1, 40, 146);
                }
                if(player.lives >= 3){
                    TFT_DrawSprite(&pacman1, 20, 146);
                }
                if(player.lives >= 2){
                    TFT_DrawSprite(&pacman1, 0, 146);
                }
            }
            else if((i > 50) && (player.lives == 0)){
                state = EndScreen;
                startFlag = 0;
                level = 2;
                TFT_FillScreen(ST7735_BLACK);
                TFT_DrawText(50, 60, "SCORE", ST7735_WHITE, ST7735_BLACK);
                char scoreText[10];
                utoa(score, scoreText, 10); //makes score text
                TFT_DrawText(50, 70, scoreText, ST7735_WHITE, ST7735_BLACK);
                TFT_DrawText(40, 100, "HIGH SCORE", ST7735_YELLOW, ST7735_BLACK);
                char highScoreText[10];
                utoa(highScore, highScoreText, 10); //makes score text
                TFT_DrawText(50, 110, highScoreText, ST7735_WHITE, ST7735_BLACK);
                deathFlag = 0;
                i = 0;
            }
            else {
                state = Dead;
            }
            break;
        case EndScreen:
            if(i > 50){
                state = TitleScreen;
                TFT_FillScreen(ST7735_BLACK);
                TFT_DrawText(50, 40, "PACMAN", ST7735_YELLOW, ST7735_BLACK);
                TFT_DrawText(40, 70, "HIGH SCORE", ST7735_YELLOW, ST7735_BLACK);
                char highScoreText[10];
                utoa(highScore, highScoreText, 10); //makes score text
                TFT_DrawText(40, 80, highScoreText, ST7735_WHITE, ST7735_BLACK);
                TFT_DrawText(10, 120, "PRESS START BUTTON", ST7735_WHITE, ST7735_BLACK);
                score = 0;
                player.lives = 4;
                i = 0;
                level = 0;
            }
            else{
                state = EndScreen;
            }
            break;

    }
    switch(state){//actions
        case GameState_Start:
            break;
        case TitleScreen:
            break;
        case ButtonPress:
            break;
        case GameOn:
            flash = !flash;
            if(flash % 2) TFT_DrawRectangle(5 * TILE_SIZE, 9 * TILE_SIZE, 6 * TILE_SIZE + 1, 4 * TILE_SIZE, ST7735_BLACK);
            DrawMaze();
            if(score >= highScore) highScore = score;
            char scoreText[10];
            utoa(score, scoreText, 10); //makes score text
            TFT_DrawText(2, 10, scoreText, ST7735_WHITE, ST7735_BLACK);
            break;
        case Victory:
            i++;    
            break;
        case Dead:  
            i++;
            break;
        case EndScreen:
            TFT_DrawText(50, 60, "SCORE", ST7735_WHITE, ST7735_BLACK);
            char scoreText1[10];
            utoa(score, scoreText1, 10); //makes score text
            TFT_DrawText(50, 70, scoreText1, ST7735_WHITE, ST7735_BLACK);
            TFT_DrawText(40, 100, "HIGH SCORE", ST7735_YELLOW, ST7735_BLACK);
            char highScoreText1[10];
            utoa(highScore, highScoreText1, 10); //makes score text
            TFT_DrawText(50, 110, highScoreText1, ST7735_WHITE, ST7735_BLACK);
            i++;
            break;
    }
    return state;
}
int TickFct_LED(int state){
    static unsigned char i;
    switch(state){ // transitions
        case LED_Start:
            i = 0;
            state = LED_OffState;
            break;
        case LED_OffState:
            if(deathFlag){
                i = 0;
                state = LED_RedFlash;
            }
            else if(ghostEatenFlag){
                i = 0;
                state = LED_GreenFlash;
            }
            else if(red.lives == 1 || pink.lives == 1 || orange.lives == 1 || cyan.lives == 1){
                state = LED_BlueState;
            }
            else{
                state = LED_OffState;
            }
            break;
        case LED_BlueState:
            if(deathFlag){
                i = 0;
                state = LED_RedFlash;
            }
            else if(ghostEatenFlag){
                i = 0;
                state = LED_GreenFlash;
            }
            else if(red.lives == 1 || pink.lives == 1 || orange.lives == 1 || cyan.lives == 1){
                state = LED_BlueState;
            }
            else{
                state = LED_OffState;
            }
            break;
        case LED_GreenFlash:
            if(deathFlag){
                i = 0;
                state = LED_RedFlash;
            }
            else if(i >= 6){
                ghostEatenFlag = false;
                i = 0;
                if(red.lives == 1 || pink.lives == 1 || orange.lives == 1 || cyan.lives == 1){
                    state = LED_BlueState;
                }
                else{
                    state = LED_OffState;
                }
            }
            else{
                state = LED_GreenFlash;
            }
            break;
        case LED_RedFlash:
            if(i >= 6){
                i = 0;
                if(deathFlag){
                    state = LED_RedFlash;
                }
                else if(red.lives == 1 || pink.lives == 1 || orange.lives == 1 || cyan.lives == 1){
                    state = LED_BlueState;
                }
                else{
                    state = LED_OffState;
                }
            }
            else{
                state = LED_RedFlash;
            }
            break;
    }
    switch(state){ // actions
        case LED_Start:
            break;
        case LED_OffState:
            LED_Off();
            break;
        case LED_BlueState:
            LED_SetBlue();
            break;
        case LED_GreenFlash:
            if(i % 2 == 0){
                LED_SetGreen();
            }
            else{
                LED_Off();
            }
            i++;
            break;
        case LED_RedFlash:
            if(i % 2 == 0){
                LED_SetRed();
            }
            else{
                LED_Off();
            }
            i++;
            break;
    }
    return state;
}
void TimerISR() {
	for ( unsigned int i = 0; i < NUM_TASKS; i++ ) {                   // Iterate through each task in the task array
		if ( tasks[i].elapsedTime == tasks[i].period ) {           // Check if the task is ready to tick
			tasks[i].state = tasks[i].TickFct(tasks[i].state); // Tick and set the next state for this task
			tasks[i].elapsedTime = 0;                          // Reset the elapsed time for the next tick
		}
		tasks[i].elapsedTime += GCD_PERIOD;                        // Increment the elapsed time by GCD_PERIOD
	}
}

int main(void)
{
    TFT_Init();
    ADC_init();
    //port inits
    DDRC    = 0x00;
    PORTC   = 0xFF;
    DDRD |= (1 << PD2) | (1 << PD3) | (1 << PD4);
    LED_Off();

    //buzzer pwm inits
    DDRB |= (1 << PB1);
    PORTB &= ~(1 << PB1);

    TCCR1A |= (1 << WGM11);
    TCCR1B |= (1 << WGM12) | (1 << WGM13);
    TCCR1A |= (1 << COM1A1);
    TCCR1B |= (1 << CS11);
    ICR1 = 39999;
    OCR1A = 0;

    #pragma region taskInits

    tasks[0].period = Joystick_PERIOD;
    tasks[0].state = Joystick_Start;
    tasks[0].elapsedTime = Joystick_PERIOD;
    tasks[0].TickFct = &TickFct_Joystick;

    tasks[1].period = MovePlayer_PERIOD;
    tasks[1].state = MovePlayer_Start;
    tasks[1].elapsedTime = MovePlayer_PERIOD;
    tasks[1].TickFct = &TickFct_MovePlayer;

    tasks[2].period = RenderEntity_PERIOD;
    tasks[2].state = RenderEntity_Start;
    tasks[2].elapsedTime = RenderEntity_PERIOD;
    tasks[2].TickFct = &TickFct_RenderEntity;

    tasks[3].period = AnimatePlayer_PERIOD;
    tasks[3].state = AnimatePlayer_Start;
    tasks[3].elapsedTime = AnimatePlayer_PERIOD;
    tasks[3].TickFct = &TickFct_AnimatePlayer;

    tasks[4].period = Audio_PERIOD;
    tasks[4].state = Audio_Start;
    tasks[4].elapsedTime = Audio_PERIOD;
    tasks[4].TickFct = &TickFct_Audio;

    tasks[5].period = Ghost_PERIOD;
    tasks[5].state = RedGhost_Start;
    tasks[5].elapsedTime = Ghost_PERIOD;
    tasks[5].TickFct = &TickFct_RedGhost;

    tasks[6].period = Ghost_PERIOD;
    tasks[6].state = PinkGhost_Start;
    tasks[6].elapsedTime = Ghost_PERIOD;
    tasks[6].TickFct = &TickFct_PinkGhost;

    tasks[7].period = Ghost_PERIOD;
    tasks[7].state = OrangeGhost_Start;
    tasks[7].elapsedTime = Ghost_PERIOD;
    tasks[7].TickFct = &TickFct_OrangeGhost;

    tasks[8].period = Ghost_PERIOD;
    tasks[8].state = CyanGhost_Start;
    tasks[8].elapsedTime = Ghost_PERIOD;
    tasks[8].TickFct = &TickFct_CyanGhost;

    tasks[9].period = GameState_PERIOD;
    tasks[9].state = GameState_Start;
    tasks[9].elapsedTime = GameState_PERIOD;
    tasks[9].TickFct = &TickFct_GameState;

    tasks[10].period = LED_PERIOD;
    tasks[10].state = LED_Start;
    tasks[10].elapsedTime = LED_PERIOD;
    tasks[10].TickFct = &TickFct_LED;
    
    #pragma endregion

    TimerSet(GCD_PERIOD);
    TimerOn();

    while(1){}
    return 0;
}