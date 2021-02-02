
#pragma once

#include "blip_buf.h"

namespace gbapu::_internal {

struct BlipBuf {

    blip_buffer_t *bbuf[2];

    BlipBuf();

    ~BlipBuf();

};


}

