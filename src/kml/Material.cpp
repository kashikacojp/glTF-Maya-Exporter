#include "Material.h"

namespace kml
{
	typedef std::map<std::string, int>		   IntegerMapType;
	typedef std::map<std::string, float>       ValueMapType;
	typedef std::map<std::string, std::string> TextureMapType;

	Material::Material()
	{

	}

	void        Material::SetInteger(const std::string& key, int val)
	{
		imap[key] = val;
	}

	void        Material::SetFloat(const std::string& key, float val)
	{
		fmap[key] = val;
	}

	void        Material::SetString(const std::string& key, const std::string& val)
	{
		smap[key] = val;
	}

	int         Material::GetInteger(const std::string& key)const
	{
		IntegerMapType::const_iterator it = imap.find(key);
		if (it != imap.end())
		{
			return it->second;
		}
		return 0;
	}

	float       Material::GetFloat(const std::string& key)const
	{
		ValueMapType::const_iterator it = fmap.find(key);
		if (it != fmap.end())
		{
			return it->second;
		}
		return 0;
	}

	std::string Material::GetString(const std::string& key)const
	{
		TextureMapType::const_iterator it = smap.find(key);
		if (it != smap.end())
		{
			return it->second;
		}
		return "";
	}

	std::vector<std::string> Material::GetStringKeys()const
	{
		std::vector<std::string> ret;
		for (TextureMapType::const_iterator it = smap.begin(); it != smap.end(); it++)
		{
			ret.push_back(it->first);
		}
		return ret;
	}

}
