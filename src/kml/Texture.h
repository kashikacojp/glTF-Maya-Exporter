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
		Texture() {};
		~Texture() {};

		Texture(const Texture& rh) {
			m_textureFilePath = rh.m_textureFilePath;
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