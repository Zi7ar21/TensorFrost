import TensorFrost as tf
import math
import numpy as np

tf.initialize(tf.opengl)

def log_softmax(X):
    X = X - tf.unsqueeze(tf.max(X))
    return X - tf.log(tf.unsqueeze(tf.sum(tf.exp(X))) + 1e-6)

def leaky_relu(X):
    return tf.select(X > 0.0, X, 0.01 * X)

def mul_bias(X, W):
    ids = tf.indices(list(X.shape[:-1]) + [W.shape[-2]])
    return tf.select(ids[-1] == X.shape[-1], 1.0, X[ids]) @ W

class MNIST_net(tf.Module):
    def __init__(self, input_size = 784, output_size = 10, hidden_size = 128, hidden_layers = 2, is_compiler = False):
        super().__init__()
        self.W = tf.ParameterArray()
        self.layers = hidden_layers + 1
        if is_compiler:
            self.W[0] = tf.Parameter([input_size + 1, -1], tf.float32)
            for i in range(hidden_layers - 1):
                self.W[i + 1] = tf.Parameter([-1, -1], tf.float32)
            self.W[hidden_layers] = tf.Parameter([-1, output_size], tf.float32)
        else:
            self.W[0] = tf.Parameter([input_size + 1, hidden_size], tf.float32)
            for i in range(hidden_layers - 1):
                self.W[i + 1] = tf.Parameter([hidden_size + 1, hidden_size], tf.float32)
            self.W[hidden_layers] = tf.Parameter([hidden_size + 1, output_size], tf.float32)

    def forward(self, X):
        for i in range(self.layers - 1):
            X = leaky_relu(mul_bias(X, self.W[i]))
        return mul_bias(X, self.W[self.layers - 1])

    def loss(self, X, Y):
        Yhat = self.forward(X)
        return tf.mean(tf.mean(-Y * log_softmax(Yhat)))

lr = 0.0001
decay = 0.99

def OptimizerStep():
    model = MNIST_net(is_compiler = True)
    opt = tf.optimizers.adam(model, lr)
    opt.initialize_input()

    X = tf.input([-1, -1], tf.float32)
    Y = tf.input([-1, 10], tf.float32)
    
    info = tf.input([-1], tf.int32)
    offset = info[0]
    batch_size = info[1]

    #TODO: implement slicing instead of this crap
    i, j = tf.indices([batch_size, X.shape[1]])
    Xbatch = X[i + offset, j]
    i, j = tf.indices([batch_size, Y.shape[1]])
    Ybatch = Y[i + offset, j]

    L = opt.step(Xbatch, Ybatch)

    params = opt.parameters()
    params.append(L)
    return params

train_step = tf.compile(OptimizerStep)

def ComputeForward():
    model = MNIST_net(is_compiler = True)
    model.initialize_input()
    X = tf.input([-1, -1], tf.float32)
    return model.forward(X)

compute_forward = tf.compile(ComputeForward)

# Load MNIST data
data = np.load('mnist.npz')

def image_to_vector(X):
    return np.reshape(X, (len(X), -1))         # Flatten: (N x 28 x 28) -> (N x 784)

Xtrain = image_to_vector(data['train_x'])  
Ytrain = np.zeros((Xtrain.shape[0], 10))
Ytrain[np.arange(Xtrain.shape[0]), data['train_y']] = 1.0

Xtest = image_to_vector(data['test_x'])     
Ytest = data['test_y']

batch_size = 1024
epochs = 360
iterations = Xtrain.shape[0] // batch_size

model = MNIST_net()
opt = tf.optimizers.adam(model, lr)
opt.initialize_parameters()

Xtf = tf.tensor(Xtrain)
Ytf = tf.tensor(Ytrain)

def test_accuracy(model, X, Y):
    Yhat = compute_forward(model, X)
    Predict = np.argmax(Yhat.numpy, axis = 1)
    correct_tf = np.sum(Predict == Y)
    return correct_tf * 100.0 / len(Y)

loss_curve = []
accuracy_curve = []
for i in range(epochs):
    avg_loss_tf = 0.0

    #shuffle offsets
    offsets = np.random.permutation(Xtrain.shape[0] // batch_size) * batch_size

    for j in range(iterations):
        res = train_step(opt, Xtf, Ytf, [offsets[j], batch_size])
        opt.update_parameters(res[:-1])
        loss = res[-1].numpy
        avg_loss_tf += loss
        loss_curve.append(loss)

    #accuracy = test_accuracy(model, Xtest, Ytest)
    #accuracy_curve.append(accuracy)
    print("Epoch: ", i, " Tf Loss: ", avg_loss_tf / iterations)

test_accuracy_tf = test_accuracy(model, Xtest, Ytest)
print("Final Tf test accuracy: ", test_accuracy_tf, "%")

accuracy_on_train = test_accuracy(model, Xtrain, data['train_y'])
print("Final Tf train accuracy: ", accuracy_on_train, "%")

# Plot loss history
import matplotlib.pyplot as plt
plt.plot(loss_curve)
plt.xlabel('Iteration')
plt.ylabel('Loss')
plt.show()

# Plot accuracy history
#plt.plot(accuracy_curve)
#plt.xlabel('Epoch')
#plt.ylabel('Accuracy')
#plt.show()
