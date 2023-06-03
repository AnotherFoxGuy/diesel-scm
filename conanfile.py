import os
from conan import ConanFile
from conan.tools.files import copy


class Fuel(ConanFile):
    name = "fuel-scm"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    def requirements(self):
        self.requires("qt6keychain/0.14.1@anotherfoxguy/stable")

    def generate(self):
        for dep in self.dependencies.values():
            for f in dep.cpp_info.bindirs:
                self.cp_data(f)
            for f in dep.cpp_info.libdirs:
                self.cp_data(f)

    def cp_data(self, src):
        bindir = os.path.join(self.build_folder, "bin")
        copy(self, "*.dll", src, bindir, False)
        copy(self, "*.so*", src, bindir, False)
