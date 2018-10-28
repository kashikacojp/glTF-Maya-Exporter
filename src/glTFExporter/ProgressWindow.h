#pragma once
#ifndef __PROGRESS_WINDOW_H__
#define __PROGRESS_WINDOW_H__

#include <string>

class ProgressWindow
{
public:
	ProgressWindow(const std::string& title, int max_size);
	~ProgressWindow();
	void SetProgress(const int progress);
    void SetProgressStatus(const std::string& str);
	bool IsCancelled()const;
};

#endif
