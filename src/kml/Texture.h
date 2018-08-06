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
	
	protected:
		std::string m_textureFilePath;

	};
}

#endif