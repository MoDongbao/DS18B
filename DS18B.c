#include <stdio.h>
#include "reg51.h"
#include "intrins.h"
#include "onewire.h"

typedef unsigned char BYTE;
typedef unsigned int WORD;

#define FOSC 11059200L          //系统频率
#define BAUD 115200             //串口波特率

#define NONE_PARITY     0       //无校验
#define ODD_PARITY      1       //奇校验
#define EVEN_PARITY     2       //偶校验
#define MARK_PARITY     3       //标记校验
#define SPACE_PARITY    4       //空白校验

#define PARITYBIT NONE_PARITY   //定义校验位

sfr P0M1 = 0x93;
sfr P0M0 = 0x94;
sfr P1M1 = 0x91;
sfr P1M0 = 0x92;
sfr P2M1 = 0x95;
sfr P2M0 = 0x96;
sfr P3M1 = 0xb1;
sfr P3M0 = 0xb2;
sfr P4M1 = 0xb3;
sfr P4M0 = 0xb4;
sfr P5M1 = 0xC9;
sfr P5M0 = 0xCA;
sfr P6M1 = 0xCB;
sfr P6M0 = 0xCC;
sfr P7M1 = 0xE1;
sfr P7M0 = 0xE2;

sfr AUXR  = 0x8e;               //辅助寄存器

sfr P_SW1   = 0xA2;             //外设功能切换寄存器1

#define S1_S0 0x40              //P_SW1.6
#define S1_S1 0x80              //P_SW1.7

sbit P22 = P2^2;
bit busy;

unsigned char smgtab[17]={0xc0,0xf9,0xa4,0xb0,0x99,0x92,0x82,0xf8,0x80,0x90,0x88,0x83,0xc6,0xa1,0x86,0x8e,0xff};
unsigned char smg[8]={0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
unsigned char smgcount;
char str[30];
unsigned int ds18b20_temp;

unsigned int ds18b20_h = 25;
unsigned int ds18b20_l = 14;

sbit Key_S = P3^0;
sbit Key_Add = P3^2;
sbit Key_Dec = P3^3;
char setting_mode = 0;

void SendData(BYTE dat);
void SendString(char *s);
void smgdeal(void);
void smgdisplay(unsigned char i);

void Delay500ms()		//@12.000MHz
{
	unsigned char i, j, k;

	_nop_();
	_nop_();
	i = 23;
	j = 205;
	k = 120;
	do
	{
		do
		{
			while (--k);
		} while (--j);
	} while (--i);
}


/*================== 定时器函数 ==========================*/
void Timer0Init(void)		//2毫秒@12.000MHz
{

	AUXR &= 0x7F;		//定时器时钟12T模式
	TMOD &= 0xF0;		//设置定时器模式
	TL0 = 0x30;		  //设置定时初值
	TH0 = 0xF8;		  //设置定时初值
	TF0 = 0;		    //清除TF0标志
	TR0 = 1;		    //定时器0开始计时
	ET0 = 1;
	EA  = 1; 
}

void main()
{
    P0M0 = 0x00;
    P0M1 = 0x00;
    P1M0 = 0x00;
    P1M1 = 0x00;
    P2M0 = 0x00;
    P2M1 = 0x00;
    P3M0 = 0x00;
    P3M1 = 0x00;
    P4M0 = 0x00;
    P4M1 = 0x00;
    P5M0 = 0x00;
    P5M1 = 0x00;
    P6M0 = 0x00;
    P6M1 = 0x00;
    P7M0 = 0x00;
    P7M1 = 0x00;

    ACC = P_SW1;
    ACC &= ~(S1_S0 | S1_S1);    //S1_S0=0 S1_S1=0
    P_SW1 = ACC;                //(P3.0/RxD, P3.1/TxD) 

#if (PARITYBIT == NONE_PARITY)
    SCON = 0x50;                //8位可变波特率
#elif (PARITYBIT == ODD_PARITY) || (PARITYBIT == EVEN_PARITY) || (PARITYBIT == MARK_PARITY)
    SCON = 0xda;                //9位可变波特率,校验位初始为1
#elif (PARITYBIT == SPACE_PARITY)
    SCON = 0xd2;                //9位可变波特率,校验位初始为0
#endif

    AUXR = 0x40;                //定时器1为1T模式
    TMOD = 0x00;                //定时器1为模式0(16位自动重载)
    TL1 = (65536 - (FOSC/4/BAUD));   //设置波特率重装值
    TH1 = (65536 - (FOSC/4/BAUD))>>8;
    TR1 = 1;                    //定时器1开始启动
    ES = 1;                     //使能串口中断
    EA = 1;

    SendString("STC15F2K60S2\r\nUart Test !\r\n");
		Timer0Init();
    while(1)
		{
			ES = 1;
			ds18b20_temp = rd_temperature();
			sprintf(str, "temp is %d\r\n", ds18b20_temp);
			SendString(str);
			ES = 0;
			if(Key_S == 0)
			{
				setting_mode = (setting_mode+1) % 2;
				Delay500ms();
			}
			if(setting_mode == 1)
			{
				if(Key_Add == 0)
				{
					ds18b20_h++;
					Delay500ms();
				}
				else if(Key_Dec == 0)
				{
					ds18b20_h--;
					Delay500ms();
				}
			}
			if(setting_mode == 0)
			{
				if(Key_Add == 0)
				{
					ds18b20_l++;
					Delay500ms();
				}
				else if(Key_Dec == 0)
				{
					ds18b20_l--;
					Delay500ms();
				}
			}
		}
}

/*----------------------------
UART 中断服务程序
-----------------------------*/
void Uart() interrupt 4
{
    if (RI)
    {
        RI = 0;                 //清除RI位
        P0 = SBUF;              //P0显示串口数据
        P22 = RB8;              //P2.2显示校验位
    }
    if (TI)
    {
        TI = 0;                 //清除TI位
        busy = 0;               //清忙标志
    }
}

/*----------------------------
发送串口数据
----------------------------*/
void SendData(BYTE dat)
{
    while (busy);               //等待前面的数据发送完成
    ACC = dat;                  //获取校验位P (PSW.0)
    if (P)                      //根据P来设置校验位
    {
#if (PARITYBIT == ODD_PARITY)
        TB8 = 0;                //设置校验位为0
#elif (PARITYBIT == EVEN_PARITY)
        TB8 = 1;                //设置校验位为1
#endif
    }
    else
    {
#if (PARITYBIT == ODD_PARITY)
        TB8 = 1;                //设置校验位为1
#elif (PARITYBIT == EVEN_PARITY)
        TB8 = 0;                //设置校验位为0
#endif
    }
    busy = 1;
    SBUF = ACC;                 //写数据到UART数据寄存器
}

/*----------------------------
发送字符串
----------------------------*/
void SendString(char *s)
{
    while (*s)                  //检测字符串结束标志
    {
        SendData(*s++);         //发送当前字符
    }
}

void  seviceTimer0(void) interrupt 1
{
  smgdeal();
	smgdisplay(smgcount);
  smgcount++;
	if(smgcount==8)
	{
	   smgcount=0;
	}
}

/*================== 数码管显示函数 ==========================*/
void smgdeal(void)
{
		if((ds18b20_temp/10) >= ds18b20_h)
		{
		  smg[0]=smgtab[15];//F
		}
		else if((ds18b20_temp/10) < ds18b20_l)
		{
		  smg[0]=smgtab[13];//d
		}
		else
		{
			smg[0]=smgtab[16];//消隐
		}
		
		if(setting_mode == 1)
		{
			smg[1]=smgtab[15];
			smg[2]=smgtab[ds18b20_h/10];
			smg[3]=smgtab[ds18b20_h%10];
		}
		else
		{
			smg[1]=smgtab[13];
			smg[2]=smgtab[ds18b20_l/10];
			smg[3]=smgtab[ds18b20_l%10];		
		}
		
   smg[4]=smgtab[ds18b20_temp/100];
   smg[5]=smgtab[ds18b20_temp%100/10]&0x7f;
   smg[6]=smgtab[ds18b20_temp%10];
	 smg[7]=smgtab[12];//C
}

void smgdisplay(unsigned char i)
{ 
  P2=(P2&0x1f)|0xE0;
  P0=smg[i];
  P2=(P2&0x1f)|0xc0;
  P0=0x01<<i;  
}


