"""Conan recipe for EnvPool dependencies"""

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import copy
import os


class EnvPoolConan(ConanFile):
    name = "envpool"
    version = "1.0.0"
    description = "High-performance parallel RL environment pool"
    author = "Garena Online Private Limited"
    license = "Apache-2.0"
    url = "https://github.com/sail-sg/envpool"

    # Settings
    settings = "os", "compiler", "build_type", "arch"

    # Options
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "build_tests": [True, False],
        "build_benchmarks": [True, False],
        "build_python": [True, False],
        "build_atari": [True, False],
        "build_mujoco": [True, False],
        "build_vizdoom": [True, False],
        "build_procgen": [True, False],
        "enable_cuda": [True, False],
        "use_modern_impl": [True, False],
    }

    default_options = {
        "shared": False,
        "fPIC": True,
        "build_tests": True,
        "build_benchmarks": True,
        "build_python": True,
        "build_atari": True,
        "build_mujoco": True,
        "build_vizdoom": False,  # Complex build
        "build_procgen": False,  # Complex build
        "enable_cuda": False,
        "use_modern_impl": True,
        # Dependency options
        "glog/*:with_gflags": True,
        "glog/*:with_unwind": False,
        "opencv/*:with_jpeg": "libjpeg-turbo",
        "opencv/*:with_png": True,
        "opencv/*:with_tiff": False,
        "opencv/*:with_openexr": False,
        "opencv/*:with_eigen": False,
        "opencv/*:with_webp": False,
        "opencv/*:with_gtk": False,
        "opencv/*:parallel": False,
        "boost/*:without_test": True,
        "boost/*:without_coroutine": True,
        "boost/*:without_fiber": True,
        "boost/*:without_graph": True,
        "boost/*:without_graph_parallel": True,
        "boost/*:without_iostreams": True,
        "boost/*:without_json": True,
        "boost/*:without_locale": True,
        "boost/*:without_log": True,
        "boost/*:without_math": True,
        "boost/*:without_mpi": True,
        "boost/*:without_nowide": True,
        "boost/*:without_program_options": True,
        "boost/*:without_python": True,
        "boost/*:without_random": True,
        "boost/*:without_regex": True,
        "boost/*:without_serialization": True,
        "boost/*:without_stacktrace": True,
        "boost/*:without_test": True,
        "boost/*:without_timer": True,
        "boost/*:without_type_erasure": True,
        "boost/*:without_wave": True,
    }

    # Exports
    exports_sources = (
        "CMakeLists.txt",
        "cmake/*",
        "envpool/*",
        "third_party/*",
        "LICENSE",
        "README.md",
    )

    def requirements(self):
        """Conan Center dependencies"""
        # Core dependencies
        self.requires("pybind11/2.11.1")
        self.requires("gtest/1.14.0")
        self.requires("glog/0.6.0")
        self.requires("gflags/2.2.2")
        self.requires("abseil/20230802.1")
        self.requires("zlib/1.3")
        self.requires("opencv/4.8.1")
        self.requires("boost/1.83.0")

        # Environment-specific dependencies
        if self.options.build_atari:
            self.requires("sdl/2.28.5")
            # Note: libjpeg-turbo for fast image processing
            self.requires("libjpeg-turbo/3.0.1")

        if self.options.build_benchmarks:
            self.requires("benchmark/1.8.3")

        # Box2D for classic control environments
        self.requires("box2d/2.4.1")

        # Note: MuJoCo, ViZDoom, Procgen, ALE fetched via CMake FetchContent
        # as they're not in Conan Center or have complex builds

    def build_requirements(self):
        """Build-time dependencies"""
        self.tool_requires("cmake/3.27.7")

    def config_options(self):
        """Configure options based on platform"""
        if self.settings.os == "Windows":
            del self.options.fPIC
            # Disable some environments on Windows
            self.options.build_vizdoom = False

    def configure(self):
        """Configure dependencies based on options"""
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        """Use cmake_layout for better organization"""
        cmake_layout(self, src_folder=".")

    def generate(self):
        """Generate CMake integration files"""
        # CMakeToolchain
        tc = CMakeToolchain(self)
        tc.variables["ENVPOOL_BUILD_TESTS"] = self.options.build_tests
        tc.variables["ENVPOOL_BUILD_BENCHMARKS"] = self.options.build_benchmarks
        tc.variables["ENVPOOL_BUILD_PYTHON"] = self.options.build_python
        tc.variables["ENVPOOL_BUILD_ATARI"] = self.options.build_atari
        tc.variables["ENVPOOL_BUILD_MUJOCO"] = self.options.build_mujoco
        tc.variables["ENVPOOL_BUILD_VIZDOOM"] = self.options.build_vizdoom
        tc.variables["ENVPOOL_BUILD_PROCGEN"] = self.options.build_procgen
        tc.variables["ENVPOOL_ENABLE_CUDA"] = self.options.enable_cuda
        tc.variables["ENVPOOL_USE_MODERN_IMPL"] = self.options.use_modern_impl
        tc.generate()

        # CMakeDeps
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        """Build using CMake"""
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

        # Run tests if enabled
        if self.options.build_tests:
            cmake.test()

    def package(self):
        """Package the library"""
        cmake = CMake(self)
        cmake.install()

        # Copy license
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        """Provide package information"""
        # Core library
        self.cpp_info.libs = ["envpool_core"]

        # Include directories
        self.cpp_info.includedirs = ["include"]

        # Defines
        if self.options.use_modern_impl:
            self.cpp_info.defines.append("ENVPOOL_USE_MODERN_IMPL=1")

        # System libs
        if self.settings.os == "Linux":
            self.cpp_info.system_libs.extend(["pthread", "dl", "rt"])
        elif self.settings.os == "Windows":
            self.cpp_info.system_libs.append("ws2_32")

        # CMake config
        self.cpp_info.set_property("cmake_find_mode", "both")
        self.cpp_info.set_property("cmake_file_name", "EnvPool")
        self.cpp_info.set_property("cmake_target_name", "EnvPool::envpool")
        self.cpp_info.set_property("pkg_config_name", "envpool")
