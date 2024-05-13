// Copyright (c) 2022 DisplayLink (UK) Ltd.
#ifndef BUFFER_H
#define BUFFER_H

#include <cstddef>
#include <memory>
#include <cstddef>
#include <span>

#include "../library/evdi_lib.h"

class Buffer : public std::enable_shared_from_this<Buffer> {
	static int numerator;
	evdi_handle evdiHandle;

	public:
	evdi_buffer buffer;
	size_t buffer_size;
	std::span<evdi_rect> rects_span;
	std::span<uint32_t> buffer_span;
	size_t bytes_per_pixel;
	Buffer(evdi_mode mode, evdi_handle evdiHandle);
	double getHash() const;
	~Buffer();
};

#endif
