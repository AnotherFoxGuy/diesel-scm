from conans import ConanFile, CMake, tools


class QtkeychainConan(ConanFile):
    name = "qtkeychain"
    version = "0.13.2"
    license = "BSD-3"
    author = "Edgar"
    url = "https://github.com/AnotherFoxGuy/fuel-scm"
    description = "Platform-independent Qt API for storing passwords securely"
    settings = "os", "compiler", "build_type", "arch"
    options = {"static": [True, False]}
    default_options = {"static": False}
    scm = {
        "type": "git",
        "url": "https://github.com/frankosterfeld/qtkeychain.git",
        "revision": "v0.13.2"
    }

    def build(self):
        cmake = CMake(self)
        cmake.definitions["QTKEYCHAIN_STATIC"] = self.options.static
        cmake.definitions["BUILD_TEST_APPLICATION"] = "OFF"
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.names["cmake_find_package"] = "Qt5Keychain"
        self.cpp_info.names["cmake_find_package_multi"] = "Qt5Keychain"
        self.cpp_info.libs = tools.collect_libs(self)
