#pragma once
#ifndef __PROGRESS_WINDOW_H__
#define __PROGRESS_WINDOW_H__

#include <string>

class ProgressWindow
{
public:
	ProgressWindow(const std::string& title, int max_size);
	~ProgressWindow();
	void setProgress(const int progress);
	bool isCancelled()const;
};

#endif
