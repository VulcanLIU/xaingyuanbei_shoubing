/*
 * GccApplication68.cpp
 *
 * Created: 2017/11/18 12:12:02
 * Author : Vulcan
 */ 

#define F_CPU 8000000UL
#define  BAUD 9600
#define Zero_POS (4*128+111-1)//零点位置是 第Zero_POS的第五位
#define  RX_begin_byte 0xff

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "Adafruit_SSD1306.cpp"

Adafruit_SSD1306 SSD1306;

void USART_init();
void USART_send(int);
uint8_t RX_group[3];
bool RX_flag;//是否接收到手柄发过来的数组
int8_t RX_Index=0;
bool RX_COMP_flag=0;

void INT0_init();
void INT1_init();
void INT2_init();

void display ();
void Write_Knum_to_Buffer(int,uint8_t);
void Write_X_to_Buffer(int);
void Write_Pic_to_Buffer(uint8_t*,int);
void Buffer_Left_Scroll();

int kp = 6;
int ki = 0;
int X=0;
uint8_t v1,v2;
bool Kmun_flag=0;
uint8_t pwm1_base=200,pwm2_base=200;
int Page0_Icon_Index = 0;// Page0 选中项目的编号 INT1 index++； INT2 index--；
char INT2_counts=0xff;//记录INT2的中断发生次数
uint8_t num [11][8]={	
	{0x00,0x7E,0xFF,0x91,0x89,0xFF,0x7E,0x00},//"0"
	{0x00,0x00,0x04,0x06,0xFF,0xFF,0x00,0x00},//"1"
	{0x00,0xC6,0xE7,0xB1,0x99,0x8F,0x86,0x00},//"2"
	{0x00,0x66,0xC3,0x91,0x91,0xFF,0x7E,0x00},//"3"
	{0x00,0x30,0x3E,0x27,0x20,0xFC,0xFC,0x20},//"4"
	{0x00,0x8F,0x8F,0x89,0xC9,0x79,0x31,0x00},//"5"
	{0x00,0x78,0xFC,0x8E,0x8B,0xF9,0x70,0x00},//"6"
	{0x00,0x03,0xC1,0xF1,0x3D,0x0F,0x03,0x00},//"7"
	{0x00,0x76,0xFF,0x8D,0x99,0xFF,0x76,0x00},//"8"
	{0x00,0x0E,0x9F,0xD1,0x71,0x3F,0x1E,0x00}, //"9"
	{0x00,0x10,0x10,0x10,0x10,0x10,0x10,0x00}//"-"
	};
	
uint8_t icon [2][8] = {
	{0x00,0x00,0x00,0x7F,0x3E,0x1C,0x08,0x00},//指向箭头
	{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00} //8*8空白
};
uint8_t item [4][16]={
	{0x00,0xFF,0xFF,0x08,0x3E,0xF7,0xC1,0x00,0xFE,0x22,0x22,0x1C,0x00,0x24,0x00,0x00},//"kp:"
	{0x00,0xFF,0xFF,0x08,0x3E,0xF7,0xC1,0x00,0x88,0xFA,0x80,0x00,0x24,0x00,0x00,0x00},//"ki:"
	{0x00,0xFF,0xFF,0x11,0x11,0x1F,0x0E,0x00,0x88,0xF8,0x80,0x00,0x24,0x00,0x00,0x00},//"P1:"
	{0x00,0xFF,0xFF,0x11,0x11,0x1F,0x0E,0x00,0xC8,0xA8,0x90,0x00,0x24,0x00,0x00,0x00},//"P2:"
};

int main(void)
{
	SSD1306.begin(0x3c);
	USART_init();
	INT0_init();
	INT1_init();
	INT2_init();
	Page0_Icon_Index = 0;
	SSD1306.display();
	PORTD |=0x01;
    while (1) 
    {
		if (Kmun_flag==1)
		{ 
			//发送数据 
			USART_send(RX_begin_byte);
			USART_send(kp>>8);	//——byte0
			USART_send(kp);		//——byte1
			USART_send(ki>>8);	//——byte2
			USART_send(ki);		//——byte3
			USART_send(pwm1_base);//——byte4
			USART_send(pwm2_base);//——byte5
			
			Kmun_flag = 0;
		}
		if (RX_COMP_flag==1)
		{
			//把从遥控端发来的数据搬运到内部的储存器中
			X=RX_group[0];
			v1=RX_group[1];
			v2=RX_group[2];

			RX_COMP_flag=0;
		}
		display();
    }   
}

void USART_init()
{
	UCSRB =(1<<RXEN)|(1<<TXEN)|(1<<RXCIE);
	UCSRC =(1<<URSEL)|(1<<UCSZ1)|(1<<UCSZ0);
	UBRRL =51;
}

void INT0_init()
{
	SREG |=0X80;
	PORTD |=0XFF;
	MCUCR |=(1<<ISC01);
	GICR |=(1<<INT0);
}

void INT1_init()
{
	SREG |=0X80;
	PORTD |=0XFF;//端口内部上拉
	DDRA |=0xff;//端口输出低电平接地
	MCUCR |=(1<<ISC11);
	GICR |=(1<<INT1);
}

void INT2_init()
{
	PORTB|=(1<<PORTB2);//端口内部上拉
	MCUCSR|=(1<<ISC2);
	GICR |=(1<<INT2);
}

void USART_send(int Knum)
{
	while ( !( UCSRA & (1<<UDRE)) );
	UDR = Knum;
}

void display ()
{
	/*画小箭头*/
	if (Page0_Icon_Index %2==0)
	{
		Write_Pic_to_Buffer(icon[0],0);
		Write_Pic_to_Buffer(icon[1],66);
	}
	else
	{
		Write_Pic_to_Buffer(icon[0],66);
		Write_Pic_to_Buffer(icon[1],0);
	}
	/*画字和冒号 写上数据*/
	if (Page0_Icon_Index==0||Page0_Icon_Index==1)
	{
		Write_Pic_to_Buffer(item[0],8);
		Write_Pic_to_Buffer(item[1],74);
		Write_Knum_to_Buffer(kp,24);
		Write_Knum_to_Buffer(ki,90);
	}
	if (Page0_Icon_Index==2||Page0_Icon_Index==3)
	{
		Write_Pic_to_Buffer(item[2],8);
		Write_Pic_to_Buffer(item[3],74);
		Write_Knum_to_Buffer(pwm1_base,24);
		Write_Knum_to_Buffer(pwm2_base,90);
	}
	if (Page0_Icon_Index==4||Page0_Icon_Index==5)
	{
		Write_Pic_to_Buffer(icon[1],8);
		Write_Pic_to_Buffer(icon[1],74);
		Write_Knum_to_Buffer(v1,24);
		Write_Knum_to_Buffer(v2,90);
	}
	Buffer_Left_Scroll();
	Write_X_to_Buffer(X);
	SSD1306.display();
}

void Write_Knum_to_Buffer(int Knum,uint8_t position)
{
	uint8_t numgroup[4];//num1,num2,num3,num4;
	numgroup[0] = Knum/1000;
	numgroup[1] = (Knum/100)%10;
	numgroup[2] = (Knum/10)%10;
	numgroup[3] = Knum%10;
	
	for (int i = 0;i<4;i++)
	{
		for (int j = 0;j<8;j++)
		{
			SSD1306.buffer [position+i*8+j] = num[numgroup[i]][j];
		}
	}
}

void Write_X_to_Buffer(int x_pos)
{
	for (int j = 0;j<8;j++)
	{
		SSD1306.buffer [128*4+115+j] = num[abs(x_pos)][j];
	}
	if (x_pos%2)
	{
		SSD1306.buffer [Zero_POS-128*(x_pos+1)/2] = 0xC0;
	}
	else
	{
		SSD1306.buffer [Zero_POS-128*x_pos/2] = 0x0C;
	}
}
void Write_Pic_to_Buffer(uint8_t *PIC,int pos)
{
	for (int j = 0;j<16;j++)
	{
		SSD1306.buffer [pos+j] = PIC[j];
	}
}
void Buffer_Left_Scroll()
{
	for (int page=1;page<=7;page++)
	{
		for (int column=0;column<111;column++)
		{
			SSD1306.buffer [page*128+column]=SSD1306.buffer [page*128+column+1];
		}
	}
}
ISR (INT0_vect)
{
	if ((PIND & 0x10)==0)
	{
		if (Page0_Icon_Index==0){kp++;}
		if (Page0_Icon_Index==1){ki++;}
		if (Page0_Icon_Index==2){pwm1_base++;}
		if (Page0_Icon_Index==3){pwm2_base++;}
	}
	else 
	{
		if (Page0_Icon_Index==0){kp--;}
		if (Page0_Icon_Index==1){ki--;}
		if (Page0_Icon_Index==2){pwm1_base--;}
		if (Page0_Icon_Index==3){pwm2_base--;}
	}
	if (kp<0){kp=0;}
	if (ki<0){ki=0;}
	Kmun_flag=1;
}
ISR (INT1_vect)
{
	Page0_Icon_Index --;
	if (Page0_Icon_Index>5)
	{
		Page0_Icon_Index -=6;
	}
	if (Page0_Icon_Index<0)
	{
		Page0_Icon_Index +=6;
		
	}
	PORTD =(PORTD & 0x7F)|((~PORTD)&0x80);
}
ISR (INT2_vect)
{
	if (INT2_counts==0x00)
	{
	Page0_Icon_Index ++;
	if (Page0_Icon_Index>5)
	{
		Page0_Icon_Index -=6;
	}
	if (Page0_Icon_Index<0)
	{
		Page0_Icon_Index +=6;
	}
	}
    INT2_counts =~INT2_counts;
}

ISR (USART_RXC_vect)
 {
 	int UDR_TEMP = UDR;
 	if ((RX_flag == 1)&&(RX_Index>=0))
	{
		RX_group[RX_Index]=UDR_TEMP;
		RX_Index++;
 	}
 	if(RX_Index>=3)
 	{
		RX_Index = 0;//数组指针置零
		RX_flag = 0;//起始帧标志位清零
		RX_COMP_flag=1;//4个8bit接收完成
 	}
 	if (UDR_TEMP==RX_begin_byte){RX_flag = 1;}//接收到起始帧
}