#ifndef MAC_UTILS_H
#define MAC_UTILS_H

#include "config.h"

long mac_wcstol(const wchar_t *nptr, wchar_t **endptr, int base);

#ifndef HAVE_WCSCASECMP
#include <wchar.h>

int mac_wcscasecmp(const wchar_t *s1, const wchar_t *s2);
int mac_wcsncasecmp(const wchar_t *s1, const wchar_t *s2, size_t n);

#endif

#endif
