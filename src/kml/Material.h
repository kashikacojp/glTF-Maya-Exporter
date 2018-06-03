#pragma once
#ifndef _KML_MATERIAL_H_
#define _KML_MATERIAL_H_

#include <map>
#include <vector>

namespace kml
{
	class Material
	{
	public:
		Material();
		void        SetInteger(const std::string& key, int val);
		void        SetFloat (const std::string& key, float val);
		void        SetString(const std::string& key, const std::string& val);
		int         GetInteger(const std::string& key)const;
		float       GetFloat (const std::string& key)const;
		std::string GetString(const std::string& key)const;
	public:
		std::vector<std::string> GetStringKeys()const;
	public:
		float       GetValue (const std::string& key)const
		{
			return GetFloat(key);
		}
		std::string GetTextureName(const std::string& key)const
		{
			return GetString(key);
		}
	protected:
		std::map<std::string, int>         imap;
		std::map<std::string, float>       fmap;
		std::map<std::string, std::string> smap;
	};
}

#endif