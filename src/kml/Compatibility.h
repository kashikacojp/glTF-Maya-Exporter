#pragma once
#ifndef _KML_COMPATIBILITY_H_
#define _KML_COMPATIBILITY_H_

#include <cmath>

#ifdef _MSC_VER

namespace std
{
    bool isnan(float x);
    bool isnan(double x);
} // namespace std

#endif
#endif