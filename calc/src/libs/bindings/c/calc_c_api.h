#pragma once

#ifdef __cplusplus
extern "C" {
#endif

bool Calculator_compute(const char *expr, double *result);
const char *Calculator_get_last_error();

#ifdef __cplusplus
}
#endif
