// Copyright (c) 2022 DisplayLink (UK) Ltd.
#include "Buffer.h"
#include "../library/evdi_lib.h"
#include <cstdlib>
#include <cstdio>

int Buffer::numerator = 0;

Buffer::Buffer(evdi_mode mode, evdi_handle evdiHandle)
{
    int id = numerator++;

    this->evdiHandle = evdiHandle;
    int stride = mode.width;
    int pitch_mask = 63;

	stride += pitch_mask;
	stride &= ~pitch_mask;
	stride *= 4;

    buffer.id = id;
    buffer.width = mode.width;
    buffer.height = mode.height;
    buffer.stride = stride;
    buffer.rect_count = 16;
    buffer.rects = reinterpret_cast<evdi_rect*>(
            calloc(buffer.rect_count, sizeof(struct evdi_rect)));
    buffer.buffer = calloc(mode.width*mode.width, mode.bits_per_pixel/8);

    evdi_register_buffer(evdiHandle, buffer);
}

Buffer::~Buffer()
{
    evdi_unregister_buffer(evdiHandle, buffer.id);
    free(buffer.buffer);
    free(buffer.rects);
}
