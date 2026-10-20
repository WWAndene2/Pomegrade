#include "OrasXyFashion.h"

namespace remake
{

std::vector<XyNative> XyFashionNatives()
{
    const std::vector<uint32_t> zero = {0xE3A00000, 0xE12FFF1E}; // mov r0, #0; bx lr
    const std::vector<uint32_t> one = {0xE3A00001, 0xE12FFF1E};  // mov r0, #1; bx lr
    return {{"ChangeFashionBefore", zero}, {"ChangeToDefaultFashionAfter", zero}, {"IsFinishChangingFashion", one}};
}

}
