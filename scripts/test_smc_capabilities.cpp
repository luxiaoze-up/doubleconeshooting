/**
 * SMC608-BAS 控制器能力检测程序
 * 检查：轴数、IO端口数、参数设置等
 * 
 * 编译: g++ -o test_smc_capabilities test_smc_capabilities.cpp -L../lib -lLTSMC -Wl,-rpath,../lib
 * 运行: ./test_smc_capabilities
 */

#include <iostream>
#include <cstring>
#include <iomanip>

extern "C" {
    short smc_set_connect_timeout(unsigned long timems);
    short smc_board_init(unsigned short ConnectNo, unsigned short type, char* pconnectstring, unsigned long dwBaudRate);
    short smc_board_close(unsigned short ConnectNo);
    short smc_get_card_version(unsigned short ConnectNo, unsigned long* CardVersion);
    short smc_get_total_axes(unsigned short ConnectNo, unsigned long* TotalAxis);
    short smc_get_total_ionum(unsigned short ConnectNo, unsigned short* TotalIn, unsigned short* TotalOut);
    short smc_read_outport(unsigned short ConnectNo, unsigned short portno, unsigned long* value);
    short smc_write_outport(unsigned short ConnectNo, unsigned short portno, unsigned long value);
    short smc_read_inport(unsigned short ConnectNo, unsigned short portno, unsigned long* value);
    short smc_set_equiv(unsigned short ConnectNo, unsigned short axis, double equiv);
    short smc_get_equiv(unsigned short ConnectNo, unsigned short axis, double* equiv);
}

const char* get_error_desc(short ret) {
    switch(ret) {
        case 0: return "成功";
        case 1: return "参数错误";
        case 2: return "端口/轴号无效";
        case 3: return "网络连接失败";
        case 4: return "轴号超出范围";
        case 1008: return "参数设置失败(可能需要先停止轴)";
        default: return "未知错误";
    }
}

void test_controller(const char* ip, unsigned short card_id, const char* name) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  " << name << " (" << ip << ") - Card ID: " << card_id << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    // 连接
    smc_set_connect_timeout(5000);
    char ip_buf[32];
    strncpy(ip_buf, ip, sizeof(ip_buf) - 1);
    ip_buf[sizeof(ip_buf) - 1] = '\0';
    
    short ret = smc_board_init(card_id, 2, ip_buf, 0);
    if (ret != 0) {
        std::cout << "❌ 连接失败: " << get_error_desc(ret) << std::endl;
        return;
    }
    std::cout << "✅ 连接成功" << std::endl;
    
    // 获取版本
    unsigned long version = 0;
    ret = smc_get_card_version(card_id, &version);
    if (ret == 0) {
        std::cout << "\n【控制器版本】0x" << std::hex << version << std::dec << std::endl;
    }
    
    // 获取轴数
    unsigned long total_axes = 0;
    ret = smc_get_total_axes(card_id, &total_axes);
    if (ret == 0) {
        std::cout << "\n【支持轴数】" << total_axes << " 轴 (轴号: 0-" << (total_axes-1) << ")" << std::endl;
    } else {
        std::cout << "\n【支持轴数】获取失败: " << get_error_desc(ret) << std::endl;
    }
    
    // 获取 IO 数量
    unsigned short total_in = 0, total_out = 0;
    ret = smc_get_total_ionum(card_id, &total_in, &total_out);
    if (ret == 0) {
        std::cout << "\n【IO 端口数量】" << std::endl;
        std::cout << "  输入端口: " << total_in << " 个 (IN0-" << (total_in > 0 ? total_in-1 : 0) << ")" << std::endl;
        std::cout << "  输出端口: " << total_out << " 个 (OUT0-" << (total_out > 0 ? total_out-1 : 0) << ")" << std::endl;
    } else {
        std::cout << "\n【IO 端口数量】获取失败: " << get_error_desc(ret) << std::endl;
    }
    
    // 测试输出端口写入
    std::cout << "\n【输出端口写入测试】" << std::endl;
    for (unsigned short port = 0; port < 8; port++) {
        unsigned long current_val = 0;
        // 先读取当前值
        short read_ret = smc_read_outport(card_id, port, &current_val);
        // 尝试写入（写回相同的值，不改变状态）
        short write_ret = smc_write_outport(card_id, port, current_val);
        
        std::cout << "  OUT" << port << ": ";
        if (write_ret == 0) {
            std::cout << "✅ 可用 (当前值: " << current_val << ")" << std::endl;
        } else {
            std::cout << "❌ 不可用 (错误: " << write_ret << " - " << get_error_desc(write_ret) << ")" << std::endl;
        }
    }
    
    // 测试输入端口读取
    std::cout << "\n【输入端口读取测试】" << std::endl;
    for (unsigned short port = 0; port < 8; port++) {
        unsigned long value = 0;
        ret = smc_read_inport(card_id, port, &value);
        std::cout << "  IN" << port << ": ";
        if (ret == 0) {
            std::cout << "✅ 可用 (当前值: " << value << ")" << std::endl;
        } else {
            std::cout << "❌ 不可用 (错误: " << ret << " - " << get_error_desc(ret) << ")" << std::endl;
        }
    }
    
    // 测试轴参数
    std::cout << "\n【轴 equiv 参数测试】" << std::endl;
    for (unsigned short axis = 0; axis < 8; axis++) {
        double equiv = 0;
        short get_ret = smc_get_equiv(card_id, axis, &equiv);
        std::cout << "  AXIS" << axis << ": ";
        if (get_ret == 0) {
            std::cout << "✅ 可用 (当前 equiv: " << std::scientific << equiv << std::fixed << ")" << std::endl;
        } else {
            std::cout << "❌ 不可用 (错误: " << get_ret << " - " << get_error_desc(get_ret) << ")" << std::endl;
        }
    }
    
    // 关闭连接
    smc_board_close(card_id);
    std::cout << "\n连接已关闭" << std::endl;
}

int main() {
    std::cout << std::string(60, '*') << std::endl;
    std::cout << "  SMC608-BAS 控制器能力检测" << std::endl;
    std::cout << std::string(60, '*') << std::endl;
    
    // 测试三个控制器
    test_controller("192.168.1.11", 0, "控制器1 - 辅助支撑");
    test_controller("192.168.1.12", 1, "控制器2 - 反射光成像");
    test_controller("192.168.1.13", 2, "控制器3 - 六自由度/大行程");
    
    std::cout << "\n" << std::string(60, '*') << std::endl;
    std::cout << "  检测完成" << std::endl;
    std::cout << std::string(60, '*') << std::endl;
    
    return 0;
}

