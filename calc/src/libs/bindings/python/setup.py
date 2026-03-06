import pathlib
import setuptools

here = pathlib.Path(__file__).parent

setuptools.setup(
    name="calc",
    version="0.1",
    packages=setuptools.find_packages(),
    package_data={"calc": ["libc_api.so"]},
)
