#!/usr/bin/env python3
"""
SOC调试数据帧解析工具 - 实际版本

实际数据帧格式：
A8 0D FE 10 [A8 0D FE 0E] [SOC] [额定容量] [实际容量] [剩余容量] [电压] [校验]
"""

def parse_actual_frame(hex_str):
    """
    解析实际的SOC调试数据帧

    Args:
        hex_str: 十六进制字符串，如 "A8 0D FE 10 A8 0D FE 0E 00 0A ..."
    """
    # 移除空格
    hex_str = hex_str.replace(' ', '').upper()

    if len(hex_str) < 48:  # 最小长度检查
        print("❌ 数据帧长度不足")
        return

    # 转换为字节
    try:
        data = bytes.fromhex(hex_str)
    except ValueError:
        print("❌ 无效的十六进制数据")
        return

    print("\n" + "="*70)
    print("📊 SOC调试数据解析（实际版本）")
    print("="*70)

    # 外层帧头（Uart2Send添加）
    print(f"\n🔹 外层协议帧（Uart2Send自动添加）:")
    print(f"   帧头:     0x{data[0]:02X}")
    print(f"   设备地址: 0x{data[1]:02X}")
    print(f"   功能码:   0x{data[2]:02X}")
    print(f"   数据长度: 0x{data[3]:02X} ({data[3]} 字节)")

    # 内层数据（从第5字节开始）
    inner_data = data[4:4+data[3]]

    # 解析内层数据
    if len(inner_data) >= 20:
        soc_value = (inner_data[4] << 8) | inner_data[5]
        rated_cap = (inner_data[6] << 24) | (inner_data[7] << 16) | (inner_data[8] << 8) | inner_data[9]
        real_cap = (inner_data[10] << 24) | (inner_data[11] << 16) | (inner_data[12] << 8) | inner_data[13]
        rem_cap = (inner_data[14] << 24) | (inner_data[15] << 16) | (inner_data[16] << 8) | inner_data[17]
        min_volt = (inner_data[18] << 8) | inner_data[19]

        print(f"\n🔹 内层数据:")
        print(f"   帧头:     0x{inner_data[0]:02X} 0x{inner_data[1]:02X} 0x{inner_data[2]:02X} 0x{inner_data[3]:02X}")

        # 1. SOC值
        print(f"\n1️⃣  SOC值:")
        print(f"   原始值: 0x{soc_value:04X} ({soc_value})")
        print(f"   百分比: {soc_value/10:.1f}%")

        # SOC状态判断
        if soc_value == 0:
            print(f"   状态:   ⚠️  SOC为0，可能未初始化或电池完全放空")
        elif soc_value == 1000:
            print(f"   状态:   ✅ SOC为100%，电池满电")
        elif soc_value < 100:
            print(f"   状态:   🔴 低电量 (<10%)")
        elif soc_value < 200:
            print(f"   状态:   🟠 电量低 (10%~20%)")
        else:
            print(f"   状态:   🟢 正常")

        # 2. 额定容量
        rated_mah = rated_cap / 90  # 转换为mAh
        rated_ah = rated_mah / 1000
        print(f"\n2️⃣  额定容量:")
        print(f"   原始值:  0x{rated_cap:08X} ({rated_cap})")
        print(f"   mAh:     {rated_mah:.0f} mAh")
        print(f"   Ah:      {rated_ah:.2f} Ah")

        # 3. 实际容量（温度补偿后）
        real_mah = real_cap / 90
        real_ah = real_mah / 1000
        comp_rate = (real_cap * 1000) // rated_cap if rated_cap > 0 else 0
        print(f"\n3️⃣  实际容量（温度补偿后）:")
        print(f"   原始值:    0x{real_cap:08X} ({real_cap})")
        print(f"   mAh:      {real_mah:.0f} mAh")
        print(f"   Ah:       {real_ah:.2f} Ah")
        print(f"   补偿比例:  {comp_rate/10:.1f}%")

        # 4. 剩余容量
        rem_mah = rem_cap / 90
        rem_ah = rem_mah / 1000
        soc_calc = (rem_cap * 1000) // real_cap if real_cap > 0 else 0
        print(f"\n4️⃣  剩余容量:")
        print(f"   原始值:    0x{rem_cap:08X} ({rem_cap})")
        print(f"   mAh:      {rem_mah:.0f} mAh")
        print(f"   Ah:       {rem_ah:.2f} Ah")
        print(f"   计算SOC:  {soc_calc} ({soc_calc/10:.1f}%)")

        # SOC一致性验证
        print(f"\n🔍 SOC一致性验证:")
        if abs(soc_value - soc_calc) <= 10:  # 允许1%误差
            print(f"   ✅ 验证通过")
            print(f"   报告SOC: {soc_value/10:.1f}%")
            print(f"   计算SOC: {soc_calc/10:.1f}%")
            print(f"   差异:     {abs(soc_value - soc_calc)/10:.1f}%")
        else:
            print(f"   ⚠️  存在差异")
            print(f"   报告SOC: {soc_value/10:.1f}%")
            print(f"   计算SOC: {soc_calc/10:.1f}%")
            print(f"   差异:     {abs(soc_value - soc_calc)/10:.1f}%")

        # 5. 最低电压
        print(f"\n5️⃣  最低单体电压:")
        print(f"   原始值:  0x{min_volt:04X} ({min_volt})")
        print(f"   电压:    {min_volt} mV ({min_volt/1000:.3f} V)")

        # OCV查表验证
        print(f"\n🔍 OCV查表验证:")
        if min_volt >= 4100:
            print(f"   电压范围: 4100mV+")
            print(f"   预期SOC:  约90%~100%")
        elif min_volt >= 3700:
            print(f"   电压范围: 3700~4100mV")
            print(f"   预期SOC:  约50%~90%")
        elif min_volt >= 3300:
            print(f"   电压范围: 3300~3700mV")
            print(f"   预期SOC:  约10%~50%")
        elif min_volt >= 2700:
            print(f"   电压范围: 2700~3300mV")
            print(f"   预期SOC:  约0%~10%")
        else:
            print(f"   电压范围: <2700mV")
            print(f"   预期SOC:  约0% (欠压)")

        # OCV与SOC对比
        if min_volt >= 3700 and soc_value < 500:
            print(f"   ⚠️  电压较高但SOC较低，可能需要校准")
        elif min_volt < 3000 and soc_value > 200:
            print(f"   ⚠️  电压较低但SOC较高，可能需要校准")

        # 6. 温度补偿状态
        print(f"\n🌡️  温度补偿状态:")
        if comp_rate >= 950 and comp_rate <= 1050:
            print(f"   ✅ 正常 (补偿比例: {comp_rate/10:.1f}%)")
        elif comp_rate < 950:
            print(f"   ⚠️  容量降低 (补偿比例: {comp_rate/10:.1f}%)")
            print(f"   原因: 温度过低")
        else:
            print(f"   ⚠️  容量异常高 (补偿比例: {comp_rate/10:.1f}%)")

        # 7. 电池健康度估算
        if real_cap > 0:
            health = (real_cap * 1000) // rated_cap
            print(f"\n🏥 电池健康度估算:")
            print(f"   当前容量比例: {health/10:.1f}%")
            if health >= 900:
                print(f"   状态: ✅ 健康")
            elif health >= 700:
                print(f"   状态: 🟡 轻微老化")
            else:
                print(f"   状态: 🔴 明显老化")

    print("="*70 + "\n")


def main():
    print("="*70)
    print("🔧 SOC调试数据帧解析工具（实际版本）")
    print("="*70)
    print("\n请粘贴从串口助手接收的数据帧（十六进制）")
    print("例如：A8 0D FE 10 A8 0D FE 0E 00 0A 00 36 EE 80 00 37 18 B0 00 00 CF")
    print("\n输入 'q' 退出\n")

    while True:
        try:
            hex_str = input(">>> ").strip()
            if hex_str.lower() == 'q':
                print("👋 退出")
                break
            if hex_str:
                parse_actual_frame(hex_str)
        except KeyboardInterrupt:
            print("\n👋 退出")
            break
        except Exception as e:
            print(f"❌ 解析错误: {e}")


if __name__ == "__main__":
    main()
