# 🔢🥶 TensorFrost (v0.6.0 beta)
[![PyPI Build and Deploy](https://github.com/MichaelMoroz/TensorFrost/actions/workflows/build-and-deploy-to-pypi.yml/badge.svg)](https://github.com/MichaelMoroz/TensorFrost/actions/workflows/build-and-deploy-to-pypi.yml)


A statically compiled Python tensor library with autodifferentiation and bottom-up kernel fusion with a low-level IR.

Currently working platforms:
| Backend/OS | CPU | OpenGL | CUDA | Vulkan |
|------------|-----|--------|------|--------|
| Windows    | 🚧  |  🚧   |  ⛔  |  ⛔   |
| Linux      | 🚧  |  🚧   |  ⛔  |  ⛔   |

Under the hood, TensorFrost objects are basically operations that have shape (which are not tensors yet!), some operations can have children operations like loop/if, the compilation process first tries to segment the IR into parts that can be broadcast into the same shape, these parts create proto-kernels, some proto-kernels can be children to loops and if's as well if the stuff under the loop cant be fused, like in the case of iterative algorithms (qr/fft/sorting/jacobi).
These proto-kernels are optimized then to minimize the amount of links between them, if a computation is cheaper to do again rather than store/load from memory - then it does that.
After minimizing links between protokernels, it creates actual tensors for inputs and outputs of these kernels, and replaces the links with load/store operations, and you get final list of kernel operations and memory allocations which is translated into C++ code and compiled into a shared library like  [here](https://github.com/MichaelMoroz/TensorFrost/blob/main/examples/Algorithms/qr.ipynb):

```c++
std::tuple<TFTensor, TFTensor> QRDecomposition(TFContext tf, TFTensor A)
{
  int m = A.shape[0];
  int n = A.shape[1];
  tf.check_tensor(A, "A", {(uint)m, (uint)n}, TFType::Float);
  TFTensor Q = tf.allocate("Q", {(uint)m, (uint)n}, TFType::Float);
  tf.dispatch(0, {Q},  {}, {asuint(n), asuint(m)}, {(uint)m, (uint)n}, {16, 16});
  TFTensor R = tf.allocate("R", {(uint)n, (uint)n}, TFType::Float);
  tf.dispatch(1, {R},  {}, {asuint(n)}, {(uint)n, (uint)n}, {16, 16});
  for (int i = 0; i < n - 1; i += 1)
  {
    int v8_0 = 1;
    int v8_1 = 1;
    tf.dispatch(2, {R},  {A}, {asuint(m), asuint(n), asuint(i)}, {(uint)1}, {1});
    tf.dispatch(3, {Q},  {R, A}, {asuint(m), asuint(n), asuint(i)}, {(uint)m}, {256});
    int v16_2 = n - (i + 1);
    int v16_5 = n - (i + 1);
    tf.dispatch(4, {R},  {Q, A}, {asuint(i), asuint(n), asuint(m)}, {(uint)v16_2}, {256});
    tf.dispatch(5, {A},  {Q, R}, {asuint(i), asuint(n), asuint(m)}, {(uint)m, (uint)v16_2}, {16, 16});
  }
  int v24_0 = 1;
  int v24_1 = 1;
  tf.dispatch(6, {R},  {A}, {asuint(m), asuint(n)}, {(uint)1}, {1});
  tf.dispatch(7, {Q},  {R, A}, {asuint(m), asuint(n)}, {(uint)m}, {256});
  return {Q, R};
}
```

One important distinction of TensorFrost compared to JAX is that it can compile programs that are shape agnostic (JAX cant have argument value dependent shapes!) , so it can be reused for any shaped input (however with same dimensionality though). 
This is important if you want have the computation to be portable, and be possible to easily include it in a native application, or if you want to have a program that can be used for different shapes of data (like in the case of neural networks).

In some sense, you could say that TensorFrost goes with a bottom up approach of kernel fusion, instead of trying to fuse already existing kernels (however that is also planned in the future for premade hand optimized kernels).


## Examples

<a href="https://github.com/MichaelMoroz/TensorFrost/blob/main/examples/Simulation/wave_simulation.ipynb"><img src="https://github.com/MichaelMoroz/TensorFrost/blob/main/examples/sin_gordon.gif?raw=true" height="192px"></a>
<a href="https://github.com/MichaelMoroz/TensorFrost/blob/main/examples/Simulation/fluid_simulation.ipynb"><img src="https://github.com/MichaelMoroz/TensorFrost/blob/main/examples/fluid.gif?raw=true" height="192px"></a>

## Installation

## From PyPI

Current version is [0.6.0](https://pypi.org/project/tensorfrost/)

```bash
pip install tensorfrost
```

## From source

You need to have CMake installed to build the library. 

First clone the repository:
```bash
git clone --recurse-submodules https://github.com/MichaelMoroz/TensorFrost.git
cd TensorFrost
```

Then run cmake to build the library:
```bash
cmake -S . -B build && cmake --build build
```

The cmake script will automatically install the compiled python module into your python environment.

### Building wheel packages (optional)

You can either call `clean_rebuild.bat %PYTHON_VERSION%` to build the wheel packages for the specified python version (the version needs to be installed beforehand), or you can build them for all versions by calling `build_all_python_versions.bat`. The scripts will automatically build and install the library for each python version, and then build the wheel packages to the `PythonBuild/dist` folder.

## Usage

### Setup
For the library to work you need a C++ compiler that supports C++17 (Currently only Microsoft Visual Studio Compiler on Windows, and gcc on Linux)

First you need to import the library:
```python
import TensorFrost as tf
```

Then you need to initialize the library with the device you want to use and the kernel compiler flags (different for each platform):
```python
tf.initialize(tf.cpu) # or tf.opengl
```

TensorFrost will find any available MSVC(Windows) or GCC(Linux) compiler and use it to compile the main code and the kernels. In OpenGL mode the driver compiles the kernels. (TODO: compile the main code into python for faster compile times, MSVC is super slow, 1.5 seconds for a single function)

You can have TensorFrost in code generation mode instead (you cant run tensor programs here), it is much faster, but you would need to use the code manually afterwards:

```python
tf.initialize(tf.codegen, kernel_lang = tf.hlsl_lang) # or tf.glsl_lang for OpenGL, or tf.cpp_lang for C++
```

After you compiled all the tensor programs you need, you can get all the generated code and save it to a file:
```python
# Save all the compiled functions
cpp_header = tf.get_cpp_header()
all_main_functions = tf.get_all_generated_main_functions() #always in C++
with open('tensorfrost_main.cpp', 'w') as f:
    f.write(cpp_header)
    for func in all_main_functions:
        f.write(func)

# Save all the compiled kernels
all_kernels = tf.get_all_generated_kernels() #depends on the kernel_lang
for i, kernel in enumerate(all_kernels):
    with open('generated_kernels/kernel_{}.hlsl'.format(i), 'w') as f:
        f.write(kernel)
```

Right now you cant just compile the code and run it, since it also requires a Kernel compiler and executor as well as memory manager for tensors. In the future I plan to add all the required functions for that too, for better portability.

### Basic usage

Now you can create and compile functions, for example here is a very simple function does a wave simulation:
```python
def WaveEq():
    #shape is not specified -> shape is inferred from the input tensor (can result in slower execution)
    u = tf.input([-1, -1], tf.float32)
    #shape must match 
    v = tf.input(u.shape, tf.float32)

    i,j = u.indices
    laplacian = u[i-1, j] + u[i+1, j] + u[i, j-1] + u[i, j+1] - u * 4.0
    v_new = v + dt*laplacian
    u_new = u + dt*v_new

    return [v_new, u_new]

wave_eq = tf.compile(WaveEq)
```

As you can see, inputs are not arguments to the function, but are created inside the function. This is because some inputs can be constrained by the shape of other inputs, and the shape of the input tensor is not known at compile time. You can give shape arguments to the input function, constants for exactly matching shapes, or -1 for any shape. If you want to constrain the shape of the input tensor, you need to get the shape of the other tensor and use it as an argument to the input function.

The tensor programs take and output tensor memory buffers, which can be created from numpy arrays:
```python
A = tf.tensor(np.zeros([100, 100], dtype=np.float32))
B = tf.tensor(np.zeros([100, 100], dtype=np.float32))
```

Then you can run the program:
```python
A, B = wave_eq(A, B)
```
As you can see the inputs are given to the compiled function in the same order as they are created in the function.

To get the result back into a numpy array, you can use the `numpy` property:
```python
Anp = A.numpy
```

TensorFrost does not support JIT compilation (currently no plans either), so you must create the program before running it. Therefore the tensor operations must only be used inside a tensor program. Operations outside the function will throw an error, so if you want to do operations outside you must read the data into a numpy array first.

### Operations

TensorFrost supports most of the basic numpy operations, including indexing, arithmetic, and broadcasting (only partially for now).
The core operation is the indexing operation, which is used to specify indices for accessing the tensor data. Depending on the dimensinality of the tensor there can be N indices. This operation is similar to numpy's `np.ogrid` and `np.mgrid` functions, but it is basically free due to fusion.

```python
#can be created either from a provided shape or from a tensor
i,j = tf.indices([8, 8]) 
i,j = A.indices
```

For example `i` contains:

```
[[0, 0, 0, ..., 0, 0, 0],
 [1, 1, 1, ..., 1, 1, 1],
 [2, 2, 2, ..., 2, 2, 2],
    ...,
 [7, 7, 7, ..., 7, 7, 7]]
```

And analogously for `j`.

These indices can then be used to index into the tensor data, to either read or write data:
```python
#set elements [16:32, 16:32] to 1.0
i,j = tf.indices([16, 16]) 
B[i+16, j+16] = 1.0

#read elements [8:24, 8:24]
i,j = tf.indices([16, 16])
C = B[i+8, j+8]
```

Here we can see that the shape of the "computation" is not the same as the shape of the tensor, and one thread is spawned for each given index. This is the main idea of TensorFrost. Then all sequential computations of the same shape are fused into a single kernel, if they are not dependent on each other in a non-trivial way.

When doing out-of-bounds indexing, the index is currently clamped to the tensor shape. This is not ideal, but it is the simplest way to handle this. In the future there will be a way to specify the boundary conditions.

You can also use the index_grid operation which is similar to numpy's `np.meshgrid` function and provides a grid of indices for each dimension:

```python
p, k = tf.index_grid([0, i + 1], [m, n])
```

Which is equivalent to numpy's `np.meshgrid` function (only for ints with step 1 for now):

```python
p, k = np.meshgrid(np.arange(0, m), np.arange(i + 1, n))
```

### Scatter operations

These operations allow implementing non-trivial reduction operations, including, for example, matrix multiplication:

```python
def MatrixMultiplication():
    A = tf.input([-1, -1], tf.float32)
    N, M = A.shape
    B = tf.input([M, -1], tf.float32) #M must match
    K = B.shape[1]

    C = tf.zeros([N, K])
    i, j, k = tf.indices([N, K, M])
    tf.scatterAdd(C[i, j], A[i, k] * B[k, j])

    return [C]

matmul = tf.compile(MatrixMultiplication)
```

Here the 3D nature of the matrix multiplication is apparent. The scatter operation is used to accumulate the results of the row-column dot products into the elements of the resulting matrix.

(This is not the most efficient way to implement matrix multiplication, but it is the simplest way to show how scatter operations work. In the future though, some dimensions will be converted into loop indices, and the scatter operation will be used to accumulate the results of the dot products into the resulting matrix.)

### Reduction operations

Reduction operations are used to reduce the tensor data along one (TODO more) dimension(s). For example, here is a simple example of a sum reduction:

```python
def MatrixMultiplication():
    A = tf.input([-1, -1], tf.float32)
    N, M = A.shape
    B = tf.input([M, -1], tf.float32) #M must match
    K = B.shape[1]

    i, j, k = tf.indices([N, K, M])
    C = tf.sum(A[i, k] * B[k, j], axis=2) #by default axis is -1 (last axis)

    return [C]

matmul = tf.compile(MatrixMultiplication)
```

Here the `sum` operation is used to sum the dot products of the rows and columns of the input matrices along the `k` axis.
This is much more efficient than the scatter operation, and in fact this compiles to a single N*K kernel.

### Broadcasting

Broadcasting is used to make the shapes of the input tensors compatible. For example, here is a simple example of a broadcasting operation:

```python
def Broadcasting():
    A = tf.input([1, 3], tf.float32)
    B = tf.input([3, 1], tf.float32)

    C = A + B

    return [C]
```

Here the `+` operation is used to add the two input tensors. The shapes of the input tensors are `[1, 3]` and `[3, 1]`, and the shape of the output tensor is `[3, 3]`. The `+` operation is broadcasted over the input tensors, and the result is a tensor with the shape `[3, 3]`.
The rules are the same as in numpy essentially.

### Reshape

Reshape operation is used to change the shape of the tensor. For example, here is a simple example of a reshape operation:

```python
def Reshape():
    A = tf.input([2, 3], tf.float32)

    B = tf.reshape(A, [3, 2])

    return [B]
```

Here the `reshape` operation is used to change the shape of the input tensor from `[2, 3]` to `[3, 2]`.
At the moment this is implemented in a very crude way, so doing this will always halt kernel fusion, so use it only when you are sure things are unfusable
(usually at the beginning or end of the program).

Alternatively you can also use `transpose` and `unsqueeze` operations to change the shape of the tensor, which are work fine with fusion.

```python
def Transpose():
    A = tf.input([2, 3], tf.float32)

    B = tf.transpose(A) #shape is [3, 2]
    C = B.T #shape is [2, 3]

    return [C]
```

```python
def Unsqueeze():
    A = tf.input([2, 3], tf.float32)

    B = tf.unsqueeze(A, 1) #shape is [2, 1, 3]

    return [B]
```

### Matrix operations

Matrix operations are used to perform matrix operations on the tensor data. For example, here is a simple example of a matrix multiplication:

```python
def MatrixMultiplication():
    A = tf.input([-1, -1], tf.float32)
    N, M = A.shape
    B = tf.input([M, -1], tf.float32) #M must match
    K = B.shape[1]

    C = tf.matmul(A, B) #or A @ B

    return [C]

matmul = tf.compile(MatrixMultiplication)

A = tf.tensor(np.zeros([100, 100], dtype=np.float32))
B = tf.tensor(np.zeros([100, 100], dtype=np.float32))

C, = matmul(A, B)
```

Here the `matmul` operation is used to multiply the input matrices `A` and `B`. The shapes of the input tensors are `[N, M]` and `[M, K]`, and the shape of the output tensor is `[N, K]`.
The inputs can have any shape of the form [A, B, ..., N, M], and as long as they are broadcastable, the operation will work.

### Loops and conditionals

```python
#Mandelbrot set
z_re = tf.const(0.0)
z_im = tf.const(0.0)
with tf.loop(128) as k: #or tf.loop(0, 128) for a range loop, or tf.loop(0, 128, 2) for a range loop with step
    z_re_new = z_re*z_re - z_im*z_im + c_re
    z_im_new = 2.0*z_re*z_im + c_im
    z_re.val = z_re_new
    z_im.val = z_im_new
    with tf.if_cond(z_re*z_re + z_im*z_im > 256.0):
        tf.break_loop()
```

Scopes in TensorFrost are implemented through python context managers. There are `tf.loop` and `tf.if_cond` context managers that can be used to create loops and conditionals. The loop context manager takes the number of iterations as an argument, and the if_cond context manager takes a condition as an argument. The condition can be any tensor operation that returns a boolean tensor.
Also since the setting operation can not be overloaded in python, the `set` method must be used to update the tensor data outside of this scope, or alternatively the `val` property can be used to set the value of the tensor. 

```python
z_re = tf.const(0.0)
with tf.loop(128):
    z_re.set(z_re_new) #this is fine
    z_re.val = z_re_new #this is also fine
    z_re = z_re_new #this is not fine
```

Just setting the tensor to a new value will actually create a new tensor on top of the old one, and the old one will not be updated.

Loops and conditionals can be stacked and nested. Usually they are compiled into a single kernel with the scopes inside it, but they can be compiled into separate kernels if the data dependencies are not local (look at the QR decomposition example in the examples folder). Not all possible loop and conditional can be valid here, if the loop iteration count has a shape incompatible with the shapes of the tensors in the loop body, the program will not compile correctly.

PS: You can also provide a function instead of using a context manager, but it is not recommended, as it is harder to read and understand.

```python
def loop_body(k):
    z_re_new = z_re*z_re - z_im*z_im + c_re
    z_im_new = 2.0*z_re*z_im + c_im
    z_re.val = z_re_new
    z_im.val = z_im_new
    with tf.if_cond(z_re*z_re + z_im*z_im > 256.0):
        tf.break_loop()

tf.loop(0, 128, 1, loop_body)
```

### GUI and visualization

TensorFrost has simple bindings for the GLFW window library, and some ImGui bindings for GUI. You can render tensors as images (only [-1, -1, 3] float32 tensors for now) and display them in a window. You can also use ImGui to create simple GUIs for your programs. Do note that this only works in the OpenGL backend.

```python

#at this moment you can only open one window
tf.show_window(1280, 720, "a window")

while not tf.window_should_close(): #window will close if you press the close button and this will return True
    mx, my = tf.get_mouse_position()
    wx, wy = tf.get_window_size()

    #simple input example
    if tf.is_mouse_button_pressed(tf.MOUSE_BUTTON_0):
        tf.imgui_text("Mouse button 0 is pressed")

    if tf.is_key_pressed(tf.KEY_W):
        tf.imgui_text("W is pressed")

    #ImGui example
    tf.imgui_begin("an imgui window")
    tf.imgui_text("some text")
    value = tf.imgui_slider("slider", value, 0.0, 10.0)
    if(tf.imgui_button("a button")):
        print("button pressed")
    tf.imgui_end()

    #exectute a tensorfrost program that outputs a [-1, -1, 3] float32 tensor
    img, = render_image(...)

    #display the image (will be stretched to the window size with nearest neighbor interpolation)
    tf.render_frame(img)

```

### Autodifferentiation

Currently only backward mode autodifferentiation is supported, with the exception of scoped operations (loops, conditionals, etc.).

```python
y_pred = x @ W + b
loss = tf.mean((y - y_pred)**2)
dW = tf.grad(loss, W)
db = tf.grad(loss, b)
```
In this example, the `grad` function is used to compute the gradients of the loss with respect to the weights `W` and the bias `b`. If the gradient is taken from the same "loss" tensor, the compiler will still only do one backward pass. At the moment doing gradients from gradients might not work correctly.

Additionally, if the loss is not a scalar, the initial gradient tensor will be assumed to be the same shape as the loss tensor and equal to 1.0. For most cases this is quite useful, as you can compute the gradients of multiple outputs at the same time, as long as they are not dependent on each other. Like doing a gradient of a potential for N particles at the same time.

```python
dx = x1 - x2
dist = tf.sqrt(tf.sum(dx**2))
pot = 1.0 / dist
force = - tf.grad(pot, dx)
```

In this example, the `grad` function is used to compute the gradient of the potential with respect to the distance between two particles. The force is then computed as the negative gradient of the potential with respect to the distance.

Giving a custom gradient tensor is not supported yet, but it is planned for the future.

Additionally you can either stop the gradient computation for some tensors by `tensor.detach_grad()`. In that case the autograd algorithm will stop at this tensor.

Or if you want to force the gradient through a operation without applying the operation gradient you can do `tensor.pass_grad()`. This is useful for example when you want to optimize discrete parameters like a quantized weight.
### Modules 

TensorFrost has a simple module system similar to PyTorch, where you can define a module with trainable parameters and a forward function that computes the output of the module as well as a loss function. 

```python
class SmolNet(tf.Module):
    def __init__(self):
        #specify a custom random scale and offset for the weights when initializing
        self.W = tf.Parameter([16, -1], tf.float32, random_scale=0.01, random_offset=0.0)
        #dont compute gradients for the bias
        self.b = tf.Parameter([-1], tf.float32, requires_grad=False)
        
    def assert_parameters(self):
        #makes sure that the compiler knows that b has shape compatible with W
        self.b = tf.assert_tensor(self.b, [self.W.shape[1]], tf.float32)
        
    def forward(self, x):
        return x @ self.W + self.b
    
    def loss(self, x, y):
        y_pred = self.forward(x, y)
        return tf.mean((y - y_pred)**2)
```

When initializing the module you can add 3 types of TensorFrost accessible parameters:
- `tf.Parameter` - a tensor that will be passed to the TensorProgram as an argument, can be trained
- `tf.ParameterArray` - a dynamic list of parameters, all of them will be passed to the TensorProgram as arguments, can be trained
- `tf.Module` - another module, all of its parameters will be passed to the TensorProgram as arguments, can be trained

The shape argument of the parameter can be a list of integers, where -1 means that the shape is not specified yet, and will be inferred from the input tensor. If you need to compute an operation over several tensors of unspecified shape, you need to assert the shapes in the `assert_parameters` function.
`random_scale` and `random_offset` are used to initialize the weights with random values, and are optional, by default the weights are initialized with Xavier initialization for normal random values.
`requires_grad` is used to specify if the parameter should be trained or not, by default all parameters are trainable. This argument does not stop you from computing `tf.grad` manually, it is just used to specify if the parameter should be updated by the optimizer module.

By itself the module does not do anything, you need to do a second initialization step to either use it inside a TensorProgram, or initialize it as a container for the tensors outside of the program.

```python

def ComputeForward():
    model = SmolNet()
    #creates tf.input tensors from all the parameters of the module
    model.initialize_input()
    X = tf.input([-1, -1], tf.float32)
    return model.forward(X)

forward = tf.compile(ComputeForward)

model_container = SmolNet()
#creates tf.tensor tensors from all the parameters of the module and initializes them
model_container.initialize_parameters()
#you can change them afterwards too
model_container.W = tf.tensor(np.zeros([16, 100], dtype=np.float32))

X = tf.tensor(np.zeros([100, 100], dtype=np.float32))
#the module is passed as an argument to the compiled function, in the same order as they are created in the function
Y = forward(model_container, X)
```

`model.initialize_input()` creates put `tf.input()` tensors for all the parameters of the module. Afterwards `assert_parameters` is automatically called for this and all child modules. This is useful if you want to use the module inside a TensorProgram, as you can just pass the module as an argument to the compiled function, and all the parameters will be automatically created and the shapes will be asserted.
`model.initialize_parameters()` creates `tf.tensor()` tensors for all the parameters of the module and initializes them with random values. This is useful if you want to use the module outside of a TensorProgram, as you can just pass the module as an argument to the compiled function.

You can not, however, do both at the same time, as the module will not know if it is used inside or outside of a TensorProgram.

### Optimizer modules

TensorFrost has a set of built-in optimizer modules that can be used to train the parameters of the module. 
- `tf.optimizers.sgd` - Stochastic Gradient Descent, has a `learning_rate` and `grad_clip` parameters, default values are 0.001 and 0.0 respectively.
- `tf.optimizers.adam` - Adam optimizer, has a `learning_rate`, `beta1`, `beta2` and `grad_clip` parameters, default values are 0.001, 0.9, 0.999 and 0.0 respectively.
- `tf.optimizers.rmsprop` - RMSProp optimizer, has a `learning_rate`, `decay` and `grad_clip` parameters, default values are 0.001, 0.9 and 0.0 respectively.

All optimizer modules are initialized with the module as the first argument, and the training hyperparameters as the rest of the arguments.

```python
def OptimizerStep():
    X = tf.input([-1, -1], tf.float32)
    Y = tf.input([-1, 10], tf.float32)

    model = SmolNet()
    opt = tf.optimizers.adam(model, learning_rate=0.001, beta1=0.9, beta2=0.999)
    opt.initialize_input()
    
    #do a single step of the optimizer (automatically computes gradients and updates the parameters)
    L = opt.step(X, Y) 
    #or 
    #L = model.loss(X, Y)
    #opt.step(L)

    params = opt.parameters()
    params.append(L)
    return params

step = tf.compile(OptimizerStep)

model_container = SmolNet()
opt = tf.optimizers.adam(model_container)
opt.initialize_parameters()

X = tf.tensor(np.zeros([100, 100], dtype=np.float32))
Y = tf.tensor(np.zeros([100, 10], dtype=np.float32))
out = step(X, Y, opt)
opt.update_parameters(res[:-1])
loss = res[-1].numpy[0]
```

Outputting the optimizer state is somewhat inconvenient at the moment, as you can only output a list of tensors from the compiled function, so you need to append the loss to the list of parameters and then extract it from the list afterwards. The optimizer state is not saved in the module, so you need to pass it as an argument to the compiled function, and then update the parameters of the module with the updated parameters from the optimizer.

### Debugging

For debugging convenience there are 2 function types that you can call inside a tensor program:

```python
tf.renderdoc_start_capture()
tf.renderdoc_end_capture()
```

These functions will start and end a RenderDoc capture, only if python is started from the RenderDoc GUI. This is useful for debugging the OpenGL backend, as it allows you to inspect compiled kernel execution, its code and buffers.

```python
tf.region_begin('Region name')
tf.region_end('Region name')
```

When debugging from RenderDoc (or any other OpenGL debugger), these functions will create a region in the RenderDoc capture, which can be useful for profiling and seeing what parts of the program are slow.
The placement of these functions might not reflect their position in the code, as the code is heavily optimized and fused, so if you placed a region in the middle of a generated kernel, it will be placed at the beginning or end of the kernel. Placing them in a scoped operation might make the compilation fail or unfuse kernels, so be careful with that.

### Usage tips

- Using an explicit shape for the input tensors can help the compiler to optimize the program better, as it can infer the shapes of the tensors in the program better. On top of that some optimizations like loop unrolls or staged reductions only happen if the shape is known at compile time.
- Large matrix multiplications are currently very much not optimized, as the compiler does not use groupshared memory or any other optimizations for matrix multiplication. This is planned for the future. For now using TensorFrost mostly makes sense for small to medium sized architectures where cache hits are high.
- Complex operations like convolutions can be implemented through sum + indexing operaitons, example below (taken from [here](https://github.com/MichaelMoroz/TensorFrost/blob/main/examples/ML/module.py))

  While this might seem less optimal than a hand optimized convolution kernel especially when computing its gradient, but it is much more flexible and is actually optimized quite well by the compiler. While the gradient of the indexing operations is an atomicAdd operation, in this case, several of the dimensions of the gradient kernel are not used in the index of the tensors, and get unrolled into sums removing the atomics from the kernel.
  In such a way you can implement any operation you want, even matrix multiplication works fine (`tf.sum(A[i, k] * B[k, j])`), and the compiler will optimize it and its gradient quite well.
  Not all atomics will get optimized out however, so be careful when taking gradients of indexed tensors, as the current atomicAdd for floats is an emulated operation and is can get extremely slow with high write contention.
```python
def conv2d(self, X, W, b):
        bi, wi, hi, cout, cin, it = tf.indices([X.shape[0], X.shape[1] - W.shape[2] + 1, X.shape[2] - W.shape[3] + 1, W.shape[0], W.shape[1], W.shape[2] * W.shape[3]])
        i, j = it%W.shape[2], it/W.shape[2]
        conv = tf.sum(tf.sum(X[bi, wi + i, hi + j, cin] * W[cout, cin, i, j]))
        return conv + b 
```

- Inplace operation gradients simply don't work, even though it does compile, the gradients are not computed correctly. This is planned to be fixed in the future.
- You can check the compiled code in the Temp folder in `generated_lib_*.cpp` files, it is not very readable, but you can see the operations and the memory allocations, the kernel code is in the same file, only on CPU backend.
## Roadmap 

Core features:
- [x] Basic operations (memory, indexing, arithmetic, etc.)
- [x] Basic kernel fusion and compilation
- [x] Advanced built-in functions (random, special functions, etc.)
- [x] Advanced operations (loops, conditionals, etc.)
- [x] Backward mode autodifferentiation
- [ ] Forward mode autodifferentiation
- [ ] Gradients of control flow operations and gradients from gradients
- [x] Kernel code and execution graph export and editing
- [ ] Advanced data types and quantization
- [ ] Compile from Python AST instead of tracing
- [ ] Advanced IR optimizations
- [ ] Kernel shape and cache optimization
  
Algorithm library:
- [x] Scan, reduction, etc.
- [x] Module system
- [x] Optimizer modules (SGD, Adam, RMSProp)
- [ ] Sorting algorithms
- [x] Matrix operations (matrix multiplication, etc.)
- [ ] Advanced matrix operations (QR, SVD, eigenvalues, etc.)
- [ ] Fast Fourier Transform
- [ ] High-level neural network layers (convolution, etc.)

Platforms:
- [x] Windows
- [x] Linux
- [ ] MacOS

Backends:
- [x] CPU (using user-provided compiler)
- [x] OpenGL (most basic GPU backend, works meh)
- [ ] ISPC (for better CPU utilization)
- [ ] Vulkan
- [ ] CUDA
- [ ] WGPU (for web)

(hopefully im not going to abandon this project before finishing lol)

(upd 1 year in: I now know that it is impossible to finish such a project without spending like 10 years on it huh)

## Contributing
Contributions are welcome! If you want to contribute, please open an issue first to discuss the changes you want to make.
