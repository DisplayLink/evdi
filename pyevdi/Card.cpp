// Copyright (c) 2022 DisplayLink (UK) Ltd.
#include "../library/evdi_lib.h"
#include <pybind11/pybind11.h>
#include "Card.h"
#include "Buffer.h"

namespace py = pybind11;


void default_update_ready_handler(int buffer_to_be_updated, void *user_data)
{
    Card* card_ptr = reinterpret_cast<Card*>(user_data);
    assert(buffer_to_be_updated == card_ptr->buffer_requested->buffer.id);
    card_ptr->grab_pixels();
}

void card_C_mode_handler(struct evdi_mode mode, void *user_data)
{
    py::module logging = py::module::import("logging");
    logging.attr("info")("Got mode_changed signal.");
    Card* card = reinterpret_cast<Card*>(user_data);

    assert(card);

    card->setMode(mode);
    card->makeBuffers(2);

    if(card->m_modeHandler != nullptr){
        card->m_modeHandler(mode);
    }

    card->request_update();
}

void Card::setMode(struct evdi_mode mode)
{
    this->mode = mode;
}

void Card::makeBuffers(int count)
{
    clearBuffers();
    for(int i=0; i<count; i++){
        buffers.emplace_back(new Buffer(mode, evdiHandle));
    }
}

void Card::clearBuffers()
{
    buffer_requested.reset();
    buffers.clear();
}

void dpms_handler(int dpms_mode, void* /*user_data*/){
    py::module logging = py::module::import("logging");
    logging.attr("info")("Got dpms signal." + std::to_string(dpms_mode));
}

Card::Card(int device) :
    evdiHandle(evdi_open(device))
{
    if(evdiHandle==nullptr)
    {
        throw py::value_error("Card /dev/dri/card" + std::to_string(device) + "does not exists!");
    }

    memset(&eventContext, 0, sizeof(eventContext));

    m_modeHandler = nullptr;
    acquire_framebuffer_cb = nullptr;

    eventContext.mode_changed_handler = &card_C_mode_handler;
    eventContext.update_ready_handler = &default_update_ready_handler;
    eventContext.dpms_handler=dpms_handler;
    eventContext.user_data = this;

    memset(&mode, 0, sizeof(mode));
}

Card::~Card()
{
    close();
}

void Card::close()
{
    if(evdiHandle != nullptr)
    {
        clearBuffers();
        evdi_close(evdiHandle);
    }
    evdiHandle = nullptr;
}

void Card::connect(const char *edid, const unsigned int edid_length,
    const uint32_t pixel_area_limit, const uint32_t pixel_per_second_limit)
{
    evdi_connect(evdiHandle, reinterpret_cast<const unsigned char *>(edid),
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
    tv.tv_usec = waiting_time*1000;

    request_update();

    if(select(fd + 1, &rfds, NULL, NULL, &tv)){
        evdi_handle_events(evdiHandle, &eventContext);
    }
}

void Card::request_update()
{
    if(buffer_requested){
        return;
    }

    for(auto &i : buffers){
        if(i.use_count() == 1)
        {
            buffer_requested = i;
            break;
        }
    }

    if(!buffer_requested){
        return;
    }

    bool update_ready = evdi_request_update(evdiHandle, buffer_requested->buffer.id);

    if(update_ready){
        grab_pixels();
    }
}

void Card::grab_pixels()
{
    if(!buffer_requested){
        return;
    }

    evdi_grab_pixels(evdiHandle, buffer_requested->buffer.rects, &buffer_requested->buffer.rect_count);

    if(acquire_framebuffer_cb)
        acquire_framebuffer_cb(std::move(buffer_requested));
    buffer_requested = nullptr;

    request_update();
}
