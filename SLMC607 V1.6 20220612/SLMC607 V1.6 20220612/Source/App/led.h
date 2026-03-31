/////////////////////////////////////////////////////////////////////////////////////	 
//本程序电路只提供学习，项目参考使用，未经公司允许倒卖资料，商业使用等其他任何使用
//公司已申获知识产权证书，如有侵权行为本公司必将追究责任
//BMS电池管理开发板
//日期:2019/11/20
//版本:V1.2
//上海巴亿电子科技有限公司					  
/////////////////////////////////////////////////////////////////////////////////////

#ifndef __LED_H
#define __LED_H	 
#include "sys.h"


typedef enum 
{
	LED_R=0,
	LED_G,
	LED_Y,
  BUZZER,
	LED_MAX
}	eLED_VOLE;

typedef enum 
{
	OUT1_GPIO=0,
	OUT2_GPIO,
	OUT3_GPIO,
	OUT_MAX
}	eOUT_VOLE;


typedef enum 
{
	IN1_GPIO=0,
	IN2_GPIO,
	IN3_GPIO,
	IN_MAX
}	eIN_VOLE;

#define  GPIO_DATA_LOW   0
#define  GPIO_DATA_HIGH  1



u8 Led_Run_Cpu(void);
void LedRun_Set_On(void);
void LedRun_Set_Off(void);
void LedRun_Set_Not(void);
u16 LedBuzz_SetPort(u8 led_ch, u8 value);
u16 OutGpio_SetPort(u8 out_ch, u8 value);
u16 InGpio_GetPort(u8 in_ch, u8* value);
void LEDgpio_test(void);


#endif


