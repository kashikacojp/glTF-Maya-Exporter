#include "Material.h"
#include "Texture.h"

#include <memory>

namespace kml
{
    typedef std::map<std::string, int> IntegerMapType;
    typedef std::map<std::string, float> FloatMapType;
    typedef std::map<std::string, std::string> StringMapType;
    typedef std::map<std::string, std::shared_ptr<Texture> > TextureMapType;

    Material::Material()
    {
        name_ = std::string("defaultMaterial");
    }

    void Material::SetInteger(const std::string& key, int val)
    {
        imap[key] = val;
    }

    void Material::SetFloat(const std::string& key, float val)
    {
        fmap[key] = val;
    }

    void Material::SetString(const std::string& key, const std::string& val)
    {
        smap[key] = val;
    }

    void Material::SetTexture(const std::string& key, std::shared_ptr<Texture> tex)
    {
        tmap[key] = tex;
    }

    int Material::GetInteger(const std::string& key) const
    {
        IntegerMapType::const_iterator it = imap.find(key);
        if (it != imap.end())
        {
            return it->second;
        }
        return 0;
    }

    float Material::GetFloat(const std::string& key) const
    {
        FloatMapType::const_iterator it = fmap.find(key);
        if (it != fmap.end())
        {
            return it->second;
        }
        return 0;
    }

    std::string Material::GetString(const std::string& key)const
	{
		StringMapType::const_iterator it = smap.find(key);
		if (it != smap.end())
		{
			return it->second;
		}
		return "";
	}

    std::shared_ptr<Texture> Material::GetTexture(const std::string& key) const
    {
        const std::map<std::string, std::shared_ptr<Texture> >::const_iterator it = tmap.find(key);
        if (it == tmap.end())
        {
            return nullptr;
        }
        else
        {
            return it->second;
        }
    }
    std::vector<std::string> Material::GetTextureKeys() const
    {
        std::vector<std::string> ret;
        for (TextureMapType::const_iterator it = tmap.begin(); it != tmap.end(); it++)
        {
            ret.push_back(it->first);
        }
        return ret;
    }

} // namespace kml
