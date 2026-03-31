/////////////////////////////////////////////////////////////////////////////////////	 
//本程序电路只提供学习，项目参考使用，未经公司允许倒卖资料，商业使用等其他任何使用
//公司已申获知识产权证书，如有侵权行为本公司必将追究责任
//BMS电池管理开发板
//日期:2019/11/20
//版本:V1.2
//上海巴亿电子科技有限公司					  
/////////////////////////////////////////////////////////////////////////////////////

#ifndef __TIMER_H
#define __TIMER_H
#include "sys.h"



typedef struct {	
				
        uint8_t   Tim10ms_count; 	// 10ms时钟标志
        uint8_t   Tim10ms_flag;   // 10ms标志
        uint8_t   Tim100ms_count; //100ms计数
	      uint8_t   Tim100ms_flag;  //100ms事件标志位
        uint8_t   Tim200ms_flag;  //200ms事件标志位
	      uint8_t   Tim200ms_count; //200ms计数
        uint8_t   Tim500ms_count; //500ms计数
        uint8_t   Tim500ms_flag; //500ms事件标志位
        uint8_t   Tim1s_count;  //1s计数
        uint8_t   Tim1s_flag ;  //1s事件标志位
        uint8_t   Tim10s_count; //10s计数
        uint8_t   Tim10s_flag ; //10s事件标志位
        uint8_t   Tim1min_count; //1计数
        uint8_t   Tim1min_flag ; //1分钟事件标志位
	   }TaskTime;

#define  TaskTime_DEFAULTS  {0,0,0,0,0,0,0,0,0,0,0,0,0,0}  // 初始化参数
extern  TaskTime       TaskTimePare;  

//*******************全局变量******************//




//*******************************************//
void  Tim1_Init(u16 arr,u16 psc);
void Systim_Time_Run(void);
void Clear_flag(void);   //清除事件标志位


#endif
