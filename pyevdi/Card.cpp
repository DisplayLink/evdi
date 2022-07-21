// Copyright (c) 2022 DisplayLink (UK) Ltd.
#include "../library/evdi_lib.h"
#include <pybind11/pybind11.h>
#include "Card.h"

namespace py = pybind11;

Card::Card(int device) :
    evdiHandle(evdi_open(device))
{
    if(evdiHandle==nullptr)
    {
        throw py::value_error("Card /dev/dri/card" + std::to_string(device) + "does not exists!");
    }
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
