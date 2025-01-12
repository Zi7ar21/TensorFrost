#include <utility>
#include <vector>

#include <Frontend/Python/PyModule.h>

namespace TensorFrost {

class PyModule : public Module {
public:
    using Module::Module; // Inherit constructors

    void assert_parameters() override {
        PYBIND11_OVERRIDE(void, Module, assert_parameters);
    }

    py::object loss(py::object X, py::object Y) override {
        PYBIND11_OVERRIDE_PURE(py::object, Module, loss, X, Y);
    }

    py::object forward(py::object X) override {
        PYBIND11_OVERRIDE_PURE(py::object, Module, forward, X);
    }
};

class ModuleOptimizer : public PyModule {
public:
    enum class OptimizerType {
        ADAM,
        SGD,
        RMSProp
    };

    OptimizerType optimizer_type;
    const float epsilon = 1e-8f;

    ModuleOptimizer(OptimizerType type, Module* net, map<string, py::object> params)
        : PyModule(), optimizer_type(type) {
        setattr("net", py::cast(net));

        for (auto& [name, value] : params) {
            setattr(name, value);
        }

        Parameter t({1}, TFType::Float, false);
        setattr("t", py::cast(t));

        initializeOptimizer(net);
    }

    void initializeOptimizer(Module* net) {
        py::list net_params = net->parameters();
        py::list requires_grads = net->requires_grads_list();
        switch (optimizer_type) {
            case OptimizerType::ADAM:
                initializeParameterArray("m", net_params, requires_grads);
                initializeParameterArray("v", net_params, requires_grads);
                break;
            case OptimizerType::SGD:
                // No additional parameters needed
                break;
            case OptimizerType::RMSProp:
                initializeParameterArray("v", net_params, requires_grads);
                break;
        }
    }

    void initializeParameterArray(const string& name, py::list& net_params, py::list& requires_grads) {
        setattr(name, py::cast(ParameterArray()));
        for (size_t i = 0; i < py::len(net_params); ++i) {
            py::object param = net_params[i];
            if (!py::cast<bool>(requires_grads[i])) {
                continue;
            }
            Parameter new_param = Parameter(py::cast<Parameter&>(param).shape, TFType::Float, false);
            py::cast<ParameterArray&>(getattr(name)).setitem(i, py::cast(new_param));
        }
    }

    void assert_parameters() override {
        py::list net_params = getattr("net").attr("parameters")();
        py::list requires_grads = getattr("net").attr("requires_grads_list")();
        assertParameterArray("m", net_params, requires_grads);
        assertParameterArray("v", net_params, requires_grads);
    }

    void assertParameterArray(const string& name, py::list& net_params, py::list& requires_grads) {
        if (hasattr(name)) {
            for (size_t i = 0; i < py::len(net_params); ++i) {
                if (!py::cast<bool>(requires_grads[i])) {
                    continue;
                }
                py::object param = net_params[i];
                py::object arr = py::cast<ParameterArray&>(getattr(name)).getitem(i);

                py::object shape = param.attr("shape");
                arr = tf.attr("assert_tensor")(arr, shape, param.attr("type"));

                py::cast<ParameterArray&>(getattr(name)).setitem(i, arr);
            }
        }
    }

    py::object step(py::object X, py::object Y) {
        py::object net = getattr("net");
        py::object loss = net.attr("loss")(X, Y);
        step(loss);
        return loss;
    }

    void step(py::object loss) {
        Tensor::BeginRegion("OptimizerStep");
        py::object t = getattr("t");
        t = t + py::float_(1.0);
        setattr("t", t);

        py::object net = getattr("net");
        py::list net_params = net.attr("parameters")();
        py::list requires_grads = net.attr("requires_grads_list")();

        py::object learning_rate = getattr("learning_rate");
        py::object grad_clip = getattr("grad_clip");

        bool has_clip = py::isinstance<py::float_>(grad_clip) && py::cast<float>(grad_clip) > 0.0f;
        Tensor::BeginRegion("UpdateWeights");
        for (size_t i = 0; i < py::len(net_params); ++i) {
            bool requires_grad = py::cast<bool>(requires_grads[i]);
            if (!requires_grad) {
                continue;
            }
            py::object param = net_params[i];
            py::object grad = tf.attr("grad")(loss, param);
            if(has_clip) {
                grad = tf.attr("clamp")(grad, -grad_clip, grad_clip);
            }

            py::object update;
            switch (optimizer_type) {
                case OptimizerType::ADAM:
                    update = adam_update(i, param, grad, t, learning_rate);
                    break;
                case OptimizerType::SGD:
                    update = sgd_update(param, grad, learning_rate);
                    break;
                case OptimizerType::RMSProp:
                    update = rmsprop_update(i, param, grad, learning_rate);
                    break;
            }

            param = param - update;
            net_params[i] = param;
        }
        Tensor::EndRegion("UpdateWeights");
        net.attr("update_parameters")(net_params);
        Tensor::EndRegion("OptimizerStep");
    }

private:
    py::object adam_update(size_t i, py::object& param, py::object& grad, py::object& t, py::object& learning_rate) {
        float beta1 = py::cast<float>(getattr("beta1"));
        float beta2 = py::cast<float>(getattr("beta2"));

        py::object m = py::cast<ParameterArray&>(getattr("m")).getitem(i);
        py::object v = py::cast<ParameterArray&>(getattr("v")).getitem(i);

        m = tf.attr("lerp")(grad, m, beta1);
        v = tf.attr("lerp")(grad.attr("__mul__")(grad), v, beta2);

        py::object mhat = m.attr("__truediv__")(py::float_(1.0) - tf.attr("pow")(beta1, t.attr("__getitem__")(0)));
        py::object vhat = v.attr("__truediv__")(py::float_(1.0) - tf.attr("pow")(beta2, t.attr("__getitem__")(0)));

        py::cast<ParameterArray&>(getattr("m")).setitem(i, m);
        py::cast<ParameterArray&>(getattr("v")).setitem(i, v);

        return mhat.attr("__truediv__")(tf.attr("sqrt")(vhat).attr("__add__")(epsilon)).attr("__mul__")(learning_rate);
    }

    py::object sgd_update(py::object& param, py::object& grad, py::object& learning_rate) {
        return grad.attr("__mul__")(learning_rate);
    }

    py::object rmsprop_update(size_t i, py::object& param, py::object& grad, py::object& learning_rate) {
        float decay = py::cast<float>(getattr("decay"));

        py::object v = py::cast<ParameterArray&>(getattr("v")).getitem(i);
        v = tf.attr("lerp")(grad.attr("__mul__")(grad), v, decay);
        py::cast<ParameterArray&>(getattr("v")).setitem(i, v);

        return grad.attr("__mul__")(learning_rate).attr("__truediv__")(tf.attr("sqrt")(v).attr("__add__")(epsilon));
    }
};

void ModuleDefinitions(py::module& m) {
    py::class_<Parameter>(m, "Parameter")
        .def(py::init<const std::vector<int>&, TFType, float, float, bool>(), py::arg("shape"), py::arg("dtype") = TFType::Float, py::arg("random_scale") = -1.0f, py::arg("random_offset") = 0.0f, py::arg("requires_grad") = true)
        .def_readwrite("shape", &Parameter::shape)
        .def_readwrite("dtype", &Parameter::dtype)
        .def_readwrite("random_scale", &Parameter::random_scale)
        .def_readwrite("random_offset", &Parameter::random_offset)
        .def("__repr__", [](const Parameter& p) {
            return "Parameter(shape=" + std::to_string(p.shape.size()) + ", dtype=" + std::to_string(p.dtype) + ", random_scale=" + std::to_string(p.random_scale) + ", random_offset=" + std::to_string(p.random_offset) + ", requires_grad=" + std::to_string(p.requires_grad) + ")";
        });

    py::class_<ParameterArray>(m, "ParameterArray")
        .def(py::init<>())
        .def("__getitem__", &ParameterArray::getitem)
        .def("__setitem__", &ParameterArray::setitem);

    py::class_<Module, PyModule>(m, "Module")
        .def(py::init<bool>(), py::arg("requires_grad") = true)
        .def("__getattr__", &Module::getattr)
        .def("__setattr__", &Module::setattr)
        .def("hasattr", &Module::hasattr)
        .def("param_requires_grad", &Module::param_requires_grad)
        .def("initialize_input", &Module::initialize_input)
        .def("initialize_parameters", &Module::initialize_parameters)
        .def("parameters", &Module::parameters)
        .def("requires_grads_list", &Module::requires_grads_list)
        .def("create_input", &Module::create_input)
        .def("update_parameters", &Module::update_parameters)
        .def("assert_parameters", &Module::assert_parameters)
        .def("loss", &Module::loss)
        .def("forward", &Module::forward);

    auto optimizer_type = py::enum_<ModuleOptimizer::OptimizerType>(m, "OptimizerType")
        .value("ADAM", ModuleOptimizer::OptimizerType::ADAM)
        .value("SGD", ModuleOptimizer::OptimizerType::SGD)
        .value("RMSProp", ModuleOptimizer::OptimizerType::RMSProp);

    py::class_<ModuleOptimizer, Module>(m, "ModuleOptimizer")
        .def(py::init<ModuleOptimizer::OptimizerType, Module*, map<string, py::object>>(), py::arg("type"), py::arg("net"), py::arg("params"))
        .def("assert_parameters", &ModuleOptimizer::assert_parameters)
        .def("step", py::overload_cast<py::object, py::object>(&ModuleOptimizer::step))
        .def("step", py::overload_cast<py::object>(&ModuleOptimizer::step));

    py::module optimizers = m.def_submodule("optimizers", "Optimizers submodule");

    optimizers.def("adam",
        [](Module* net, py::object learning_rate, py::object beta1, py::object beta2, py::object clip) {
            return new ModuleOptimizer(ModuleOptimizer::OptimizerType::ADAM, net, {
                {"learning_rate", learning_rate},
                {"beta1", beta1},
                {"beta2", beta2},
                {"grad_clip", clip}
            });
        },
        py::arg("net"), py::arg("learning_rate") = 0.001f, py::arg("beta1") = 0.9f, py::arg("beta2") = 0.999f, py::arg("clip") = 0.0f,
        py::return_value_policy::take_ownership
    );

    optimizers.def("sgd",
        [](Module* net, py::object learning_rate, py::object clip) {
            return new ModuleOptimizer(ModuleOptimizer::OptimizerType::SGD, net, {
                {"learning_rate", learning_rate},
                {"grad_clip", clip}
            });
        },
        py::arg("net"), py::arg("learning_rate") = 0.001f, py::arg("clip") = 0.0f,
        py::return_value_policy::take_ownership
    );

    optimizers.def("rmsprop",
        [](Module* net, py::object learning_rate, py::object decay, py::object clip) {
            return new ModuleOptimizer(ModuleOptimizer::OptimizerType::RMSProp, net, {
                {"learning_rate", learning_rate},
                {"decay", decay},
                {"grad_clip", clip}
            });
        },
        py::arg("net"), py::arg("learning_rate") = 0.001f, py::arg("decay") = 0.9f, py::arg("clip") = 0.0f,
        py::return_value_policy::take_ownership
    );
}

}  // namespace TensorFrost