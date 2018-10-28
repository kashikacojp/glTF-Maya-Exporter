#include "Compatibility.h"

// for MSC compatibility functions

#ifdef _MSC_VER

namespace std
{
    bool isnan(float x)
    {
        return (bool)::isnan(x);
    }
    bool isnan(double x)
    {
        return (bool)::isnan(x);
    }
} // namespace std
#endif
