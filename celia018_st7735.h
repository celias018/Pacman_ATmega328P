#ifndef ST7735_H
#define ST7735_H

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "spiAVR.h"
#include <avr/pgmspace.h>
#include <stdio.h>

//ctrl pin definitions
#define TFT_CS PB2
#define TFT_DC PD7
#define TFT_RST PD6

//low lvl pin ctrl
#define CS_LOW()    PORTB &= ~(1 << TFT_CS)
#define CS_HIGH()   PORTB |=  (1 << TFT_CS)

#define DC_CMD()    PORTD &= ~(1 << TFT_DC)
#define DC_DATA()   PORTD |=  (1 << TFT_DC)

#define RST_LOW()   PORTD &= ~(1 << TFT_RST)
#define RST_HIGH()  PORTD |=  (1 << TFT_RST)


//(x,y)
typedef struct _position{
    unsigned char x;
    unsigned char y;
} Position;

//width,length,image
typedef struct _sprite{
	unsigned char width; 		//total width, this is the x coord
	unsigned char length; 	    //total length, this si the y coord
    const uint16_t* image;
	
} Sprite;

const uint8_t font5x7[][5] PROGMEM = {
    //only digits + colon + space for score
    {0x00,0x00,0x00,0x00,0x00},//space
    {0x3E,0x51,0x49,0x45,0x3E},//0
    {0x00,0x42,0x7F,0x40,0x00},//1
    {0x42,0x61,0x51,0x49,0x46},//2
    {0x21,0x41,0x45,0x4B,0x31},//3
    {0x18,0x14,0x12,0x7F,0x10},//4
    {0x27,0x45,0x45,0x45,0x39},//5
    {0x3C,0x4A,0x49,0x49,0x30},//6
    {0x01,0x71,0x09,0x05,0x03},//7
    {0x36,0x49,0x49,0x49,0x36},//8
    {0x06,0x49,0x49,0x29,0x1E},//9
    {0x7E,0x11,0x11,0x11,0x7E},//A
    {0x7F,0x49,0x49,0x49,0x36},//B
    {0x3E,0x41,0x41,0x41,0x22},//C
    {0x7F,0x41,0x41,0x22,0x1C},//D
    {0x7F,0x49,0x49,0x49,0x41},//E
    {0x7F,0x09,0x09,0x09,0x01},//F
    {0x3E,0x41,0x49,0x49,0x7A},//G
    {0x7F,0x08,0x08,0x08,0x7F},//H
    {0x00,0x41,0x7F,0x41,0x00},//I
    {0x20,0x40,0x41,0x3F,0x01},//J
    {0x7F,0x08,0x14,0x22,0x41},//K
    {0x7F,0x40,0x40,0x40,0x40},//L
    {0x7F,0x02,0x04,0x02,0x7F},//M
    {0x7F,0x04,0x08,0x10,0x7F},//N
    {0x3E,0x41,0x41,0x41,0x3E},//O
    {0x7F,0x09,0x09,0x09,0x06},//P
    {0x3E,0x41,0x51,0x21,0x5E},//Q
    {0x7F,0x09,0x19,0x29,0x46},//R
    {0x46,0x49,0x49,0x49,0x31},//S
    {0x01,0x01,0x7F,0x01,0x01},//T
    {0x3F,0x40,0x40,0x40,0x3F},//U
    {0x1F,0x20,0x40,0x20,0x1F},//V
    {0x7F,0x20,0x18,0x20,0x7F},//W
    {0x63,0x14,0x08,0x14,0x63},//X
    {0x03,0x04,0x78,0x04,0x03},//Y
    {0x61,0x51,0x49,0x45,0x43} //Z
};
//write functions
void TFT_WriteCommand(uint8_t cmd) {
    DC_CMD();

    CS_LOW();
    SPI_SEND(cmd);
    CS_HIGH();
}

void TFT_WriteData(uint8_t data) {
    DC_DATA();

    CS_LOW();
    SPI_SEND(data);
    CS_HIGH();
}

//reset display
void TFT_Reset() {
    RST_HIGH();
    _delay_ms(5);

    RST_LOW();
    _delay_ms(20);

    RST_HIGH();
    _delay_ms(150);
}

//init display
void TFT_Init() {
    //dc and rst pins 
    DDRD |= (1 << TFT_DC) | (1 << TFT_RST);

    //cs pin
    DDRB |= (1 << TFT_CS);

    CS_HIGH();

    SPI_INIT();

    //hardware rst
    TFT_Reset();

    //software rst
    TFT_WriteCommand(0x01);
    _delay_ms(150);

    //sleep 
    TFT_WriteCommand(0x11);
    _delay_ms(150);

    //color mode
    TFT_WriteCommand(0x3A);
    TFT_WriteData(0x05);

    //mem access ctrl
    TFT_WriteCommand(0x36);
    TFT_WriteData(0x00);

    //display on
    TFT_WriteCommand(0x29);
    _delay_ms(100);
}

//set draw window 
void TFT_SetAddrWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    //offsets
    x0 += 2;
    x1 += 2;

    y0 += 1;
    y1 += 1;

    //column address set
    TFT_WriteCommand(0x2A);

    TFT_WriteData(0x00);
    TFT_WriteData(x0);

    TFT_WriteData(0x00);
    TFT_WriteData(x1);

    //row address set
    TFT_WriteCommand(0x2B);

    TFT_WriteData(0x00);
    TFT_WriteData(y0);

    TFT_WriteData(0x00);
    TFT_WriteData(y1);

    //write to RAM
    TFT_WriteCommand(0x2C);
}

//draw pixel
void TFT_DrawPixel(uint8_t x, uint8_t y, uint16_t color) {
    TFT_SetAddrWindow(x, y, x, y);
    TFT_WriteData(color >> 8);
    TFT_WriteData(color & 0xFF);
}

//fill screen
void TFT_FillScreen(uint16_t color) {
    uint16_t i;

    TFT_SetAddrWindow(0, 0, 127, 159); //for 128 x 160 screen
    DC_DATA();
    CS_LOW();

    for(i = 0; i < 128UL * 160UL; i++)
    {
        SPI_SEND(color >> 8);
        SPI_SEND(color & 0xFF);
    }

    CS_HIGH();
}

void TFT_DrawSprite(Sprite* s, unsigned char drawX, unsigned char drawY){
    unsigned char x;
    unsigned char y;

    TFT_SetAddrWindow(drawX, drawY, drawX + s->width - 1, drawY + s->length - 1);

    DC_DATA();
    CS_LOW();

    for(y = 0; y < s->length; y++){
        for(x = 0; x < s->width; x++){
            uint16_t color = pgm_read_word(&(s->image[(y * s->width) + x]));
            if(color == 0x0000){
                // send background color instead
                SPI_SEND(0x00);
                SPI_SEND(0x00);
            }
            else{
                SPI_SEND(color >> 8);
                SPI_SEND(color & 0xFF);
            }
        }
    }
    CS_HIGH();
}

void TFT_DrawRectangle(unsigned char drawX, unsigned char drawY, unsigned char width, unsigned char length, uint16_t color){
    unsigned int i;
    // define drawing area
    TFT_SetAddrWindow(drawX, drawY, drawX + width - 1, drawY + length - 1);

    DC_DATA();
    CS_LOW();

    for(i = 0; i < (width * length); i++){
        SPI_SEND(color >> 8);
        SPI_SEND(color & 0xFF);
    }

    CS_HIGH();
}
void TFT_DrawChar(uint8_t x, uint8_t y, char c, uint16_t color, uint16_t bg){
    uint8_t index;

    if(c == ' ') index = 0;
    else if(c >= '0' && c <= '9') index = c - '0' + 1;
    else if(c >= 'A' && c <= 'Z') index = c - 'A' + 11;
    else return;

    for(uint8_t col = 0; col < 5; col++){
        uint8_t line = pgm_read_byte(&font5x7[index][col]);
        for(uint8_t row = 0; row < 7; row++){
            if(line & (1 << row)){
                TFT_DrawPixel(x + col, y + row, color);
            }
            else{
                TFT_DrawPixel(x + col, y + row, bg);
            }
        }
    }
    for(uint8_t row = 0; row < 7; row++){//collum for spacing
        TFT_DrawPixel(x + 5, y + row, bg);
    }
}
void TFT_DrawText(uint8_t x, uint8_t y, char* str, uint16_t color, uint16_t bg){
    while(*str){
        TFT_DrawChar(x, y, *str, color, bg);
        x += 6;
        str++;
    }
}

//colors
#define ST7735_BLACK   0x0000
#define ST7735_WHITE   0xFFFF
#define ST7735_RED     0xF800
#define ST7735_GREEN   0x07E0
#define ST7735_BLUE    0x001F
#define ST7735_YELLOW  0xFFE0
#define ST7735_CYAN    0x07FF
#define ST7735_MAGENTA 0xF81F
#define ST7735_LIGHT_YELLOW 0xFFF6


#endif /* ST7735_H */