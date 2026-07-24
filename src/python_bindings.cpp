#include <pybind11/pybind11.h>
#include "endstone_worldgen/chunk_buffer.h"
namespace py=pybind11;using namespace endstone_worldgen;
PYBIND11_MODULE(_endstone_worldgen,m){py::class_<ChunkPos>(m,"ChunkPos").def(py::init<>()).def_readwrite("x",&ChunkPos::x).def_readwrite("z",&ChunkPos::z);py::class_<ChunkBuffer>(m,"ChunkBuffer").def(py::init<ChunkPos,int,int,std::uint32_t>(),py::arg("pos"),py::arg("min_y")=-64,py::arg("max_y")=319,py::arg("fill")=0).def("get_runtime_id",&ChunkBuffer::getRuntimeId).def("set_runtime_id",&ChunkBuffer::setRuntimeId).def("fingerprint",&ChunkBuffer::fingerprint);}
