#pragma once
#ifndef _KML_TEXTURE_H_
#define _KML_TEXTURE_H_

#include <map>
#include <vector>
#include <string>

namespace kml
{
	class Texture
	{
	public:
		enum FilterType {
			FILTER_NEAREST = 0,
			FILTER_LINEAR
		};

	public:
		Texture():
			m_repeatU(0.0f), m_repeatV(0.0f),
			m_offsetU(0.0f), m_offsetV(0.0f),
			m_wrapU(true), m_wrapV(true),
			m_filter(FILTER_NEAREST),
			m_udimmode(false)
		{};

		~Texture() {};

		Texture(const Texture& rh) {
			m_textureFilePath = rh.m_textureFilePath;
			m_repeatU  = rh.m_repeatU;
			m_repeatV  = rh.m_repeatV;
			m_offsetU  = rh.m_offsetU;
			m_offsetV  = rh.m_offsetV;
			m_wrapU    = rh.m_wrapU;
			m_wrapV    = rh.m_wrapV;
			m_filter   = rh.m_filter;
			m_udimmode = rh.m_udimmode;
		}

		Texture* clone() {
			return new Texture(*this);
		}
		
		void SetFilePath(const std::string& filePath) {
			m_textureFilePath = filePath;
		}

		std::string GetFilePath() const {
			return m_textureFilePath;
		}
	
		void SetRepeat(float repeatU, float repeatV) {
			m_repeatU = repeatU;
			m_repeatV = repeatV;
		}
		float GetRepeatU() const { return m_repeatU; }
		float GetRepeatV() const { return m_repeatV; }

		void SetOffset(float offsetU, float offsetV) {
			m_offsetU = offsetU;
			m_offsetV = offsetV;
		}
		float GetOffsetU() const { return m_offsetU; }
		float GetOffsetV() const { return m_offsetV; }

		void SetWrap(bool wrapU, bool wrapV) {
			m_wrapU = wrapU;
			m_wrapV = wrapV;
		}
		bool GetWrapU() const { return m_wrapU; }
		bool GetWrapV() const { return m_wrapV; }

		void SetFilter(FilterType filter) {
			m_filter = filter;
		}
		FilterType GetFilter() const {
			return m_filter;
		}

		void SetUDIMMode(bool enable) {
			m_udimmode = enable;
		}
		bool GetUDIMMode() const {
			return m_udimmode;
		}

	protected:
		std::string m_textureFilePath;
		float m_repeatU, m_repeatV;
		float m_offsetU, m_offsetV;
		bool m_wrapU, m_wrapV;
		FilterType m_filter;
		bool m_udimmode;
	};
}

#endif