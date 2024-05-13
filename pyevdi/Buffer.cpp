// Copyright (c) 2022 DisplayLink (UK) Ltd.
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <string_view>

#include "../library/evdi_lib.h"
#include "Buffer.h"

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
	buffer.rects = reinterpret_cast<evdi_rect *>(
		calloc(buffer.rect_count, sizeof(struct evdi_rect)));
	rects_span = std::span<evdi_rect>(buffer.rects, buffer.rect_count);
	bytes_per_pixel = mode.bits_per_pixel / 8;
	buffer_size = mode.width * mode.height * bytes_per_pixel;
	buffer.buffer = calloc(1, buffer_size);
	buffer_span =
		std::span<uint32_t>(reinterpret_cast<uint32_t *>(buffer.buffer),
					buffer_size / sizeof(uint32_t));

	evdi_register_buffer(evdiHandle, buffer);
}

double Buffer::getHash() const
{
	auto aux_span = std::span<char32_t>(reinterpret_cast<char32_t *>(buffer.buffer),
					buffer_size / sizeof(char32_t));
	auto result = std::hash<std::u32string_view>{}(
		std::u32string_view(aux_span.begin(), aux_span.end()));

	return result;
}

Buffer::~Buffer()
{
	evdi_unregister_buffer(evdiHandle, buffer.id);
	free(buffer.buffer);
	free(buffer.rects);
}
