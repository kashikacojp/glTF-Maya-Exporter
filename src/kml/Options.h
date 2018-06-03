#pragma once
#ifndef _KML_OPTIONS_H
#define _KML_OPTIONS_H

#include <map>
#include <string>
#include <memory>
#include <vector>

namespace kml
{
	class Options
	{
	public:
		typedef std::map<std::string, int> map_type;
	public:
		void SetInt(const std::string& key, int val);
		int  GetInt(const std::string& key, int def = 0)const;

		void SetInteger(const std::string& key, int val) { SetInt(key, val); }
		int  GetInteger(const std::string& key, int def = 0)const { return GetInt(key, def); }
	public:
		static std::shared_ptr<Options> GetGlobalOptions();
	private:
		map_type map_;
	};
}

#endif
