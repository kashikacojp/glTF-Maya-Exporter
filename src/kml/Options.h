#pragma once
#ifndef _KML_OPTIONS_H
#define _KML_OPTIONS_H

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace kml
{
    class Options
    {
    public:
        typedef std::map<std::string, int> imap_type;
        typedef std::map<std::string, std::string> smap_type;

    public:
        void SetInt(const std::string& key, int val);
        int GetInt(const std::string& key, int def = 0) const;

        void SetString(const std::string& key, const std::string& val);
        std::string GetString(const std::string& key, const std::string& def = "") const;

        void SetInteger(const std::string& key, int val) { SetInt(key, val); }
        int GetInteger(const std::string& key, int def = 0) const { return GetInt(key, def); }

    public:
        static std::shared_ptr<Options> GetGlobalOptions();

    private:
        imap_type imap_;
        smap_type smap_;
    };
} // namespace kml

#endif
