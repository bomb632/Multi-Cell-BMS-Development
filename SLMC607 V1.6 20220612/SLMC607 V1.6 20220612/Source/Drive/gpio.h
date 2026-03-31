/////////////////////////////////////////////////////////////////////////////////////	 
//本程序电路只提供学习，项目参考使用，未经公司允许倒卖资料，商业使用等其他任何使用
//公司已申获知识产权证书，如有侵权行为本公司必将追究责任
//BMS电池管理开发板
//日期:2019/11/20
//版本:V1.2
//上海巴亿电子科技有限公司					  
/////////////////////////////////////////////////////////////////////////////////////

#ifndef __GPIO_H
#define __GPIO_H	 
#include "sys.h"

#define  SYS_POWER_ON         PBout(5)=1 
#define  SYS_POWER_OFF        PBout(5)=0 
#define  BQ769xx_WAKE_ON      PBout(3)=1
#define  BQ769xx_WAKE_OFF     PBout(3)=0 

void Init_GPIO_SYS_Power(void); 
void Init_GPIO_LED(void);
void Init_GPIO_KEY(void);
void Init_GPIO_IN0(void);
void Init_GPIO_OUT0(void);
void Init_GPIO_ALERT(void);
void Init_GPIO_BQ769xx_Wake(void);
void Init_GPIO_R485Pwr(void);
void Init_GPIO_R485EN(void);
void Init_GPIO_CanPwr(void);
void Init_GPIO_IIC(void ); 
void Init_GPIO_CAN(void ); 

void Init_GPIO_ChargeActiva(void);
u16 ChargeActiva_Read_Gpio(u8 *activa);
#endif
