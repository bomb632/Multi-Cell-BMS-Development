/////////////////////////////////////////////////////////////////////////////////////	 
//本程序电路只提供学习，项目参考使用，未经公司允许倒卖资料，商业使用等其他任何使用
//公司已申获知识产权证书，如有侵权行为本公司必将追究责任
//BMS电池管理开发板
//日期:2019/11/20
//版本:V1.2
//上海巴亿电子科技有限公司					  
/////////////////////////////////////////////////////////////////////////////////////

#ifndef __ADC_H
#define __ADC_H	
#include "sys.h"

#define   ADC1_DR_Address    0x4001244C 
#define  ADCSamp_DEFAULTS     0        // 初始化参数

typedef struct {
unsigned char ia;
}ADCSamp;
extern   ADCSamp     ADCSampPare;






void ADC_Init_Sam(void);
void DMA_Init_Adc(void);
void ADC_GetSample(void);
signed int TemChange(unsigned int uiADCV);



#endif 



