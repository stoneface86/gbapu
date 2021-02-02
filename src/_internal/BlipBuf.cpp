
#include "BlipBuf.hpp"

namespace gbapu::_internal {

BlipBuf::BlipBuf() :
    bbuf{ nullptr }
{
}

BlipBuf::~BlipBuf() {
    blip_delete(bbuf[0]);
    blip_delete(bbuf[1]);
}

}

