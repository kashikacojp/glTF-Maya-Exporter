#pragma once
#ifndef __PROGRESS_WINDOW_H__
#define __PROGRESS_WINDOW_H__

class ProgressWindow
{
public:
	ProgressWindow(int max_size);
	~ProgressWindow();
	void setProgress(const int progress);
	bool isCancelled()const;
};

#endif
