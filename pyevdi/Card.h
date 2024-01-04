// Copyright (c) 2022 DisplayLink (UK) Ltd.
#ifndef CARD_H
#define CARD_H

#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <functional>
#include <list>
#include <memory>

#include "Buffer.h"

namespace py = pybind11;

class Card {
	evdi_handle evdiHandle;
	evdi_event_context eventContext;
	evdi_mode mode;

	std::list<std::shared_ptr<Buffer> > buffers;
	std::shared_ptr<Buffer> buffer_requested;

	void setMode(struct evdi_mode mode);
	void makeBuffers(int count);
	void clearBuffers();
	void request_update();
	void grab_pixels();

	friend void default_update_ready_handler(int buffer_to_be_updated,
						 void *user_data);
	friend void card_C_mode_handler(struct evdi_mode mode, void *user_data);

    public:
	/// used py::function to allow lambdas to work
	/// void(struct evdi_mode)
	py::function mode_handler;
	/// void(std::shared_ptr<Buffer> buffer)
	py::function acquire_framebuffer_handler;

	explicit Card(int device);
	~Card();
	void close();
	void connect(const char *edid, const unsigned int edid_length,
		     const uint32_t pixel_area_limit,
		     const uint32_t pixel_per_second_limit);
	void disconnect();

	struct evdi_mode getMode() const;

	void handle_events(int waiting_time);
};

#endif
