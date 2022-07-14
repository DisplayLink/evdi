// Copyright (c) 2022 DisplayLink (UK) Ltd.
#include "../library/evdi_lib.h"
#include <pybind11/pybind11.h>

namespace py = pybind11;

PYBIND11_MODULE(PyEvdi, m) {
    m.doc() = "python bindings for evdi library";

    evdi_lib_version lv;
    evdi_get_lib_version(&lv);
    
    m.attr("version") = py::make_tuple(lv.version_major, lv.version_minor, lv.version_patchlevel);
}
