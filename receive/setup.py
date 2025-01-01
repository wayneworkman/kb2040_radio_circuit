from setuptools import setup, find_packages

setup(
    name="kb2040_radio_circuit",
    version="0.1.0", 
    packages=find_packages(where="src"),
    package_dir={"": "src"},
    python_requires=">=3.7",
    install_requires=[
        "numpy>=1.20.0",
        "sounddevice>=0.4.0",
        "cffi>=1.15.0",
    ],
    setup_requires=[
        "cffi>=1.15.0",
        "setuptools>=45",
        "wheel",
    ],
    cffi_modules=[
        "src/direwolf/build_fsk.py:ffibuilder",
    ],
    include_package_data=True,
)
