/**
 * 最小测试程序：验证雷赛 SMC 运动控制器网络连接
 * 
 * 编译 (Linux/WSL):
 *   g++ -o test_smc_connection test_smc_connection.cpp -L../lib -lLTSMC -Wl,-rpath,../lib
 * 
 * 运行:
 *   ./test_smc_connection
 */

#include <iostream>
#include <cstring>
#include <chrono>

// SMC 库函数声明
extern "C" {
    short smc_set_connect_timeout(unsigned long timems);
    short smc_board_init(unsigned short ConnectNo, unsigned short type, char* pconnectstring, unsigned long dwBaudRate);
    short smc_board_close(unsigned short ConnectNo);
    short smc_get_connect_status(unsigned short ConnectNo);
    short smc_get_card_version(unsigned short ConnectNo, unsigned long* CardVersion);
}

// 错误代码解释
const char* get_error_desc(short ret) {
    switch(ret) {
        case 0: return "成功";
        case 1: return "参数错误";
        case 2: return "通信错误";
        case 3: return "网络连接失败/超时";
        case 4: return "设备忙";
        case 5: return "命令错误";
        default: return "未知错误";
    }
}

void test_controller(const char* ip, unsigned short card_id) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "测试控制器: " << ip << " (Card ID: " << card_id << ")" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 设置连接超时 (5秒)
    std::cout << "[1] 设置连接超时: 5000ms... ";
    short ret = smc_set_connect_timeout(5000);
    std::cout << "返回: " << ret << std::endl;
    
    // 尝试连接
    char ip_buf[32];
    strncpy(ip_buf, ip, sizeof(ip_buf) - 1);
    ip_buf[sizeof(ip_buf) - 1] = '\0';
    
    std::cout << "[2] 调用 smc_board_init(card_id=" << card_id 
              << ", type=2, ip=" << ip << ")... " << std::endl;
    
    auto start = std::chrono::steady_clock::now();
    ret = smc_board_init(card_id, 2, ip_buf, 0);
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::cout << "    返回值: " << ret << " (" << get_error_desc(ret) << ")" << std::endl;
    std::cout << "    耗时: " << duration << " ms" << std::endl;
    
    if (ret == 0) {
        // 连接成功，获取版本信息
        std::cout << "[3] 连接成功! 获取控制器版本... ";
        unsigned long version = 0;
        ret = smc_get_card_version(card_id, &version);
        if (ret == 0) {
            std::cout << "版本: 0x" << std::hex << version << std::dec << std::endl;
        } else {
            std::cout << "获取失败 (ret=" << ret << ")" << std::endl;
        }
        
        // 检查连接状态
        std::cout << "[4] 检查连接状态: ";
        ret = smc_get_connect_status(card_id);
        std::cout << (ret == 0 ? "已连接" : "未连接") << std::endl;
        
        // 关闭连接
        std::cout << "[5] 关闭连接... ";
        ret = smc_board_close(card_id);
        std::cout << "返回: " << ret << std::endl;
        
        std::cout << "\n✓ 控制器 " << ip << " 连接测试通过!" << std::endl;
    } else {
        std::cout << "\n✗ 控制器 " << ip << " 连接失败!" << std::endl;
        std::cout << "  可能原因:" << std::endl;
        std::cout << "  - 控制器未上电或网络未连接" << std::endl;
        std::cout << "  - IP 地址配置错误" << std::endl;
        std::cout << "  - 防火墙阻止连接" << std::endl;
        std::cout << "  - WSL 网络与硬件不兼容" << std::endl;
    }
}

int main() {
    std::cout << "================================================" << std::endl;
    std::cout << "  雷赛 SMC 运动控制器连接测试" << std::endl;
    std::cout << "================================================" << std::endl;
    
    // 测试三个控制器
    // 注意：每个控制器需要使用不同的 card_id
    test_controller("192.168.1.11", 0);  // 控制器1
    test_controller("192.168.1.12", 1);  // 控制器2
    test_controller("192.168.1.13", 2);  // 控制器3
    
    std::cout << "\n================================================" << std::endl;
    std::cout << "  测试完成" << std::endl;
    std::cout << "================================================" << std::endl;
    
    return 0;
}

