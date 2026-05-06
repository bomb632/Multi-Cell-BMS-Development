#!/usr/bin/env python3
"""
SOC调试数据帧解析工具

用法：
1. 从串口助手复制数据帧（十六进制）
2. 运行此脚本：python3 soc-data-parser.py
3. 粘贴数据帧
4. 查看解析结果

数据帧格式：A8 0D FE 0E [14字节数据] AA [校验]
"""

def parse_soc_frame(hex_str):
    """
    解析SOC调试数据帧

    Args:
        hex_str: 十六进制字符串，如 "A8 0D FE 0E 03 E8 ..."
    """
    # 移除空格并转换
    hex_str = hex_str.replace(' ', '').replace('0x', '').upper()

    if len(hex_str) < 44:  # 最小长度检查
        print("❌ 数据帧长度不足，应为22字节（44个十六进制字符）")
        return

    # 转换为字节
    try:
        data = bytes.fromhex(hex_str)
    except ValueError:
        print("❌ 无效的十六进制数据")
        return

    # 验证帧头
    if data[0] != 0xA8 or data[1] != 0x0D or data[2] != 0xFE or data[3] != 0x0E:
        print("⚠️  警告：帧头不匹配")
        print(f"   预期：A8 0D FE 0E")
        print(f"   实际：{data[0]:02X} {data[1]:02X} {data[2]:02X} {data[3]:02X}")

    # 验证帧尾
    if data[20] != 0xAA:
        print("⚠️  警告：帧尾不匹配")
        print(f"   预期：AA")
        print(f"   实际：{data[20]:02X}")

    print("\n" + "="*60)
    print("📊 SOC调试数据解析")
    print("="*60)

    # 解析数据
    soc_value = (data[4] << 8) | data[5]
    rated_cap = (data[6] << 24) | (data[7] << 16) | (data[8] << 8) | data[9]
    real_cap = (data[10] << 24) | (data[11] << 16) | (data[12] << 8) | data[13]
    rem_cap = (data[14] << 24) | (data[15] << 16) | (data[16] << 8) | data[17]
    min_volt = (data[18] << 8) | data[19]
    checksum = data[21]

    # 1. SOC值
    print(f"\n1️⃣  SOC值: {soc_value} ({soc_value/10:.1f}%)")

    # 2. 额定容量
    rated_mah = rated_cap / 90  # 转换为mAh
    rated_ah = rated_mah / 1000  # 转换为Ah
    print(f"\n2️⃣  额定容量:")
    print(f"   - 原始值: {rated_cap} (10mA/200ms)")
    print(f"   - mAh:    {rated_mah:.0f} mAh")
    print(f"   - Ah:     {rated_ah:.2f} Ah")

    # 3. 实际容量
    real_mah = real_cap / 90
    real_ah = real_mah / 1000
    comp_rate = (real_cap * 1000) // rated_cap if rated_cap > 0 else 0
    print(f"\n3️⃣  实际容量（温度补偿后）:")
    print(f"   - 原始值:       {real_cap} (10mA/200ms)")
    print(f"   - mAh:         {real_mah:.0f} mAh")
    print(f"   - Ah:          {real_ah:.2f} Ah")
    print(f"   - 补偿比例:    {comp_rate/10:.1f}%")

    # 4. 剩余容量
    rem_mah = rem_cap / 90
    rem_ah = rem_mah / 1000
    soc_calc = (rem_cap * 1000) // real_cap if real_cap > 0 else 0
    print(f"\n4️⃣  剩余容量:")
    print(f"   - 原始值:       {rem_cap} (10mA/200ms)")
    print(f"   - mAh:         {rem_mah:.0f} mAh")
    print(f"   - Ah:          {rem_ah:.2f} Ah")
    print(f"   - 计算SOC:     {soc_calc} ({soc_calc/10:.1f}%)")

    # 5. 最低电压
    print(f"\n5️⃣  最低单体电压: {min_volt} mV ({min_volt/1000:.3f} V)")

    # 6. 校验和
    calc_checksum = sum(data[4:20]) & 0xFF
    print(f"\n6️⃣  校验和:")
    print(f"   - 接收值:   0x{checksum:02X}")
    print(f"   - 计算值:   0x{calc_checksum:02X}")
    if checksum == calc_checksum:
        print(f"   - 状态:    ✅ 校验通过")
    else:
        print(f"   - 状态:    ❌ 校验失败")

    # SOC验证
    print("\n" + "="*60)
    print("🔍 SOC算法验证")
    print("="*60)

    if abs(soc_value - soc_calc) <= 5:  # 允许0.5%误差
        print(f"✅ SOC一致性验证通过")
        print(f"   - 报告SOC: {soc_value/10:.1f}%")
        print(f"   - 计算SOC: {soc_calc/10:.1f}%")
        print(f"   - 差异:     {abs(soc_value - soc_calc)/10:.1f}%")
    else:
        print(f"⚠️  SOC一致性验证失败")
        print(f"   - 报告SOC: {soc_value/10:.1f}%")
        print(f"   - 计算SOC: {soc_calc/10:.1f}%")
        print(f"   - 差异:     {abs(soc_value - soc_calc)/10:.1f}%")

    # OCV查表验证
    print(f"\n📋 OCV查表验证:")
    print(f"   - 最低电压: {min_volt} mV")
    if min_volt >= 4100:
        print(f"   - 预期SOC: 约90%~100%")
    elif min_volt >= 3700:
        print(f"   - 预期SOC: 约50%~90%")
    elif min_volt >= 3300:
        print(f"   - 预期SOC: 约10%~50%")
    else:
        print(f"   - 预期SOC: 约0%~10%")

    print(f"\n💡 建议:")
    if real_cap < rated_cap * 75 / 100:
        print(f"   - ⚠️  温度过低，实际容量降至75%以下")
    elif real_cap > rated_cap * 105 / 100:
        print(f"   - ⚠️  实际容量超过105%，请检查温度补偿算法")
    else:
        print(f"   - ✅ 温度补偿正常")

    print("="*60 + "\n")


def main():
    print("="*60)
    print("🔧 SOC调试数据帧解析工具")
    print("="*60)
    print("\n请粘贴从串口助手接收的数据帧（十六进制）")
    print("例如：A8 0D FE 0E 03 E8 XX XX XX XX XX XX XX XX XX XX XX XX 0A BC AA XX")
    print("\n输入 'q' 退出\n")

    while True:
        try:
            hex_str = input(">>> ").strip()
            if hex_str.lower() == 'q':
                print("👋 退出")
                break
            if hex_str:
                parse_soc_frame(hex_str)
        except KeyboardInterrupt:
            print("\n👋 退出")
            break
        except Exception as e:
            print(f"❌ 解析错误: {e}")


if __name__ == "__main__":
    main()
