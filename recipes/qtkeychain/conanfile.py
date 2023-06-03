from conan import ConanFile
from conan.tools.files import get, collect_libs
from conan.tools.cmake import CMakeToolchain, CMake, CMakeDeps, cmake_layout


class QtkeychainConan(ConanFile):
    name = "qt6keychain"
    version = "0.14.1"
    license = "BSD-3"
    author = "Edgar"
    url = "https://github.com/AnotherFoxGuy/fuel-scm"
    description = "Platform-independent Qt API for storing passwords securely"
    settings = "os", "compiler", "build_type", "arch"
    options = {"static": [True, False]}
    default_options = {"static": False}

    def layout(self):
        cmake_layout(self)

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["QTKEYCHAIN_STATIC"] = self.options.static
        tc.variables["BUILD_WITH_QT6"] = "ON"
        tc.variables["BUILD_TEST_APPLICATION"] = "OFF"
        tc.variables["CMAKE_PREFIX_PATH"] = "E:/Qt/6.5.0/msvc2019_64"
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_module_file_name", "Qt6Keychain")
        self.cpp_info.set_property(
            "cmake_module_target_name", "Qt6Keychain::Qt6Keychain"
        )
        self.cpp_info.set_property("cmake_file_name", "Qt6Keychain")
        self.cpp_info.set_property("cmake_target_name", "Qt6Keychain::Qt6Keychain")
        self.cpp_info.libs = collect_libs(self)
