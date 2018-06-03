#include "Compatibility.h"

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
}
#endif
