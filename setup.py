import os
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import sys
import setuptools

class get_pybind_include(object):
    def __init__(self, user=False):
        self.user = user
    def __str__(self):
        import pybind11
        return pybind11.get_include(self.user)

ext_modules = [
    Extension(
        'core_engine',
        ['cpp_core/Pybind_wrapper.cpp', 'cpp_core/MCTS.cpp', 'cpp_core/games/GoGame.cpp', 'cpp_core/games/GomokuGame.cpp'],
        include_dirs=[
            get_pybind_include(),
            get_pybind_include(user=True),
            'cpp_core'
        ],
        language='c++',
        extra_compile_args=['-std=c++17', '-O3', '-Wall']
    ),
]

setup(
    name='core_engine',
    version='0.0.1',
    author='AlphaGo Zero Implementation',
    description='C++ Core Engine with Pybind11',
    ext_modules=ext_modules,
    setup_requires=['pybind11>=2.12.0'],
    zip_safe=False,
)
