// Copyright (c) 2022 DisplayLink (UK) Ltd.
#include "../library/evdi_lib.h"
#include <pybind11/pybind11.h>
#include "Card.h"

namespace py = pybind11;

void card_C_mode_handler(struct evdi_mode mode, void *user_data)
{
    Card* card = reinterpret_cast<Card*>(user_data);
    if (card)
        card->setMode(mode);

    if(card && card->m_modeHandler != nullptr){
        reinterpret_cast<Card*>(user_data)->m_modeHandler(mode);
    }
}

void Card::setMode(struct evdi_mode mode)
{
    this->mode = mode;
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

    eventContext.mode_changed_handler = &card_C_mode_handler;
    eventContext.user_data = this;

    memset(&mode, 0, sizeof(mode));
}

Card::~Card()
{ 
    if(evdiHandle != nullptr)
    {
        evdi_close(evdiHandle);
    }
}

void Card::close()
{
    if(evdiHandle != nullptr)
    {
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

void Card::handle_events(int waiting_time)
{
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    int fd = evdi_get_event_ready(evdiHandle);
    FD_SET(fd, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = waiting_time*1000;

    if(select(fd + 1, &rfds, NULL, NULL, &tv)){
        evdi_handle_events(evdiHandle, &eventContext);
    }
}

struct evdi_mode Card::getMode() const
{
    return mode;
}
