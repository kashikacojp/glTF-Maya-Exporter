#include "Options.h"

namespace kml
{
    typedef typename Options::imap_type imap_type;
    typedef typename Options::smap_type smap_type;

    void Options::SetInt(const std::string& key, int val)
    {
        imap_[key] = val;
    }

    int Options::GetInt(const std::string& key, int def) const
    {
        imap_type::const_iterator it = imap_.find(key);
        if (it != imap_.end())
        {
            return it->second;
        }
        return def;
    }

    void Options::SetString(const std::string& key, const std::string& val)
    {
        smap_[key] = val;
    }

    std::string Options::GetString(const std::string& key, const std::string& def) const
    {
        smap_type::const_iterator it = smap_.find(key);
        if (it != smap_.end())
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
            g_GlobalOptions.reset(new Options());
        }
        return g_GlobalOptions;
    }

} // namespace kml
