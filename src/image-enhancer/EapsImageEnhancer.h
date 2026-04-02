#pragma once

#include<iostream>
#include <memory>

namespace eap {
	namespace sma {
		namespace Enhancer {

			class EnhancerBase {
			public:
				virtual ~EnhancerBase() {}
				//\ main function 
				//\ IOimage,input and output ptr of image 
				virtual bool Enhance(void* const IOimage) = 0;

			protected:
				virtual bool Run() = 0;
				virtual bool GetEnhancedImage(void* const output) = 0;
				int _width, _height;
			};

			using EnhancerPtr = std::shared_ptr<EnhancerBase>;

			EnhancerPtr CreateDefogEnhancer(int width, int height);

			EnhancerPtr CreateDelowEnhancer(int width, int height);
		}
	}
}
