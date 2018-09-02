from conans import ConanFile, CMake, tools


class FunctionoidConan(ConanFile):
    name = "Functionoid"
    version = "1.0.0"
    requires = 'Boost/1.68.0@microblink/stable'
    license = "MIT"
    url = "https://github.com/microblink/functionoid"
    generators = "cmake"
    scm = {
        "type": "git",
        "url": "auto",
        "revision": "auto"
    }
    no_copy_source = True


    def package(self):
        self.copy("include/*.hpp")


    def package_id(self):
        self.info.header_only()

