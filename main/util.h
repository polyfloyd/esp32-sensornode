#ifndef _UTIL_H
#define _UTIL_H

#define TRY(e) do {\
    esp_err_t _err = e;\
    if (_err) return _err;\
} while (0)

#endif
