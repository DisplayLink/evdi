// Copyright (c) 2022 DisplayLink (UK) Ltd.
#ifndef BUFFER_H
#define BUFFER_H

#include "../library/evdi_lib.h"
#include <memory>


class Buffer : public std::enable_shared_from_this<Buffer>
{
    static int numerator; 
    evdi_handle evdiHandle; 
public:
    evdi_buffer buffer;
    Buffer(evdi_mode mode, evdi_handle evdiHandle);
    ~Buffer();
};

#endif
