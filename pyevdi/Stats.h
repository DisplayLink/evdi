// Copyright (c) 2024 DisplayLink (UK) Ltd.
#ifndef STATS_H
#define STATS_H

#include <functional>
#include <chrono>
#include <vector>

#include "Buffer.h"
#include "../library/evdi_lib.h"

class Stats
{
public:
	Stats() = default;
	virtual ~Stats() = default;

	virtual void grab_pixels(evdi_handle, struct evdi_rect *, int *);

};

void timeMeasuringHandler(std::shared_ptr<Buffer> buffer);

class MemoryAccessStats: public Stats
{
private:
	std::vector<std::chrono::microseconds> grabPixelStats;
	std::vector<std::chrono::microseconds> bufferStats;

public:
	uint64_t totalGrabPixelsTime() const;
	uint64_t avgGrabPixelsTime() const;
	uint64_t totalBufferReadTime() const;
	uint64_t avgBufferReadTime() const;
	int countGrabPixels() const;
	int countBuffer() const;

	virtual void grab_pixels(evdi_handle handle,
			struct evdi_rect *rects,
			int *num_rects) override;
	// Object of this class can be used as a framebuffer handler
	// used by the Card class. There is an example in
	// tests/test_connect.py in testReadingBuffer where the object
	// is assigned to Card::acquire_framebuffer_handler, later it
	// will be used in Card::grab_pixels
	void operator()(std::shared_ptr<Buffer> buffer);
};

#endif
