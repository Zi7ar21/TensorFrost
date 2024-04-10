#include <utility>
#include <vector>

#include <Frontend/Python/PyTensor.h>

namespace TensorFrost {

void PyTensorDefinition(py::module&, py::class_<PyTensor>&);
void TensorViewDefinition(py::module&, py::class_<TensorView>&);
void TensorFunctionsDefinition(py::module&);
void TensorProgramDefinition(py::module&, py::class_<TensorProgram>&);
void TensorMemoryDefinition(py::module& m,
                            py::class_<TensorMemory>& py_tensor_mem);
void WindowDefinitions(py::module& m);

PYBIND11_MODULE(TensorFrost, m) {
	auto data_type = py::enum_<DataType>(m, "DataType");
	auto backend_type = py::enum_<BackendType>(m, "BackendType");
	auto code_gen_lang = py::enum_<CodeGenLang>(m, "CodeGenLang");
	auto py_tensor = py::class_<PyTensor>(m, "Tensor");
	auto tensor_view = py::class_<TensorView>(m, "TensorView");
	auto tensor_program = py::class_<TensorProgram>(m, "TensorProgram");
	auto py_tensor_mem = py::class_<TensorMemory>(m, "TensorMemory");

	data_type.value("float", DataType::Float);
	data_type.value("int", DataType::Int);
	data_type.value("uint", DataType::Uint);
	data_type.value("bool", DataType::Bool);
	backend_type.value("cpu", BackendType::CPU);
	backend_type.value("vulkan", BackendType::Vulkan);
	backend_type.value("opengl", BackendType::OpenGL);
	backend_type.value("codegen", BackendType::CodeGen);
	code_gen_lang.value("cpp", CodeGenLang::CPP);
	code_gen_lang.value("glsl", CodeGenLang::GLSL);
	code_gen_lang.value("hlsl", CodeGenLang::HLSL);

	m.attr("float32") = DataType::Float;
	m.attr("int32") = DataType::Int;
	m.attr("uint32") = DataType::Uint;
	m.attr("bool1") = DataType::Bool;

	m.attr("cpu") = BackendType::CPU;
	m.attr("vulkan") = BackendType::Vulkan;
	m.attr("opengl") = BackendType::OpenGL;
	m.attr("codegen") = BackendType::CodeGen;

	m.attr("cpp_lang") = CodeGenLang::CPP;
	m.attr("glsl_lang") = CodeGenLang::GLSL;
	m.attr("hlsl_lang") = CodeGenLang::HLSL;

	PyTensorDefinition(m, py_tensor);
	TensorViewDefinition(m, tensor_view);

	// implicit conversion from TensorView to PyTensor
	py::implicitly_convertible<TensorView, PyTensor>();
	py::implicitly_convertible<float, PyTensor>();
	py::implicitly_convertible<int, PyTensor>();
	py::implicitly_convertible<unsigned int, PyTensor>();

	TensorFunctionsDefinition(m);
	TensorProgramDefinition(m, tensor_program);
	TensorMemoryDefinition(m, py_tensor_mem);
	WindowDefinitions(m);

	m.def("initialize",
	      [](BackendType backend_type, const std::string& kernel_compile_options, CodeGenLang kernel_lang) {
		      InitializeBackend(backend_type, kernel_compile_options, kernel_lang);
	      }, py::arg("backend_type") = BackendType::CPU, py::arg("kernel_compile_options") = "", py::arg("kernel_lang") = CodeGenLang::None, "Initialize the backend");

#ifdef NDEBUG
	py::print("TensorFrost module loaded!");
#else
	py::print("TensorFrost module loaded in debug mode! Expect slow performance.");
#endif
}

}  // namespace TensorFrost