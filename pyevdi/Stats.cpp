// Copyright (c) 2024 DisplayLink (UK) Ltd.
#include <chrono>
#include <numeric>

#include "Stats.h"

namespace
{

std::chrono::microseconds totalTime(const std::vector<std::chrono::microseconds>& stats)
{
	return std::accumulate(stats.begin(), stats.end(), std::chrono::microseconds(0));
}

std::chrono::microseconds averageTime(const std::vector<std::chrono::microseconds>& stats)
{
	return totalTime(stats) / stats.size();
}

} //namespace

void Stats::grab_pixels(evdi_handle handle,
			struct evdi_rect *rects, int *num_rects)
{
	evdi_grab_pixels(handle, rects, num_rects);
}

uint64_t MemoryAccessStats::totalGrabPixelsTime() const
{
	return totalTime(grabPixelStats).count();
}

uint64_t MemoryAccessStats::avgGrabPixelsTime() const
{
	return averageTime(grabPixelStats).count();
}

uint64_t MemoryAccessStats::totalBufferReadTime() const
{
	return totalTime(bufferStats).count();
}

uint64_t MemoryAccessStats::avgBufferReadTime() const
{
	return averageTime(bufferStats).count();
}

int MemoryAccessStats::countGrabPixels() const
{
	return grabPixelStats.size();
}

int MemoryAccessStats::countBuffer() const
{
	return grabPixelStats.size();
}

void MemoryAccessStats::grab_pixels(evdi_handle handle,
			struct evdi_rect *rects, int *num_rects)
{
	auto start = std::chrono::high_resolution_clock::now();
	
	Stats::grab_pixels(handle, rects, num_rects);

	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	grabPixelStats.push_back(duration);
}

void MemoryAccessStats::operator()(std::shared_ptr<Buffer> buffer) {
	auto start = std::chrono::high_resolution_clock::now();

	buffer->getHash();

	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	
	bufferStats.push_back(duration);
}

