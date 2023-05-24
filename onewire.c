/*
  程序说明: 单总线驱动程序
  软件环境: Keil uVision 4.10 
  硬件环境: CT107单片机综合实训平台(外部晶振12MHz) STC89C52RC单片机
  日    期: 2011-8-9
*/
#include "reg52.h"

sbit DQ = P1^4;  //单总线接口

//单总线延时函数
void Delay_OneWire(unsigned int t)  //STC89C52RC
{
	while(t--);
}

//通过单总线向DS18B20写一个字节
void Write_DS18B20(unsigned char dat)
{
	unsigned char i;
	for(i=0;i<8;i++)
	{
		DQ = 0;
		DQ = dat&0x01;
		Delay_OneWire(50);
		DQ = 1;
		dat >>= 1;
	}
	Delay_OneWire(50);
}

//从DS18B20读取一个字节
unsigned char Read_DS18B20(void)
{
	unsigned char i;
	unsigned char dat;
  
	for(i=0;i<8;i++)
	{
		DQ = 0;
		dat >>= 1;
		DQ = 1;
		if(DQ)
		{
			dat |= 0x80;
		}	    
		Delay_OneWire(50);
	}
	return dat;
}

//DS18B20设备初始化
bit init_ds18b20(void)
{
  	bit initflag = 0;
  	
  	DQ = 1;
  	Delay_OneWire(120);
  	DQ = 0;
  	Delay_OneWire(800);
  	DQ = 1;
  	Delay_OneWire(100); 
    initflag = DQ;     
  	Delay_OneWire(50);
  
  	return initflag;
}

unsigned int rd_temperature(void)
{
	unsigned char low, high;
	unsigned int temp;
	
	init_ds18b20();									//ds18b20温度传感器复位
	Write_DS18B20(0xCC);							//写入字节0xCC，跳过ROM指令
	Write_DS18B20(0x44);							//开始温度转换
	Delay_OneWire(1000);
	
	init_ds18b20();
	Write_DS18B20(0xCC);
	Write_DS18B20(0xBE);							//读取高速暂存器
	
	low = Read_DS18B20();							//读取暂存器的第0字节
	high = Read_DS18B20();							//读取暂存器的第1个字节
	
	temp = high;
  temp = (temp<<8) | low;
	
	if((temp&0xf800) == 0x0000)				        //通过温度数据的高5位判断采用结果是正温度还是负温度
	{
	  temp >>= 4;
	  temp = temp*10;
	  temp = temp+(low&0x0f)*0.0625*10;
	}
	return temp;
}
