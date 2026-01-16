/**
 * SMC608-BAS IO 端口详细测试
 * 测试不同的 IO 写入方式
 */

#include <iostream>
#include <cstring>
#include <iomanip>

extern "C" {
    short smc_set_connect_timeout(unsigned long timems);
    short smc_board_init(unsigned short ConnectNo, unsigned short type, char* pconnectstring, unsigned long dwBaudRate);
    short smc_board_close(unsigned short ConnectNo);
    
    // 通用 IO
    short smc_read_outport(unsigned short ConnectNo, unsigned short portno, unsigned long* value);
    short smc_write_outport(unsigned short ConnectNo, unsigned short portno, unsigned long value);
    short smc_read_inport(unsigned short ConnectNo, unsigned short portno, unsigned long* value);
    
    // 按位读写 IO (可能的 API)
    short smc_read_outbit(unsigned short ConnectNo, unsigned short bitno, unsigned short* value);
    short smc_write_outbit(unsigned short ConnectNo, unsigned short bitno, unsigned short value);
    short smc_read_inbit(unsigned short ConnectNo, unsigned short bitno, unsigned short* value);
    
    // 伺服使能控制
    short smc_write_sevon_pin(unsigned short ConnectNo, unsigned short axis, unsigned short on_off);
    short smc_read_sevon_pin(unsigned short ConnectNo, unsigned short axis);
    
    // 获取 IO 总数
    short smc_get_total_ionum(unsigned short ConnectNo, unsigned short* TotalIn, unsigned short* TotalOut);
}

void test_io_methods(const char* ip, unsigned short card_id) {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "  测试控制器: " << ip << " (Card ID: " << card_id << ")" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    // 连接
    smc_set_connect_timeout(5000);
    char ip_buf[32];
    strncpy(ip_buf, ip, sizeof(ip_buf) - 1);
    ip_buf[sizeof(ip_buf) - 1] = '\0';
    
    short ret = smc_board_init(card_id, 2, ip_buf, 0);
    if (ret != 0) {
        std::cout << "❌ 连接失败" << std::endl;
        return;
    }
    std::cout << "✅ 连接成功\n" << std::endl;
    
    // 获取 IO 数量
    unsigned short total_in = 0, total_out = 0;
    ret = smc_get_total_ionum(card_id, &total_in, &total_out);
    std::cout << "IO 数量: 输入=" << total_in << ", 输出=" << total_out << std::endl;
    
    // 测试方法1: smc_write_outport - 按端口号写入
    std::cout << "\n【方法1: smc_write_outport(portno, value)】" << std::endl;
    for (unsigned short port = 0; port < 3; port++) {
        unsigned long val = 0;
        ret = smc_read_outport(card_id, port, &val);
        std::cout << "  portno=" << port << ": read=" << ret 
                  << " (val=0x" << std::hex << val << std::dec << ")" << std::endl;
    }
    
    // 测试方法2: smc_write_outbit - 按位写入
    std::cout << "\n【方法2: smc_write_outbit(bitno, value) - 按位操作】" << std::endl;
    for (unsigned short bit = 0; bit < 12; bit++) {
        unsigned short val = 0;
        ret = smc_read_outbit(card_id, bit, &val);
        
        short write_ret = -999;
        // 尝试写入当前值（不改变状态）
        if (ret == 0) {
            write_ret = smc_write_outbit(card_id, bit, val);
        }
        
        std::cout << "  bit" << std::setw(2) << bit << ": read_ret=" << std::setw(3) << ret;
        if (ret == 0) {
            std::cout << " (val=" << val << "), write_ret=" << write_ret;
            if (write_ret == 0) std::cout << " ✅";
            else std::cout << " ❌";
        } else {
            std::cout << " ❌ 读取失败";
        }
        std::cout << std::endl;
    }
    
    // 测试方法3: smc_write_sevon_pin - 伺服使能
    std::cout << "\n【方法3: smc_write_sevon_pin(axis, on_off) - 伺服使能】" << std::endl;
    for (unsigned short axis = 0; axis < 8; axis++) {
        ret = smc_read_sevon_pin(card_id, axis);
        std::cout << "  axis" << axis << ": current=" << ret << std::endl;
    }
    
    // 测试 portno=0 的位模式
    std::cout << "\n【测试: portno=0 作为 8 位掩码】" << std::endl;
    unsigned long port0_val = 0;
    ret = smc_read_outport(card_id, 0, &port0_val);
    std::cout << "  当前 port0 值: 0x" << std::hex << port0_val << std::dec << " (" << port0_val << ")" << std::endl;
    std::cout << "  二进制: ";
    for (int i = 7; i >= 0; i--) {
        std::cout << ((port0_val >> i) & 1);
    }
    std::cout << " (bit7..bit0)" << std::endl;
    
    // 尝试写入 port0 = 0x01 (只开 bit0)
    std::cout << "\n  尝试写入 port0 = 0x01..." << std::endl;
    ret = smc_write_outport(card_id, 0, 0x01);
    std::cout << "  write_outport(0, 0x01) 返回: " << ret << std::endl;
    
    ret = smc_read_outport(card_id, 0, &port0_val);
    std::cout << "  读回 port0 值: 0x" << std::hex << port0_val << std::dec << std::endl;
    
    // 恢复为 0
    smc_write_outport(card_id, 0, 0x00);
    
    smc_board_close(card_id);
    std::cout << "\n连接已关闭" << std::endl;
}

int main() {
    std::cout << std::string(70, '*') << std::endl;
    std::cout << "  SMC608-BAS IO 端口详细测试" << std::endl;
    std::cout << std::string(70, '*') << std::endl;
    
    // 只测试控制器3 (192.168.1.13) - 因为它用于电源控制
    test_io_methods("192.168.1.13", 0);
    
    std::cout << "\n" << std::string(70, '*') << std::endl;
    std::cout << "  测试完成" << std::endl;
    std::cout << std::string(70, '*') << std::endl;
    
    return 0;
}

