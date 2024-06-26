// Copyright (c) 2022 DisplayLink (UK) Ltd.
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <span>

#include "../library/evdi_lib.h"
#include "Card.h"

namespace py = pybind11;

void log_function(void * /*user_data*/, const char *format, ...)
{
	va_list args1, args2;
	va_start(args1, format);

	va_copy(args2, args1);

	int size = 1 + std::vsnprintf(nullptr, 0, format, args1);
	va_end(args1);

	char buffer[size];

	std::vsnprintf(buffer, size, format, args2);
	va_end(args2);

	std::string str(buffer);

	py::module logging = py::module::import("logging");
	logging.attr("log")(logging.attr("INFO"), str);
}

PYBIND11_MODULE(PyEvdi, m)
{
	m.doc() = "Python bindings for evdi library";

	evdi_logging el = { .function = &log_function, .user_data = nullptr};
	el.function = &log_function;
	evdi_set_logging(el);

	evdi_lib_version lv;
	evdi_get_lib_version(&lv);

	m.attr("version") = py::make_tuple(lv.version_major, lv.version_minor,
					   lv.version_patchlevel);

	m.def("check_device", &evdi_check_device);

	m.def("add_device", &evdi_add_device);

	py::enum_<evdi_device_status>(m, "DeviceStatus")
		.value("AVAILABLE", AVAILABLE)
		.value("UNRECOGNIZED", UNRECOGNIZED)
		.value("NOT_PRESENT", NOT_PRESENT)
		.export_values();

	py::class_<evdi_mode>(m, "DisplayMode")
		.def(py::init<>())
		.def_readwrite("width", &evdi_mode::width)
		.def_readwrite("height", &evdi_mode::height)
		.def_readwrite("refresh_rate", &evdi_mode::refresh_rate)
		.def_readwrite("bits_per_pixel", &evdi_mode::bits_per_pixel)
		.def_readwrite("pixel_format", &evdi_mode::pixel_format);

	py::class_<evdi_rect>(m, "evdi_rect")
		.def(py::init<>())
		.def_readwrite("x1", &evdi_rect::x1)
		.def_readwrite("x2", &evdi_rect::x2)
		.def_readwrite("y1", &evdi_rect::y1)
		.def_readwrite("y2", &evdi_rect::y2);

	py::class_<Buffer, std::shared_ptr<Buffer> >(m, "Buffer")
		.def_property_readonly(
			"id", [](Buffer &self) { return self.buffer.id; })
		.def_property_readonly("bytes",
				       [](Buffer &self) {
					       return self.buffer.buffer;
				       })
		.def_property_readonly(
			"width", [](Buffer &self) { return self.buffer.width; })
		.def_property_readonly("height",
				       [](Buffer &self) {
					       return self.buffer.height;
				       })
		.def_property_readonly("stride",
				       [](Buffer &self) {
					       return self.buffer.stride;
				       })
		.def_property_readonly(
			"rects",
			[](Buffer &self) {
				std::vector<evdi_rect> rects;
				for (int i = 0; i < self.buffer.rect_count;
				     i++) {
					rects.push_back(self.buffer.rects[i]);
				}
				return rects;
			})
		.def_property_readonly("rect_count", [](Buffer &self) {
			return self.buffer.rect_count;
		});

	py::class_<Card>(m, "Card")
		.def(py::init<int>())
		.def(py::init<int, std::shared_ptr<Stats>>())
		.def("getMode", &Card::getMode)
		.def("close", &Card::close)
		.def("connect", &Card::connect)
		.def("disconnect", &Card::disconnect)
		.def("handle_events", &Card::handle_events)
		.def("enableCursorEvents", &Card::enableCursorEvents)
		.def_readwrite("acquire_framebuffer_handler",
			       &Card::acquire_framebuffer_handler)
		.def_readwrite("mode_changed_handler", &Card::mode_handler);

	py::class_<Stats, std::shared_ptr<Stats>>(m, "Stats")
		.def(py::init<>());

	py::class_<MemoryAccessStats, Stats, std::shared_ptr<MemoryAccessStats>>(m, "MemoryAccessStats")
		.def(py::init<>())
		.def("__call__", &MemoryAccessStats::operator())
		.def("totalGrabPixelsTime", &MemoryAccessStats::totalGrabPixelsTime)
		.def("avgGrabPixelsTime", &MemoryAccessStats::avgGrabPixelsTime)
		.def("totalBufferReadTime", &MemoryAccessStats::totalBufferReadTime)
		.def("avgBufferReadTime", &MemoryAccessStats::avgBufferReadTime)
		.def("countGrabPixels", &MemoryAccessStats::MemoryAccessStats::countGrabPixels)
		.def("countBuffer", &MemoryAccessStats::countBuffer);
}
