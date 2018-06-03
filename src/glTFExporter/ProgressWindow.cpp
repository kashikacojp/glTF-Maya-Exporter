#include "ProgressWindow.h"

#include <maya/MProgressWindow.h>

#ifdef _MSC_VER
#pragma warning(disable:4819)
#endif

#ifdef _MSC_VER
#pragma comment( lib, "OpenMayaUI" ) 
#endif

ProgressWindow::ProgressWindow(int max_size)
{
	MProgressWindow::reserve();
	MProgressWindow::setProgressMin(0);
	MProgressWindow::setProgressMax(max_size);
	MProgressWindow::startProgress();
}

ProgressWindow::~ProgressWindow()
{
	MProgressWindow::endProgress();
}

void ProgressWindow::setProgress(const int progress)
{
	MProgressWindow::setProgress(progress);
}

bool ProgressWindow::isCancelled()const
{
	return MProgressWindow::isCancelled();
}