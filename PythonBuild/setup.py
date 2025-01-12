from setuptools import setup

#load readme
with open("../README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

setup(
    name='TensorFrost',
    version='0.6.0',
    author="Mykhailo Moroz",
    author_email="michael08840884@gmail.com",
    description="Tensor library with automatic kernel fusion",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/MichaelMoroz/TensorFrost",
    classifiers=[
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "License :: OSI Approved :: MIT License",
        "Environment :: Win32 (MS Windows)",
        "Operating System :: POSIX :: Linux",
        "Operating System :: Microsoft :: Windows",
        "Intended Audience :: Developers",
        "Topic :: Scientific/Engineering :: Artificial Intelligence",
        "Topic :: Software Development :: Libraries :: Python Modules",
        "Development Status :: 4 - Beta",
        "Environment :: X11 Applications",
        "Environment :: GPU",
    ],
    python_requires='>=3.7',
    packages=["TensorFrost"],

    # Include pre-compiled extension
    package_data={"TensorFrost": ["*.so", "*.pyd", "*.dll"]},
    has_ext_modules=lambda: True
)
