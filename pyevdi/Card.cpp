// Copyright (c) 2022 DisplayLink (UK) Ltd.
#include <sstream>

#include "../library/evdi_lib.h"
#include "Buffer.h"
#include "Card.h"

namespace py = pybind11;

void default_update_ready_handler(int buffer_to_be_updated, void *user_data)
{
	Card *card_ptr = reinterpret_cast<Card *>(user_data);
	assert(buffer_to_be_updated == card_ptr->buffer_requested->buffer.id);
	card_ptr->grab_pixels();
}

void card_C_mode_handler(struct evdi_mode mode, void *user_data)
{
	py::module logging = py::module::import("logging");
	logging.attr("info")("Got mode_changed signal.");
	Card *card = reinterpret_cast<Card *>(user_data);

	assert(card);

	card->setMode(mode);
	card->makeBuffers(2);

	if (card->mode_handler) {
		card->mode_handler(mode);
	}

	card->request_update();
}

void card_C_cursor_set_handler(struct evdi_cursor_set cursor_set, void *user_data)
{
	py::module logging = py::module::import("logging");
	logging.attr("debug")("Got cursor set event.");
	Card *card = reinterpret_cast<Card *>(user_data);

	assert(card);

	free(cursor_set.buffer);
}

void card_C_cursor_move_handler(struct evdi_cursor_move, void *user_data)
{
	py::module logging = py::module::import("logging");
	logging.attr("debug")("Got cursor move event.");
	Card *card = reinterpret_cast<Card *>(user_data);

	assert(card);
}

void Card::setMode(struct evdi_mode mode)
{
	this->mode = mode;
}

void Card::makeBuffers(int count)
{
	clearBuffers();
	for (int i = 0; i < count; i++) {
		buffers.emplace_back(std::make_shared<Buffer>(mode, evdiHandle));
	}
}

void Card::clearBuffers()
{
	buffer_requested.reset();
	buffers.clear();
}

void dpms_handler(int dpms_mode, void * /*user_data*/)
{
	py::module logging = py::module::import("logging");
	std::stringstream s;
	s << "Got dpms signal: \"" << dpms_mode << "\"";
	logging.attr("info")(s.str());
}

Card::Card(int device)
	: evdiHandle(evdi_open(device)), m_stat(std::make_shared<Stats>())
{
	if (evdiHandle == nullptr) {
		std::stringstream s;
		s << "Failed to open card \"/dev/dri/card" << device << "\"";
		throw py::value_error(s.str());
	}
	memset(&eventContext, 0, sizeof(eventContext));

	eventContext.mode_changed_handler = &card_C_mode_handler;
	eventContext.update_ready_handler = &default_update_ready_handler;
	eventContext.cursor_set_handler = &card_C_cursor_set_handler;
	eventContext.cursor_move_handler = &card_C_cursor_move_handler;
	eventContext.dpms_handler = dpms_handler;
	eventContext.user_data = this;

	memset(&mode, 0, sizeof(mode));
}

Card::Card(int device, std::shared_ptr<Stats> stat_counter)
	: Card(device)
{
	m_stat = std::move(stat_counter);
}

Card::~Card()
{
	close();
}

void Card::close()
{
	if (evdiHandle != nullptr) {
		clearBuffers();
		evdi_close(evdiHandle);
	}
	evdiHandle = nullptr;
}

void Card::connect(const char *edid, const unsigned int edid_length,
			const uint32_t pixel_area_limit,
			const uint32_t pixel_per_second_limit)
{
	evdi_connect2(evdiHandle, reinterpret_cast<const unsigned char *>(edid),
			edid_length, pixel_area_limit, pixel_per_second_limit);
}

void Card::disconnect()
{
	evdi_disconnect(evdiHandle);
}

struct evdi_mode Card::getMode() const
{
	return mode;
}

void Card::handle_events(int waiting_time)
{
	fd_set rfds;
	struct timeval tv;
	FD_ZERO(&rfds);
	int fd = evdi_get_event_ready(evdiHandle);
	FD_SET(fd, &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = waiting_time * 1000;

	request_update();

	if (select(fd + 1, &rfds, NULL, NULL, &tv)) {
		evdi_handle_events(evdiHandle, &eventContext);
	}
}

void Card::request_update()
{
	if (buffer_requested) {
		return;
	}

	for (auto &i : buffers) {
		if (i.use_count() == 1) {
			buffer_requested = i;
			break;
		}
	}

	if (!buffer_requested) {
		return;
	}

	bool update_ready =
		evdi_request_update(evdiHandle, buffer_requested->buffer.id);

	if (update_ready) {
		grab_pixels();
	}
}

void Card::grab_pixels()
{
	if (!buffer_requested) {
		return;
	}

	m_stat->grab_pixels(evdiHandle, buffer_requested->buffer.rects,
			 &buffer_requested->buffer.rect_count);

	if (acquire_framebuffer_handler)
		acquire_framebuffer_handler(std::move(buffer_requested));
	buffer_requested = nullptr;

	request_update();
}

void Card::enableCursorEvents(bool enable)
{
	evdi_enable_cursor_events(evdiHandle, enable);
}


