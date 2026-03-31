/////////////////////////////////////////////////////////////////////////////////////	 
//本程序电路只提供学习，项目参考使用，未经公司允许倒卖资料，商业使用等其他任何使用
//公司已申获知识产权证书，如有侵权行为本公司必将追究责任
//BMS电池管理开发板
//日期:2019/11/20
//版本:V1.2
//上海巴亿电子科技有限公司					  
/////////////////////////////////////////////////////////////////////////////////////

#ifndef __USART_H
#define __USART_H
#include "stdio.h"	
#include "sys.h" 

#define  RS485_Rce_OFF     PBout(15)=1 
#define  RS485_Rce_ON      PBout(15)=0 

#define UARTNunD       0xFF 
#define START_SYMBOL   0xA8







void Uart1_init(u32 bound);
void Uart1Send(unsigned char STData,unsigned char FunCom,unsigned char LengByte,unsigned char *Data);
void Uart1Run_Pack(void);
void Uart2_init(u32 bound);
void Uart2Send(unsigned char STData,unsigned char FunCom,unsigned char LengByte,unsigned char *Data);
void Uart2Run_Pack(void);
void Usart1SendData(unsigned char Data);
void Usart2SendData(unsigned char Data);

void  Uart1DataDropChk(void);  //串口1检测超时
void  Uart2DataDropChk(void);  //串口2检测超时

#endif


