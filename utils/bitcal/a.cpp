#include <iostream>
#include "bit_tools.h"

int main() {
    try {
        // 创建一个2^10 = 1024位的bitmap
        PowerBitset bitmap(10);
        
        // 设置一些位
        bitmap.set(100);
        bitmap.set(101);
        bitmap.set(102);

         bitmap.set(1);
        bitmap.set(0);
        bitmap.set(2);
        
        // 测试操作
        std::cout << "Bitmap size: " << bitmap.size() << std::endl;
        std::cout << "Bit 101 is set: " << bitmap.test(101) << std::endl;
        std::cout << "Has 3 consecutive ones: " 
                  << bitmap.hasConsecutiveCount(3) << std::endl;

        std::cout << "Consecutive ones positions: ";
        for (auto pos : bitmap.hasConsecutiveOnesPos(3)) {
            std::cout << pos << " ";
        }

    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

    return 0;
}
