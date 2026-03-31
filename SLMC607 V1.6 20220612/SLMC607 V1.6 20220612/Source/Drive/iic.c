/////////////////////////////////////////////////////////////////////////////////////
// 本源代码仅供学习为目的参考使用，未经本公司书面授权，禁止商业使用或其他用途
// 本公司保留知识版权证明，追究版权为我公司保留追究权利
// BMS相关源代码程序
// 日期:2019/11/20
// 版本:V1.2
// 上海芯联芯电子科技有限公司
/////////////////////////////////////////////////////////////////////////////////////

/**
 * @file    iic.c
 * @brief   IIC软件模拟驱动
 * @details 实现基于GPIO的软件模拟IIC通信
 *          - 支持标准IIC协议
 *          - CRC8校验（多项式0x07）
 *          - 单字节和多字节读写
 *          - 用于与BQ769xx通信
 *
 *          引脚映射：
 *          - PB6: IIC_SCK (时钟线)
 *          - PB7: IIC_SDA (数据线，开漏输出)
 *
 *          IIC时序：
 *          - SCL高电平时，SDA下降沿为起始信号
 *          - SCL高电平时，SDA上升沿为停止信号
 *          - 数据在SCL低电平时变化，高电平时采样
 */

#include "iic.h"
#include "gpio.h"
#include "TaskFun.h"
#include "timer.h"
#include "adc.h"
#include "Flash.h"

/* ==================== 全局变量定义 ==================== */
char HW_VERSION[24] = {"硬件版本:1.1    \0"};  // 硬件版本号
char SW_VERSION[24] = {"软件版本:1.1    \0"};  // 软件版本号
char DE_VERSION[24] = {"设备版本:2.0    \0"};  // 设备版本号

/* ==================== 局部变量定义 ==================== */

/* ==================== GPIO方向控制 ==================== */

/**
 * @brief 设置SDA为输出模式
 * @details SDA线需要双向传输
 *          - 发送数据时：推挽输出模式
 *          - 接收ACK时：浮空输入模式
 */
void Sda_Set_Out_Mode(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;    // 开漏输出模式（支持双向）
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/**
 * @brief 设置SDA为输入模式
 * @details 在接收ACK位时，SDA需要切换为输入模式
 *          从机在ACK时将SDA拉低
 */
void Sda_Set_In_Mode(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;  // 浮空输入模式
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/* ==================== IIC底层函数 ==================== */

/**
 * @brief 检查IIC从机ACK信号
 * @return gRET_OK - 收到ACK (SDA=0)
 *         gRET_NG - 未收到ACK (SDA=1) 或超时
 * @details 发送一个字节后，第9个时钟周期读取ACK
 *          - 从机拉低SDA表示ACK
 *          - 从机保持SDA高表示NACK
 *          - 超时时间：250 * 2us = 500us
 */
unsigned char IICCheckAck(void)
{
	unsigned char ret = gRET_OK;
	unsigned char cnt = 0;

	Sda_Set_In_Mode();    // SDA切换为输入模式
	delay_us(2);
	SCK_OUT_HIGH;         // SCL=1
	delay_us(2);

	/* 等待从机拉低SDA（ACK） */
	while(SDA_IN_DATA == 1)
	{
		cnt++;
		if(cnt > 250)  // 超时检测
		{
			return gRET_NG;  // 未收到ACK
		}
	}
	SCK_OUT_LOW;         // SCL=0
	return ret;
}

/**
 * @brief 发送ACK信号（主机应答）
 * @details 主机在读取数据后发送ACK
 *          SDA拉低表示ACK，通知从机继续发送
 */
void IICSendAck(void)
{
	SDA_OUT_LOW;         // SDA=0 (ACK)
	Sda_Set_Out_Mode();  // SDA切换为输出模式
	SCK_OUT_HIGH;        // SCL=1
	delay_us(2);
	SCK_OUT_LOW;         // SCL=0
	delay_us(2);
	SDA_OUT_HIGH;        // SDA释放
}

/**
 * @brief 发送NACK信号（主机非应答）
 * @details 主机在读取最后一个字节后发送NACK
 *          SDA保持高表示NACK，通知从机停止发送
 */
void IICSendNoAck(void)
{
	SDA_OUT_HIGH;        // SDA=1 (NACK)
	Sda_Set_Out_Mode();  // SDA切换为输出模式
	SCK_OUT_HIGH;        // SCL=1
	delay_us(2);
	SCK_OUT_LOW;         // SCL=0
	delay_us(2);
	SDA_OUT_HIGH;        // SDA保持高
}

/**
 * @brief 产生IIC起始信号
 * @return gRET_OK
 * @details 时序：
 *          1. SDA=1, SCL=1
 *          2. 延时2us
 *          3. SDA=0 (下降沿)
 *          4. 延时2us
 *          5. SCL=0
 *
 * @note   起始条件：SCL高电平时，SDA的下降沿
 */
unsigned char IICStart(void)
{
	unsigned char ret = gRET_OK;

	SDA_OUT_HIGH;        // SDA=1
	SCK_OUT_HIGH;        // SCL=1
	Sda_Set_Out_Mode();  // SDA输出模式
	delay_us(2);
	SDA_OUT_LOW;         // SDA=0 (下降沿)
	delay_us(2);
	SCK_OUT_LOW;         // SCL=0
	return ret;
}

/**
 * @brief 产生IIC停止信号
 * @details 时序：
 *          1. SCL=0
 *          2. SDA=0
 *          3. 延时2us
 *          4. SCL=1
 *          5. 延时2us
 *          6. SDA=1 (上升沿)
 *
 * @note   停止条件：SCL高电平时，SDA的上升沿
 */
void IICStop(void)
{
	SCK_OUT_LOW;         // SCL=0
	SDA_OUT_LOW;         // SDA=0
	Sda_Set_Out_Mode();  // SDA输出模式
	delay_us(2);
	SCK_OUT_HIGH;        // SCL=1
	delay_us(2);
	SDA_OUT_HIGH;        // SDA=1 (上升沿)
}

/**
 * @brief IIC发送一个字节数据
 * @param  Data: 要发送的字节
 * @return gRET_OK - 发送成功并收到ACK
 *         gRET_NG - 未收到ACK
 * @details 发送流程：
 *          1. 从高位到低位依次发送8位数据
 *          2. 每位数据在SCL低电平时变化
 *          3. SCL高电平时从机采样
 *          4. 发送8位后检查ACK
 */
unsigned char IICSendByte(unsigned char Data)
{
	unsigned char ret = gRET_OK;
	unsigned char ia = 0;

	/* 发送8位数据，高位优先 */
	for(ia = 0; ia < 8; ia++)
	{
		if(Data & 0x80)  // 取最高位
		{
			SDA_OUT_HIGH;
		}
		else
		{
			SDA_OUT_LOW;
		}
		Sda_Set_Out_Mode();
		delay_us(2);
		SCK_OUT_HIGH;    // SCL=1，从机采样
		delay_us(2);
		SCK_OUT_LOW;     // SCL=0
		Data = Data << 1;  // 左移一位
	}

	/* 检查ACK */
	if(IICCheckAck() != gRET_OK)
	{
		return gRET_NG;  // 未收到ACK
	}
	return ret;
}

/**
 * @brief IIC接收一个字节数据
 * @return 接收到的字节数据
 * @details 接收流程：
 *          1. SDA切换为输入模式
 *          2. 从高位到低位依次读取8位数据
 *          3. 每位数据在SCL高电平时采样
 *          4. 读取8位后发送ACK或NACK（由调用者控制）
 */
unsigned char IICReceByte(void)
{
	unsigned char ia = 0;
	unsigned char readDATA = 0;

	Sda_Set_In_Mode();  // SDA切换为输入模式

	/* 接收8位数据，高位优先 */
	for(ia = 0; ia < 8; ia++)
	{
		SCK_OUT_HIGH;    // SCL=1
		delay_us(2);

		readDATA = readDATA << 1;  // 左移一位
		if(SDA_IN_DATA)  // 读取SDA
		{
			readDATA |= 0x01;  // 置位最低位
		}
		else
		{
			readDATA &= ~0x01;  // 清零最低位
		}
		SCK_OUT_LOW;     // SCL=0
		delay_us(2);
	}
	return readDATA;
}

/* ==================== CRC8校验函数 ==================== */

/**
 * @brief CRC8校验计算
 * @param  ptr: 数据缓冲区指针
 * @param  len: 数据长度
 * @param  key: CRC多项式（默认0x07）
 * @return CRC8校验值
 * @details CRC8算法：
 *          - 多项式：0x07 (x^8 + x^2 + x + 1)
 *          - 初始值：0x00
 *          - 用于BQ769xx通信校验
 */
u8 IIC_CRC8Bytes(u8 *ptr, u8 len, u8 key)
{
	u8 i;
	u8 crc = 0;

	while(len-- != 0)
	{
		for(i = 0x80; i != 0; i /= 2)
		{
			if((crc & 0x80) != 0)  // 检查最高位
			{
				crc *= 2;      // 左移
				crc ^= key;    // 异或多项式
			}
			else
			{
				crc *= 2;      // 仅左移
			}

			if((*ptr & i) != 0)  // 检查数据位
			{
				crc ^= key;    // 异或多项式
			}
		}
		ptr++;
	}
	return (crc);
}

/* ==================== IIC读写函数 ==================== */

/**
 * @brief IIC单字节写入
 * @param  SloveAddr: 从机地址（7位地址）
 * @param  RegAddr: 寄存器地址
 * @param  Data: 要写入的数据
 * @return gRET_OK - 写入成功
 *         gRET_NG - 写入失败
 * @details 写入流程：
 *          1. 发送起始信号
 *          2. 发送从机地址（写模式）
 *          3. 发送寄存器地址
 *          4. 发送数据
 *          5. 发送CRC校验
 *          6. 发送停止信号
 *
 *          帧格式：
 *          [S][Slave+W][ACK][RegAddr][ACK][Data][ACK][CRC][ACK][P]
 */
unsigned char IIC_WritByte(unsigned char SloveAddr, unsigned char RegAddr, unsigned char Data)
{
	unsigned char ret = gRET_OK;
	unsigned char rcd = 0;
	unsigned char BuffData[3] = {0};
	unsigned char crc = 0;

	IICStart();

	/* 发送从机地址（写模式） */
	rcd = IICSendByte((SloveAddr << 1) & 0xFE);
	BuffData[0] = ((SloveAddr << 1) & 0xFE);
	delay_us(5);
	if(rcd != gRET_OK)
	{
		return gRET_NG;
	}

	/* 发送寄存器地址 */
	rcd = IICSendByte(RegAddr);
	BuffData[1] = RegAddr;
	delay_us(5);
	if(rcd != gRET_OK)
	{
		return gRET_NG;
	}

	/* 发送数据 */
	rcd = IICSendByte(Data);
	BuffData[2] = Data;
	delay_us(5);
	if(rcd != gRET_OK)
	{
		return gRET_NG;
	}

	/* 计算并发送CRC */
	crc = IIC_CRC8Bytes(BuffData, 3, 0x07);
	rcd = IICSendByte(crc);
	delay_us(5);
	if(rcd != gRET_OK)
	{
		return gRET_NG;
	}

	IICStop();
	return ret;
}

/**
 * @brief IIC单字节读取
 * @param  SloveAddr: 从机地址（7位地址）
 * @param  RegAddr: 寄存器地址
 * @param  ReadData: 接收数据的指针
 * @return gRET_OK - 读取成功
 *         gRET_NG - 读取失败
 * @details 读取流程：
 *          1. 发送起始信号
 *          2. 发送从机地址（写模式）
 *          3. 发送寄存器地址
 *          4. 重新发送起始信号
 *          5. 发送从机地址（读模式）
 *          6. 接收数据（2字节：数据+CRC）
 *          7. 验证CRC
 *          8. 发送停止信号
 *
 *          帧格式：
 *          写：[S][Slave+W][ACK][RegAddr][ACK][S][Slave+R][ACK]
 *          读：[Data][ACK][CRC][NACK][P]
 */
unsigned char IIC_ReadByte(unsigned char SloveAddr, unsigned char RegAddr, unsigned char* ReadData)
{
	unsigned char ret = gRET_OK;
	unsigned int rcd = 0;
	unsigned char BuffData[3] = {0};
	unsigned char crc = 0;

	/* 写寄存器地址 */
	IICStart();
	rcd = IICSendByte((SloveAddr << 1) & 0xFE);  // 从机地址（写模式）
	delay_us(5);
	rcd = IICSendByte(RegAddr);                   // 寄存器地址
	delay_us(5);
	if(rcd != gRET_OK)
	{
		return gRET_NG;
	}

	/* 读取数据 */
	IICStart();
	rcd = IICSendByte((SloveAddr << 1) | 0x01);   // 从机地址（读模式）
	BuffData[0] = (SloveAddr << 1) | 0x01;
	delay_us(5);
	if(rcd != gRET_OK)
	{
		return gRET_NG;
	}

	BuffData[1] = IICReceByte();  // 接收数据
	IICSendAck();                  // 发送ACK
	delay_us(5);

	BuffData[2] = IICReceByte();  // 接收CRC
	IICSendNoAck();                // 发送NACK（最后一个字节）
	delay_us(5);

	IICStop();

	/* 验证CRC */
	crc = IIC_CRC8Bytes(BuffData, 2, 0x07);
	if(BuffData[2] != crc)
	{
		return gRET_NG;  // CRC校验失败
	}

	*ReadData = BuffData[1];  // 返回数据
	return ret;
}

/**
 * @brief IIC多字节写入
 * @param  SloveAddr: 从机地址（7位地址）
 * @param  RegAddr: 寄存器起始地址
 * @param  DataSend: 要发送的数据数组
 * @param  Long: 数据长度
 * @return gRET_OK - 写入成功
 *         gRET_NG - 写入失败
 * @details 写入流程：
 *          1. 发送起始信号
 *          2. 发送从机地址（写模式）
 *          3. 发送寄存器地址
 *          4. 发送第1个数据+CRC
 *          5. 依次发送剩余数据（每个数据带独立CRC）
 *          6. 发送停止信号
 *
 *          帧格式：
 *          [S][Slave+W][ACK][RegAddr][ACK][Data0][ACK][CRC0][ACK][Data1][ACK][CRC1][ACK]...[P]
 */
unsigned char IIC_WritByteMore(unsigned char SloveAddr, unsigned char RegAddr, unsigned char *DataSend, unsigned char Long)
{
	unsigned char ret = gRET_OK;
	unsigned char crc = 0, ia = 0;
	unsigned char BuffData[3] = {0};
	unsigned char rcd = 0;

	IICStart();
	rcd = IICSendByte((SloveAddr << 1) & 0xFE);  // 从机地址（写模式）
	BuffData[0] = SloveAddr << 1;
	delay_us(5);
	if(rcd != gRET_OK)
	{
		return gRET_NG;
	}

	rcd = IICSendByte(RegAddr);                   // 寄存器地址
	BuffData[1] = RegAddr;
	delay_us(5);
	if(rcd != gRET_OK)
	{
		return gRET_NG;
	}

	/* 发送第1个数据（带头部CRC） */
	rcd = IICSendByte(DataSend[0]);
	BuffData[2] = DataSend[0];
	delay_us(5);
	if(rcd != gRET_OK)
	{
		return gRET_NG;
	}

	crc = IIC_CRC8Bytes(BuffData, 3, 0x07);
	rcd = IICSendByte(crc);
	delay_us(5);
	if(rcd != gRET_OK)
	{
		return gRET_NG;
	}

	/* 发送剩余数据（每个数据独立CRC） */
	for(ia = 1; ia < Long; ia++)
	{
		rcd = IICSendByte(DataSend[ia]);
		crc = IIC_CRC8Bytes(&DataSend[ia], 1, 0x07);
		rcd = IICSendByte(crc);
		delay_us(5);
		if(rcd != gRET_OK)
		{
			return gRET_NG;
		}
	}
	IICStop();
	return ret;
}

/**
 * @brief IIC多字节读取
 * @param  SloveAddr: 从机地址（7位地址）
 * @param  RegAddr: 寄存器起始地址
 * @param  DataRece: 接收数据数组指针
 * @param  Long: 要读取的数据长度
 * @return gRET_OK - 读取成功
 *         gRET_NG - 读取失败
 * @details 读取流程：
 *          1. 发送起始信号
 *          2. 发送从机地址（写模式）
 *          3. 发送寄存器地址
 *          4. 重新发送起始信号
 *          5. 发送从机地址（读模式）
 *          6. 依次读取多个数据（每个数据带独立CRC）
 *          7. 验证所有CRC
 *          8. 发送停止信号
 *
 *          帧格式：
 *          写：[S][Slave+W][ACK][RegAddr][ACK][S][Slave+R][ACK]
 *          读：[Data0][ACK][CRC0][ACK][Data1][ACK][CRC1][ACK]...[DataN][NACK][CRCN][P]
 */
unsigned char IIC_ReadByteMore(unsigned char SloveAddr, unsigned char RegAddr, unsigned char *DataRece, unsigned char Long)
{
	unsigned char ret = gRET_OK;
	unsigned int rcd = 0;
	unsigned int ia = 0;
	unsigned char crc = 0;
	unsigned char DataRead[111] = {0};

	/* 写寄存器地址 */
	IICStart();
	rcd = IICSendByte((SloveAddr << 1) & 0xFE);  // 从机地址（写模式）
	delay_us(5);
	rcd = IICSendByte(RegAddr);                   // 寄存器地址
	if(rcd != gRET_OK)
	{
		return gRET_NG;
	}

	/* 读取数据 */
	IICStart();
	rcd = IICSendByte((SloveAddr << 1) | 0x01);   // 从机地址（读模式）
	DataRead[0] = ((SloveAddr << 1) | 0x01);
	if(rcd != gRET_OK)
	{
		return gRET_NG;
	}
	delay_us(5);

	/* 接收多个数据（每个数据带独立CRC） */
	for(ia = 0; ia < Long; ia++)
	{
		DataRead[ia * 2 + 1] = IICReceByte();  // 接收数据
		IICSendAck();                          // 发送ACK
		delay_us(5);

		DataRead[ia * 2 + 2] = IICReceByte();  // 接收CRC
		if(ia == (Long - 1))
		{
			IICSendNoAck();  // 最后一个数据，发送NACK
		}
		else
		{
			IICSendAck();    // 非最后数据，发送ACK
		}
		delay_us(5);
	}
	IICStop();

	/* 验证第1个数据的CRC */
	crc = IIC_CRC8Bytes(DataRead, 2, 0x07);
	if(DataRead[2] != crc)
	{
		ret = gRET_NG;
	}

	/* 验证剩余数据的CRC */
	for(ia = 1; ia < Long; ia++)
	{
		crc = IIC_CRC8Bytes(&DataRead[ia * 2 + 1], 1, 0x07);
		if(DataRead[ia * 2 + 2] != crc)
		{
			ret = gRET_NG;
		}
	}

	/* 提取有效数据 */
	if(ret == gRET_OK)
	{
		for(ia = 0; ia < Long; ia++)
		{
			DataRece[ia] = DataRead[ia * 2 + 1];
		}
	}
	return ret;
}
