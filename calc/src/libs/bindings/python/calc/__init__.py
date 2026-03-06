"""
@file __init__.py
@brief Python wrapper for the Calculator C API using ctypes.

This module exposes a minimal Python interface to the Calculator singleton implemented
in C++. It wraps the C API functions `Calculator_compute` and `Calculator_get_last_error`.

This version keeps everything in __init__.py so that you can import the module as:

```
from calc import Calculator
Calculator.compute("1 + 2 * 3")
```
"""

import ctypes
import pathlib

# ----------------------------------------------------------------------
# Load the shared library
# ----------------------------------------------------------------------
lib_path = pathlib.Path(__file__).parent / "libc_api.so"
lib = ctypes.CDLL(str(lib_path))

# ----------------------------------------------------------------------
# Define C function signatures for ctypes
# ----------------------------------------------------------------------
# bool Calculator_compute(const char *expr, double *result)
lib.Calculator_compute.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_double)]
lib.Calculator_compute.restype = ctypes.c_bool

# const char *Calculator_get_last_error()
lib.Calculator_get_last_error.argtypes = []
lib.Calculator_get_last_error.restype = ctypes.c_char_p

# ----------------------------------------------------------------------
# Python wrapper class
# ----------------------------------------------------------------------
class Calculator:
    """
    @brief Python-friendly interface to the C API Calculator singleton.

    This class provides a single entry point to compute mathematical expressions
    using the underlying C++ Calculator via the C API. Errors are reported as Python
    exceptions.
    """

    @staticmethod
    def compute(expr: str) -> float:
        """
        @brief Compute the result of a mathematical expression.

        Calls the C API function `Calculator_compute`. Raises a ValueError if the
        computation fails (e.g., invalid syntax or imbalanced parentheses).

        @param expr The expression to compute (e.g., "1 + 2 * 3").
        @return The computed result as a float.
        @throws ValueError If the expression is invalid or evaluation fails.
        """
        result = ctypes.c_double()
        success = lib.Calculator_compute(expr.encode("utf-8"), ctypes.byref(result))
        if not success:
            # Retrieve the last error from the C API (returns a c_char_p)
            err = lib.Calculator_get_last_error().decode("utf-8")
            raise ValueError(f"Computation error: {err}")
        return result.value

# ----------------------------------------------------------------------
# Example usage when run as a script
# ----------------------------------------------------------------------
if __name__ == "__main__":
    print("Compute '1 + 2 * 3':", Calculator.compute("1 + 2 * 3"))
    try:
        Calculator.compute("12 + 3)")
    except ValueError as e:
        print("Caught error:", e)
