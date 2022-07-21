// Copyright (c) 2022 DisplayLink (UK) Ltd.
#include "../library/evdi_lib.h"
#include <pybind11/pybind11.h>
#include "Card.h"
#include <cstdio>
#include <cstdarg>

namespace py = pybind11;

void log_function (void *user_data, const char * format, ...)
{
    va_list args1, args2;
    va_start(args1, format);

    va_copy(args2, args1);

    int size = 1+std::vsnprintf(nullptr, 0, format, args1);
    va_end(args1);

    char buffer[size];

    std::vsnprintf(buffer, size, format, args2);
    va_end(args2);

    std::string str(buffer);

    py::module logging = py::module::import("logging");
    logging.attr("log")(logging.attr("INFO"), str);
}


PYBIND11_MODULE(PyEvdi, m) {
    m.doc() = "python bindings for evdi library";

    evdi_logging el;
    el.function = &log_function;
    evdi_set_logging(el);
    
    evdi_lib_version lv;
    evdi_get_lib_version(&lv);

    m.attr("version") = py::make_tuple(lv.version_major, lv.version_minor, lv.version_patchlevel); 

    m.def("check_device", &evdi_check_device);

    m.def("add_device", &evdi_add_device);

    py::enum_<evdi_device_status>(m, "evdi_device_status")
        .value("AVAILABLE", AVAILABLE)
        .value("UNRECOGNIZED", UNRECOGNIZED)
        .value("NOT_PRESENT", NOT_PRESENT)
        .export_values(); 

    py::class_<Card>(m, "Card")
        .def(py::init<int>())
        .def("close", &Card::close);
}
