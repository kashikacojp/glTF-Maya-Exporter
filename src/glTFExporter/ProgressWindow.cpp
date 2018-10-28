#include "ProgressWindow.h"

#include <maya/MProgressWindow.h>
#include <maya/MString.h>

#ifdef _MSC_VER
#pragma warning(disable:4819)
#endif

#ifdef _MSC_VER
#pragma comment( lib, "OpenMayaUI" ) 
#endif

ProgressWindow::ProgressWindow(const std::string& title, int max_size)
{
	MProgressWindow::reserve();
    MProgressWindow::setTitle(MString(title.c_str()));
    MProgressWindow::setInterruptable(true);
	MProgressWindow::setProgressMin(0);
	MProgressWindow::setProgressMax(max_size);
    MProgressWindow::setProgressStatus(MString(""));
	MProgressWindow::startProgress();
}

ProgressWindow::~ProgressWindow()
{
	MProgressWindow::endProgress();
}

void ProgressWindow::SetProgress(const int progress)
{
	MProgressWindow::setProgress(progress);
}

void ProgressWindow::SetProgressStatus(const std::string& str)
{
    MProgressWindow::setProgressStatus(MString(str.c_str()));
}

bool ProgressWindow::IsCancelled()const
{
	return MProgressWindow::isCancelled();
}