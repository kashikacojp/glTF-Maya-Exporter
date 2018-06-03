#include "Options.h"


namespace kml {
	typedef typename Options::map_type map_type;

	void Options::SetInt(const std::string& key, int val)
	{
		map_[key] = val;
	}

	int Options::GetInt(const std::string& key, int def)const
	{
		map_type::const_iterator it = map_.find(key);
		if (it != map_.end())
		{
			return it->second;
		}
		return def;
	}

	static std::shared_ptr<Options> g_GlobalOptions;
	std::shared_ptr<Options> Options::GetGlobalOptions()
	{
		if (g_GlobalOptions.get() == NULL)
		{
			g_GlobalOptions.reset( new Options() );
		}
		return g_GlobalOptions;
	}

}