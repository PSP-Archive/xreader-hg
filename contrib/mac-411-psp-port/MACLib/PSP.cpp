#include <wchar.h>
#include "All.h"
#include "IO.h"

int towlower(int wc)
{
        return (wc >= 'A') && (wc <= 'Z')
                                ? wc + 'a' - 'A'
                                : wc
                                ;
}

int wcscasecmp(const wchar_t *s1, const wchar_t *s2)
{
        wchar_t c1, c2;

        for (; *s1; s1++, s2++) {
                c1 = towlower(*s1);
                c2 = towlower(*s2);
                if (c1 != c2)
                        return ((int)c1 - c2);
        }
        return (-*s2);
}

int wcsncasecmp(const wchar_t *s1, const wchar_t *s2, size_t n)
{
        wchar_t c1, c2;

        if (n == 0)
                return (0);
        for (; *s1; s1++, s2++) {
                c1 = towlower(*s1);
                c2 = towlower(*s2);
                if (c1 != c2)
                        return ((int)c1 - c2);
                if (--n == 0)
                        return (0);
        }
        return (-*s2);
}

int wtoi(const wchar_t *iWStr)
{
	return wcstol(iWStr, NULL, 10);
}
