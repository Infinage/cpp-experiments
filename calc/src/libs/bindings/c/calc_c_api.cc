#include "calc_c_api.h"
#include "calc/calculator.h"
#include <string>

extern "C" {
    static Calc::Calculator calc;
    static thread_local std::string last_error;
    
    const char *Calculator_get_last_error() {
        return last_error.c_str();
    }

    bool Calculator_compute(const char *expr, double *result) {
        last_error.clear();
        auto computeResult = calc.compute(expr);
        if (computeResult) {
            *result = *computeResult; 
            return true;
        } else {
            last_error = computeResult.error();
            return false;
        }
    }
}
